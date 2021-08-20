/* we may not need this header anymore */
#include "controller.h"
#include "memory_management.h"
#include "request_handler.h"
#include "rbtree_ftns.h"
#include <unistd.h>

static unsigned long stack_guard_gap = 256UL << PAGE_SHIFT;

unsigned long
mn_get_unmapped_area(struct task_struct *tsk, unsigned long addr, unsigned long len,
                     unsigned long pgoff, unsigned long flags, unsigned long *file)
{
    /* Careful about overflows.. */
    if (len > TASK_SIZE)
        return -ENOMEM;

    // Try to use range mapping to physical for anonymous
    addr = mn_arch_get_unmapped_area_topdown(tsk, file, addr, len, pgoff, flags);

    if (IS_ERR_VALUE(addr))
        return addr; //return error

    if (addr > TASK_SIZE - len)
        return -ENOMEM;
    if (offset_in_page(addr))
        return -EINVAL;

    return addr;
}

inline unsigned long
mn_vm_unmapped_area(struct task_struct *tsk, struct vm_unmapped_area_info *info)
{
    if (info->flags & VM_UNMAPPED_AREA_TOPDOWN)
        return mn_unmapped_area_topdown(tsk, info);
    else
        return mn_unmapped_area(tsk, info);
}

unsigned long vm_start_gap(struct vm_area_struct *vma)
{
    unsigned long vm_start = vma->vm_start;

    if (vma->vm_flags & VM_GROWSDOWN)
    {
        vm_start -= stack_guard_gap;
        if (vm_start > vma->vm_start)
            vm_start = 0;
    }
    return vm_start;
}

unsigned long vm_end_gap(struct vm_area_struct *vma)
{
    unsigned long vm_end = vma->vm_end;

    if (vma->vm_flags & VM_GROWSUP)
    {
        vm_end += stack_guard_gap;
        if (vm_end < vma->vm_end)
            vm_end = -PAGE_SIZE;
    }
    return vm_end;
}

// for vm_unmapped_area from linux/mm.h
unsigned long mn_unmapped_area(struct task_struct *tsk,
                               struct vm_unmapped_area_info *info)
{
    /*
	 * We implement the search by looking for an rbtree node that
	 * immediately follows a suitable gap. That is,
	 * - gap_start = vma->vm_prev->vm_end <= info->high_limit - length;
	 * - gap_end   = vma->vm_start        >= info->low_limit  + length;
	 * - gap_end - gap_start >= length
	 */

    struct mm_struct *mm = tsk->mm;
    struct vm_area_struct *vma;
    unsigned long length, low_limit, high_limit, gap_start, gap_end;

    /* Adjust search length to account for worst case alignment overhead */
    length = info->length + info->align_mask;
    if (length < info->length)
        return -ENOMEM;

    /* Adjust search limits by the desired length */
    if (info->high_limit < length)
        return -ENOMEM;
    high_limit = info->high_limit - length;

    if (info->low_limit > high_limit)
        return -ENOMEM;
    low_limit = info->low_limit + length;

    /* Check if rbtree root looks promising */
    if (RB_EMPTY_ROOT(&mm->mm_rb))
        goto check_highest;
    vma = mm->mm_rb.rb_node->vma;
    if (vma->rb_subtree_gap < length)
        goto check_highest;

    while (1)
    {
        /* Visit left subtree if it looks promising */
        gap_end = vm_start_gap(vma);
        if (gap_end >= low_limit && vma->vm_rb.rb_left)
        {
            struct vm_area_struct *left = vma->vm_rb.rb_left->vma;
            if (left->rb_subtree_gap >= length)
            {
                vma = left;
                continue;
            }
        }

        gap_start = vma->vm_prev ? vm_end_gap(vma->vm_prev) : 0;
    check_current:
        /* Check if current node has a suitable gap */
        if (gap_start > high_limit)
            return -ENOMEM;
        if (gap_end >= low_limit &&
            gap_end > gap_start && gap_end - gap_start >= length)
            goto found;

        /* Visit right subtree if it looks promising */
        if (vma->vm_rb.rb_right)
        {
            struct vm_area_struct *right = vma->vm_rb.rb_right->vma;
            if (right->rb_subtree_gap >= length)
            {
                vma = right;
                continue;
            }
        }

        /* Go back up the rbtree to find next candidate node */
        while (1)
        {
            struct rb_node *prev = &vma->vm_rb;
            if (!rb_parent(prev))
                goto check_highest;

            vma = rb_parent(prev)->vma;
            if (prev == vma->vm_rb.rb_left)
            {
                gap_start = vm_end_gap(vma->vm_prev);
                gap_end = vm_start_gap(vma);
                goto check_current;
            }
        }
    }

check_highest:
    /* Check highest gap, which does not precede any rbtree node */
    gap_start = mm->highest_vm_end;
    gap_end = ULONG_MAX; /* Only for VM_BUG_ON below */
    if (gap_start > high_limit)
        return -ENOMEM;

found:
    /* We found a suitable gap. Clip it with the original low_limit. */
    if (gap_start < info->low_limit)
        gap_start = info->low_limit;

    /* Adjust gap address to the desired alignment */
    gap_start += (info->align_offset - gap_start) & info->align_mask;
    return gap_start;
}

unsigned long mn_unmapped_area_topdown(struct task_struct *tsk,
                                       struct vm_unmapped_area_info *info)
{
    struct mm_struct *mm = tsk->mm;
    struct vm_area_struct *vma;
    unsigned long length, low_limit, high_limit, gap_start, gap_end;

    /* Adjust search length to account for worst case alignment overhead */
    length = info->length + info->align_mask;
    if (length < info->length)
        return -ENOMEM;

    /*
	 * Adjust search limits by the desired length.
	 * See implementation comment at top of unmapped_area().
	 */
    gap_end = info->high_limit;
    if (gap_end < length)
        return -ENOMEM;
    high_limit = gap_end - length;

    if (info->low_limit > high_limit)
        return -ENOMEM;
    low_limit = info->low_limit + length;

    /* Check highest gap, which does not precede any rbtree node */
    gap_start = mm->highest_vm_end;
    if (gap_start <= high_limit)
        goto found_highest;

    /* Check if rbtree root looks promising */
    if (RB_EMPTY_ROOT(&mm->mm_rb))
        return -ENOMEM;

    vma = mm->mm_rb.rb_node->vma;
    if (vma->rb_subtree_gap < length)
        return -ENOMEM;

    while (1)
    {
        /* Visit right subtree if it looks promising */
        gap_start = vma->vm_prev ? vm_end_gap(vma->vm_prev) : 0;
        if (gap_start <= high_limit && vma->vm_rb.rb_right)
        {
            struct vm_area_struct *right = vma->vm_rb.rb_right->vma;
            if (right->rb_subtree_gap >= length)
            {
                vma = right;
                continue;
            }
        }

    check_current:
        /* Check if current node has a suitable gap */
        gap_end = vm_start_gap(vma);
        if (gap_end < low_limit)
            return -ENOMEM;

        if (gap_start <= high_limit &&
            gap_end > gap_start && gap_end - gap_start >= length)
            goto found;

        /* Visit left subtree if it looks promising */
        if (vma->vm_rb.rb_left)
        {
            struct vm_area_struct *left = vma->vm_rb.rb_left->vma;
            if (left->rb_subtree_gap >= length)
            {
                vma = left;
                continue;
            }
        }

        /* Go back up the rbtree to find next candidate node */
        while (1)
        {
            struct rb_node *prev = &vma->vm_rb;
            if (!rb_parent(prev))
                return -ENOMEM;
            vma = rb_parent(prev)->vma;
            if (prev == vma->vm_rb.rb_right)
            {
                gap_start = vma->vm_prev ? vm_end_gap(vma->vm_prev) : 0;
                goto check_current;
            }
        }
    }

found:
    /* We found a suitable gap. Clip it with the original high_limit. */
    if (gap_end > info->high_limit)
        gap_end = info->high_limit;

found_highest:
    /* Compute highest gap address at the desired alignment */
    gap_end -= info->length;
    gap_end -= (gap_end - info->align_offset) & info->align_mask;

    VM_BUG_ON(gap_end < info->low_limit);
    VM_BUG_ON(gap_end < gap_start);
    return gap_end;
}

unsigned long
mn_arch_get_unmapped_area(struct task_struct *tsk, unsigned long *filp,
                          unsigned long addr, unsigned long len, unsigned long pgoff,
                          unsigned long flags)
{
    struct mm_struct *mm = tsk->mm;
    struct vm_area_struct *vma;
    struct vm_unmapped_area_info info;
    unsigned long begin, end;
    (void)filp;

    if (IS_ERR_VALUE(addr))
        return addr;

    if (flags & MAP_FIXED)
        return addr;

    begin = mm->mmap_legacy_base;
    end = TASK_SIZE;

    if (len > end)
        return -ENOMEM;

    if (addr)
    {
        addr = PAGE_ALIGN(addr);
        vma = mn_find_vma(mm, addr);
        if (end - len >= addr &&
            (!vma || addr + len <= vm_start_gap(vma))) // linux/mm.h
            return addr;
    }

    info.flags = 0;
    info.length = len;
    info.low_limit = begin;
    info.high_limit = end;
    info.align_mask = 0;
    info.align_offset = pgoff << PAGE_SHIFT;

    return mn_vm_unmapped_area(tsk, &info); //linux/mm.h
}

static int mn_mmap_address_hint_valid(unsigned long addr, unsigned long len)
{
    if (TASK_SIZE - len < addr)
        return 0;

    return (addr > DEFAULT_MAP_WINDOW) == (addr + len > DEFAULT_MAP_WINDOW);
}

unsigned long
mn_arch_get_unmapped_area_topdown(struct task_struct *tsk, unsigned long *filp,
                                  const unsigned long addr0, const unsigned long len, const unsigned long pgoff,
                                  const unsigned long flags)
{
    struct vm_area_struct *vma;
    struct mm_struct *mm = tsk->mm;
    unsigned long addr = addr0;
    struct vm_unmapped_area_info info;

    if (IS_ERR_VALUE(addr))
        return addr;

    /* Requested length too big for entire address space */
    if (len > TASK_SIZE)
        return -ENOMEM;

    /* No address checking. See comment at mmap_address_hint_valid() */
    if (flags & MAP_FIXED)
        return addr;

    /* Requesting a specific address */
    if (addr)
    {
        addr &= PAGE_MASK;
        if (!mn_mmap_address_hint_valid(addr, len))
            goto get_unmapped_area;

        vma = mn_find_vma(mm, addr);
        if (!vma || addr + len <= vm_start_gap(vma)) // linux/mm.h
        {
            return addr;
        }
    }
get_unmapped_area:

    info.flags = VM_UNMAPPED_AREA_TOPDOWN;
    info.length = len;
    info.low_limit = PAGE_SIZE;
    info.high_limit = mm->mmap_base;

    /*
	 * If hint address is above DEFAULT_MAP_WINDOW, look for unmapped area
	 * in the full address space.
	 *
	 * !in_compat_syscall() check to avoid high addresses for x32.
	 */
    if (addr > DEFAULT_MAP_WINDOW) // arch/x86/include/asm/processor.h
        info.high_limit += TASK_SIZE_MAX - DEFAULT_MAP_WINDOW;

    info.align_mask = 0;
    info.align_offset = pgoff << PAGE_SHIFT;
    addr = mn_vm_unmapped_area(tsk, &info);
    if (!(addr & ~PAGE_MASK))
        return addr;
    VM_BUG_ON(addr != (unsigned long)(-ENOMEM));

    /*
	 * A failed mmap() very likely causes application failure,
	 * so fall back to the bottom-up function here. This scenario
	 * can happen with large stack limits and large mmap()
	 * allocations.
	 */
    return mn_arch_get_unmapped_area(tsk, filp, addr0, len, pgoff, flags);
}

/* 
 * Stack expansion related functions and variables
 */
extern int may_expand_vm(struct mm_struct *mm, vm_flags_t flags, unsigned long npages);
