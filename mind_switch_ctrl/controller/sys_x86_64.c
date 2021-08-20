#include "memory_management.h"
#include "rbtree_ftns.h"
#include <linux/compat.h>

unsigned long
mn_arch_get_unmapped_area(struct task_struct *tsk, unsigned long *filp,
						  unsigned long addr, unsigned long len, unsigned long pgoff,
						  unsigned long flags)
{
	struct mm_struct *mm = tsk->mm;
	struct vm_area_struct *vma;
	struct vm_unmapped_area_info info;
	unsigned long begin, end;

	if (IS_ERR_VALUE(addr))
		return addr;

	if (flags & MAP_FIXED)
		return addr;

	begin = mm->mmap_legacy_base;
	end = TASK_SIZE; // ALWAYS 64-bit

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
	return mn_vm_unmapped_area(tsk, &info); // linux/mm.h
}

static bool mn_mmap_address_hint_valid(unsigned long addr, unsigned long len)
{
	if (TASK_SIZE - len < addr)
		return false;

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
	 */
	if (addr > DEFAULT_MAP_WINDOW) // arch/x86/include/asm/processor.h
		info.high_limit += TASK_SIZE_MAX - DEFAULT_MAP_WINDOW;

	info.align_mask = 0;
	info.align_offset = pgoff << PAGE_SHIFT;
	addr = mn_vm_unmapped_area(tsk, &info);
	if (!(addr & ~PAGE_MASK))
		return addr;
	VM_BUG_ON(addr != -ENOMEM);

	/*
	 * A failed mmap() very likely causes application failure,
	 * so fall back to the bottom-up function here. This scenario
	 * can happen with large stack limits and large mmap()
	 * allocations.
	 */
	return mn_arch_get_unmapped_area(tsk, filp, addr0, len, pgoff, flags);
}
