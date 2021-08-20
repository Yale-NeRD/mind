#include "controller.h"
#include "rbtree_ftns.h"
#include "memory_management.h"
#include "./include/disagg/config.h"

/*
 *	VMA tree-related functions
 */

#define validate_mm(mm) do { } while (0)
#define swap(a, b)             \
	do                         \
	{                          \
		typeof(a) __tmp = (a); \
		(a) = (b);             \
		(b) = __tmp;           \
	} while (0)

static unsigned long vma_compute_subtree_gap(struct vm_area_struct *vma)
{
	unsigned long max, prev_end, subtree_gap;

	/*
	 * Note: in the rare case of a VM_GROWSDOWN above a VM_GROWSUP, we
	 * allow two stack_guard_gaps between them here, and when choosing
	 * an unmapped area; whereas when expanding we only require one.
	 * That's a little inconsistent, but keeps the code here simpler.
	 */
	max = vm_start_gap(vma);                    // from linux/mm.h
	if (vma->vm_prev) {
		prev_end = vm_end_gap(vma->vm_prev);    // from linux/mm.h
		if (max > prev_end)
			max -= prev_end;
		else
			max = 0;
	}
	if (vma->vm_rb.rb_left) {
		subtree_gap = vma->vm_rb.rb_left->vma->rb_subtree_gap;
		if (subtree_gap > max)
			max = subtree_gap;
	}
	if (vma->vm_rb.rb_right) {
		subtree_gap = vma->vm_rb.rb_right->vma->rb_subtree_gap;
		if (subtree_gap > max)
			max = subtree_gap;
	}
	return max;
}

static void validate_mm_rb(struct rb_root *root, struct vm_area_struct *ignore)
{
	struct rb_node *nd;

	for (nd = rb_first(root); nd; nd = rb_next(nd)) {
		struct vm_area_struct *vma;
		vma = nd->vma;
		VM_BUG_ON_VMA(vma != ignore &&
			vma->rb_subtree_gap != vma_compute_subtree_gap(vma),
			vma);
	}
}

RB_DECLARE_CALLBACKS(static, vma_gap_callbacks, struct vm_area_struct, vm_rb,
		     unsigned long, rb_subtree_gap, vma_compute_subtree_gap)

/*
 * Update augmented rbtree rb_subtree_gap values after vma->vm_start or
 * vma->vm_prev->vm_end values changed, without modifying the vma's position
 * in the rbtree.
 */
void vma_gap_update(struct vm_area_struct *vma)
{
	/*
	 * As it turns out, RB_DECLARE_CALLBACKS() already created a callback
	 * function that does exacltly what we want.
	 */
	vma_gap_callbacks_propagate(&vma->vm_rb, NULL);
}

static inline void vma_rb_insert(struct vm_area_struct *vma,
				 struct rb_root *root)
{
	/* All rb_subtree_gap values must be consistent prior to insertion */
	validate_mm_rb(root, NULL);
	
	rb_insert_augmented(&vma->vm_rb, root, &vma_gap_callbacks); //linux/rbtree_augment.h
}

void __vma_link_rb(struct mm_struct *mm, struct vm_area_struct *vma,
		struct rb_node **rb_link, struct rb_node *rb_parent)
{
	/* Update tracking information for the gap following the new vma. */
	if (vma->vm_next)
		vma_gap_update(vma->vm_next);
	else
		mm->highest_vm_end = vm_end_gap(vma);    // from linux/mm.h

	/*
	 * vma->vm_prev wasn't known when we followed the rbtree to find the
	 * correct insertion point for that vma. As a result, we could not
	 * update the vma vm_rb parents rb_subtree_gap values on the way down.
	 * So, we first insert the vma with a zero rb_subtree_gap value
	 * (to be consistent with what we did on the way down), and then
	 * immediately update the gap to the correct value. Finally we
	 * rebalance the rbtree after all augmented values have been set.
	 */
    // from linux/rbtree.h
	rb_link_node(&vma->vm_rb, rb_parent, rb_link);	// set parent and put it at link
													// children will be set as NULL
	vma->rb_subtree_gap = 0;
	vma_gap_update(vma);

	vma_rb_insert(vma, &mm->mm_rb);
}

void __vma_link_list(struct mm_struct *mm, struct vm_area_struct *vma,
		struct vm_area_struct *prev, struct rb_node *rb_parent)
{
	struct vm_area_struct *next;
	printf("Insert new VMA: 0x%lx - 0x%lx [prev: 0x%lx, mmap: 0x%lx]\n", 
			vma->vm_start, vma->vm_end, (unsigned long)prev, (unsigned long)mm->mmap);

	vma->vm_prev = prev;
	if (prev) {
		next = prev->vm_next;
		prev->vm_next = vma;
	} else {
		mm->mmap = vma;
		if (rb_parent)
			next = rb_entry(rb_parent,
					struct vm_area_struct, vm_rb);
		else
			next = NULL;
	}
	vma->vm_next = next;
	if (next)
		next->vm_prev = vma;
}

static void
__vma_link(struct mm_struct *mm, struct vm_area_struct *vma,
	struct vm_area_struct *prev, struct rb_node **rb_link,
	struct rb_node *rb_parent)
{
	__vma_link_list(mm, vma, prev, rb_parent);
	__vma_link_rb(mm, vma, rb_link, rb_parent);
}

void vma_link(struct mm_struct *mm, struct vm_area_struct *vma,
			struct vm_area_struct *prev, struct rb_node **rb_link,
			struct rb_node *rb_parent)
{
	__vma_link(mm, vma, prev, rb_link, rb_parent);
	mm->map_count++;
	validate_mm(mm);
}

/* Look up the first VMA which satisfies  addr < vm_end,  NULL if none. */
struct vm_area_struct *mn_find_vma(struct mm_struct *mm, unsigned long addr)
{
	struct rb_node *rb_node;
	struct vm_area_struct *vma = NULL;

	rb_node = mm->mm_rb.rb_node;

	while (rb_node) {
		struct vm_area_struct *tmp;

		tmp = rb_entry(rb_node, struct vm_area_struct, vm_rb);
		// printf("Check RB-Tree: 0x%lx - 0x%lx\n", tmp->vm_start, tmp->vm_end);

		if (tmp->vm_end > addr) {
			vma = tmp;
			if (tmp->vm_start <= addr)
				break;
			rb_node = rb_node->rb_left;
		} else
			rb_node = rb_node->rb_right;
	}

	return vma;
}

/* Look up the first VMA which intersects the interval start_addr..end_addr-1,
   NULL if none.  Assume start_addr < end_addr. */
static inline struct vm_area_struct * mn_find_vma_intersection(
	struct mm_struct * mm, unsigned long start_addr, unsigned long end_addr)
{
	struct vm_area_struct * vma = mn_find_vma(mm,start_addr);

	if (vma && end_addr <= vma->vm_start)
		vma = NULL;
	return vma;
}

int mn_find_vma_links(struct mm_struct *mm, unsigned long addr,
		unsigned long end, struct vm_area_struct **pprev,
		struct rb_node ***rb_link, struct rb_node **rb_parent)
{
	struct rb_node **__rb_link, *__rb_parent, *rb_prev;

	__rb_link = &mm->mm_rb.rb_node;
	rb_prev = __rb_parent = NULL;

	while (*__rb_link) {
		struct vm_area_struct *vma_tmp;

		__rb_parent = *__rb_link;
		vma_tmp = rb_entry(__rb_parent, struct vm_area_struct, vm_rb);

		if (vma_tmp->vm_end > addr) {
			/* Fail if an existing vma overlaps the area */
			if (vma_tmp->vm_start < end)
				return -ENOMEM;
			__rb_link = &__rb_parent->rb_left;
		} else {
			rb_prev = __rb_parent;
			__rb_link = &__rb_parent->rb_right;
		}
	}

	*pprev = NULL;
	if (rb_prev)
		*pprev = rb_entry(rb_prev, struct vm_area_struct, vm_rb);
	*rb_link = __rb_link;
	*rb_parent = __rb_parent;
	return 0;
}

unsigned long count_vma_pages_range(struct mm_struct *mm,
		unsigned long addr, unsigned long end)
{
	unsigned long nr_pages = 0;
	struct vm_area_struct *vma;

	/* Find first overlaping mapping */
	vma = mn_find_vma_intersection(mm, addr, end);
	if (!vma)
		return 0;

	nr_pages = (min(end, vma->vm_end) -
		max(addr, vma->vm_start)) >> PAGE_SHIFT;

	/* Iterate over the rest of the overlaps */
	for (vma = vma->vm_next; vma; vma = vma->vm_next) {
		unsigned long overlap_len;

		if (vma->vm_start > end)
			break;

		overlap_len = min(end, vma->vm_end) - vma->vm_start;
		nr_pages += overlap_len >> PAGE_SHIFT;
	}

	return nr_pages;
}

/*
 * Helper for vma_adjust() in the split_vma insert case: insert a vma into the
 * mm's list and rbtree.  It has already been inserted into the interval tree.
 */
static void __insert_vm_struct(struct mm_struct *mm, struct vm_area_struct *vma)
{
	struct vm_area_struct *prev;
	struct rb_node **rb_link, *rb_parent;

	if (mn_find_vma_links(mm, vma->vm_start, vma->vm_end,
			   &prev, &rb_link, &rb_parent))
		BUG();
	__vma_link(mm, vma, prev, rb_link, rb_parent);
	mm->map_count++;
}

static void __vma_rb_erase(struct vm_area_struct *vma, struct rb_root *root)
{
	/*
	 * Note rb_erase_augmented is a fairly large inline function,
	 * so make sure we instantiate it only once with our desired
	 * augmented rbtree callbacks.
	 */
	rb_erase_augmented(&vma->vm_rb, root, &vma_gap_callbacks);	//linux/rbtree_augmented.h
}

static __always_inline void vma_rb_erase(struct vm_area_struct *vma,
					 struct rb_root *root)
{
	/*
	 * All rb_subtree_gap values must be consistent prior to erase,
	 * with the possible exception of the vma being erased.
	 */
	validate_mm_rb(root, vma);

	__vma_rb_erase(vma, root);
}

static __always_inline void vma_rb_erase_ignore(struct vm_area_struct *vma,
						struct rb_root *root,
						struct vm_area_struct *ignore)
{
	/*
	 * All rb_subtree_gap values must be consistent prior to erase,
	 * with the possible exception of the "next" vma being erased if
	 * next->vm_start was reduced.
	 */
	validate_mm_rb(root, ignore);

	__vma_rb_erase(vma, root);
}

static __always_inline void __vma_unlink_common(struct mm_struct *mm,
						struct vm_area_struct *vma,
						struct vm_area_struct *prev,
						int has_prev,
						struct vm_area_struct *ignore)
{
	struct vm_area_struct *next;

	vma_rb_erase_ignore(vma, &mm->mm_rb, ignore);
	next = vma->vm_next;
	if (has_prev)
		prev->vm_next = next;
	else {
		prev = vma->vm_prev;
		if (prev)
			prev->vm_next = next;
		else
			mm->mmap = next;
	}
	if (next)
		next->vm_prev = prev;
}

static inline void __vma_unlink_prev(struct mm_struct *mm,
				     struct vm_area_struct *vma,
				     struct vm_area_struct *prev)
{
	__vma_unlink_common(mm, vma, prev, 1, vma);
}

/*
 * Close a vm structure and free it, returning the next.
 * NOTE: this function itself will not erase rules of this VMA
 */
static struct vm_area_struct *remove_vma(struct vm_area_struct *vma)
{
	struct vm_area_struct *next = vma->vm_next;

	// clear memory management
	if (vma->vm_private_data)
	{
		struct memory_node_mapping *mnmap = vma->vm_private_data;
		mnmap->mn_stat->alloc_size -= (vma->vm_end - vma->vm_start);
		pr_vma("Remove VMA (0x%lx), node [0x%lx - 0x%lx]: 0x%lx [P: 0x%lx, N: 0x%lx]\n",
			   (unsigned long)vma,
				vma->vm_start, vma->vm_end,
			   (unsigned long)mnmap->node,
			   (unsigned long)mnmap->node->prev,
			   (unsigned long)mnmap->node->next);
		list_delete_node_no_header(mnmap->node);
		mnmap->node = NULL;
		free(vma->vm_private_data);
	}

	free(vma);
	return next;
}

static void _mn_remove_vmas(struct mm_struct *mm, int remove_test)
{

	struct vm_area_struct *vma = mm->mmap;
	struct memory_node_mapping *mnmap;
	int do_not_reset_mmap = 0;
	while (vma)
	{
		if (!remove_test && mm->testing_vma.data_addr 
			&& (vma->vm_end - vma->vm_start == TEST_INIT_ALLOC_SIZE))
		{
			pr_vma("Skip test data vma: 0x%lx - 0x%lx\n", vma->vm_start, vma->vm_end);
			vma = vma->vm_next;
			do_not_reset_mmap = 1;
			continue;
		}
		else if (!remove_test && mm->testing_vma.meta_addr 
			     && (vma->vm_start == mm->testing_vma.meta_addr))
		{
			pr_vma("Skip test meta vma: 0x%lx - 0x%lx\n", vma->vm_start, vma->vm_end);
			vma = vma->vm_next;
			do_not_reset_mmap = 1;
			continue;
		}
		// NOTE: possibly test VMA
		else if (!remove_test && mm->testing_vma.meta_addr && (vma->vm_end - vma->vm_start == TEST_SUB_REGION_ALLOC_SIZE))
		{
			pr_vma("Skip (possibly) test vma: 0x%lx - 0x%lx\n", vma->vm_start, vma->vm_end);
			vma = vma->vm_next;
			do_not_reset_mmap = 1;
			continue;
		}
		else
		{
			long nrpages = vma_pages(vma);
			vm_stat_account(mm, vma->vm_flags, -nrpages);
			pr_vma("Remove VMA: 0x%lx - 0x%lx [prev: 0x%lx, next: 0x%lx]\n", 
					vma->vm_start, vma->vm_end, (unsigned long)vma->vm_prev, (unsigned long)vma->vm_next);
			mnmap = vma->vm_private_data;
			if (mnmap && mnmap->base_addr) // if there is assigned memory
				del_addr_trans_rule(get_full_virtual_address(mm->owner->tgid, vma->vm_start),
									vma->vm_end - vma->vm_start, vma);
			if (vma->vm_prev)
			{
				vma->vm_prev->vm_next = vma->vm_next;
				if (vma->vm_next)
					vma->vm_next->vm_prev = vma->vm_prev;
			}
			vma = remove_vma(vma);
		}
	}
	if(!do_not_reset_mmap)
		mm->mmap = NULL;
}

void mn_remove_vmas(struct mm_struct *mm)
{
	_mn_remove_vmas(mm, 1);
}

void mn_remove_vmas_exec(struct mm_struct *mm)
{
	_mn_remove_vmas(mm, 0);
}

/*
 * Ok - we have the memory areas we should free on the vma list,
 * so release them, and do the vma updates.
 *
 * Called with the mm semaphore held.
 */
void remove_vma_list(struct mm_struct *mm, struct vm_area_struct *vma)
{
	do {
		long nrpages = vma_pages(vma);

		vm_stat_account(mm, vma->vm_flags, -nrpages);
		vma = remove_vma(vma);
	} while (vma);
	validate_mm(mm);
}

/*
 * We cannot adjust vm_start, vm_end, vm_pgoff fields of a vma that
 * is already present in an i_mmap tree without adjusting the tree.
 * The following helper function should be used when such adjustments
 * are necessary.  The "insert" vma (if any) is to be inserted
 * before we drop the necessary locks.
 */
// needed for vma_adjust
int __vma_adjust(struct vm_area_struct *vma, unsigned long start,
	unsigned long end, pgoff_t pgoff, struct vm_area_struct *insert,
	struct vm_area_struct *expand)
{
	struct mm_struct *mm = vma->vm_mm;
	struct vm_area_struct *next;
	unsigned long *file = vma->vm_file;
	int start_changed = 0, end_changed = 0;
	long adjust_next = 0;
	int remove_next = 0;

	// Adjust private data first (independently to the remaining codes)
	if (expand && vma == expand && vma->vm_private_data)	// only expansion cases
	{
		struct memory_node_mapping *mnmap = vma->vm_private_data;
		if (vma->vm_start > start)
		{
			unsigned long change = vma->vm_start - start;
			mnmap->base_addr -= change;
			mnmap->size += change;
			mnmap->mn_stat->alloc_size += change;
		}

		if (vma->vm_end < end)
		{
			unsigned long change = end - vma->vm_end;
			mnmap->size += change;
			mnmap->mn_stat->alloc_size += change;
		}

		pr_rbtree("Merge VMA: [0x%lx - 0x%lx] -> [0x%lx - 0x%lx]\n",
				  vma->vm_start, vma->vm_end, start, end);

		// Always extend previous vma when merging existing two VMAs
		if (vma->vm_next && vma->vm_next->vm_end == end)
		{
			// Erase vma that will be merged to the previous vma
			struct vm_area_struct *tar = vma->vm_next;
			detach_vmas_to_be_unmapped(mm, tar, vma, end);
			remove_vma_list(mm, tar);
		}else{
			//TODO: we should initialize expanded amount of memory if it was not mapped
		}
	}

	next = vma->vm_next;
	if (next && !insert)
	{
		struct vm_area_struct *exporter = NULL, *importer = NULL;

		if (end >= next->vm_end) 
		{	// SHOULD not happen
			fprintf(stderr, "* Expand over next VMA!\n");
			/*
			 * vma expands, overlapping all the next, and
			 * perhaps the one after too (mprotect case 6).
			 * The only other cases that gets here are
			 * case 1, case 7 and case 8.
			 */
			if (next == expand) {
				/*
				 * The only case where we don't expand "vma"
				 * and we expand "next" instead is case 8.
				 */
				VM_WARN_ON(end != next->vm_end);
				/*
				 * remove_next == 3 means we're
				 * removing "vma" and that to do so we
				 * swapped "vma" and "next".
				 */
				remove_next = 3;
				VM_WARN_ON(file != next->vm_file);
				swap(vma, next);
			} else {
				VM_WARN_ON(expand != vma);
				/*
				 * case 1, 6, 7, remove_next == 2 is case 6,
				 * remove_next == 1 is case 1 or 7.
				 */
				remove_next = 1 + (end > next->vm_end);
				VM_WARN_ON(remove_next == 2 &&
					   end != next->vm_next->vm_end);
				VM_WARN_ON(remove_next == 1 &&
					   end != next->vm_end);
				/* trim end to next, for case 6 first pass */
				end = next->vm_end;
			}

			exporter = next;
			importer = vma;
		}
		else if (end > next->vm_start)
		{ // SHOULD not happen
			fprintf(stderr, "* Expand over next VMA!\n");
			/*
			 * vma expands, overlapping part of the next:
			 * mprotect case 5 shifting the boundary up.
			 */
			adjust_next = (end - next->vm_start) >> PAGE_SHIFT;
			exporter = next;
			importer = vma;
			VM_WARN_ON(expand != importer);
		}
		else if (end < vma->vm_end)
		{
			/*
			 * vma shrinks, and !insert tells it's not
			 * split_vma inserting another: so it must be
			 * mprotect case 4 shifting the boundary down.
			 */
			adjust_next = -((vma->vm_end - end) >> PAGE_SHIFT);
			exporter = vma;
			importer = next;
			VM_WARN_ON(expand != importer);
		}
		(void)exporter;
		(void)importer;
	}
again:
	if (start != vma->vm_start) {
		vma->vm_start = start;
		start_changed = 1;
	}
	if (end != vma->vm_end) {
		vma->vm_end = end;
		end_changed = 1;
	}
	vma->vm_pgoff = pgoff;
	if (adjust_next) {
		next->vm_start += adjust_next << PAGE_SHIFT;
		next->vm_pgoff += adjust_next;
	}

	if (remove_next) {
		/*
		 * vma_merge has merged next into vma, and needs
		 * us to remove next before dropping the locks.
		 */
		if (remove_next != 3)
			__vma_unlink_prev(mm, next, vma);
		else
			/*
			 * vma is not before next if they've been
			 * swapped.
			 *
			 * pre-swap() next->vm_start was reduced so
			 * tell validate_mm_rb to ignore pre-swap()
			 * "next" (which is stored in post-swap()
			 * "vma").
			 */
			__vma_unlink_common(mm, next, NULL, 0, vma);
	} else if (insert) {
		/*
		 * split_vma has split insert from vma, and needs
		 * us to insert it before dropping the locks
		 * (it may either follow vma or precede it).
		 */
		__insert_vm_struct(mm, insert);
	} else {
		if (start_changed)
			vma_gap_update(vma);
		if (end_changed) {
			if (!next)
				mm->highest_vm_end = vm_end_gap(vma);
			else if (!adjust_next)
				vma_gap_update(next);
		}
	}

	if (remove_next) {
		mm->map_count--;
		remove_vma(next);
		/*
		 * In mprotect's case 6 (see comments on vma_merge),
		 * we must remove another next too. It would clutter
		 * up the code too much to do both in one go.
		 */
		if (remove_next != 3) {
			/*
			 * If "next" was removed and vma->vm_end was
			 * expanded (up) over it, in turn
			 * "next->vm_prev->vm_end" changed and the
			 * "vma->vm_next" gap must be updated.
			 */
			next = vma->vm_next;
		} else {
			/*
			 * For the scope of the comment "next" and
			 * "vma" considered pre-swap(): if "vma" was
			 * removed, next->vm_start was expanded (down)
			 * over it and the "next" gap must be updated.
			 * Because of the swap() the post-swap() "vma"
			 * actually points to pre-swap() "next"
			 * (post-swap() "next" as opposed is now a
			 * dangling pointer).
			 */
			next = vma;
		}
		if (remove_next == 2) {
			remove_next = 1;
			end = next->vm_end;
			goto again;
		}
		else if (next)
			vma_gap_update(next);
		else {
			/*
			 * If remove_next == 2 we obviously can't
			 * reach this path.
			 *
			 * If remove_next == 3 we can't reach this
			 * path because pre-swap() next is always not
			 * NULL. pre-swap() "next" is not being
			 * removed and its next->vm_end is not altered
			 * (and furthermore "end" already matches
			 * next->vm_end in remove_next == 3).
			 *
			 * We reach this only in the remove_next == 1
			 * case if the "next" vma that was removed was
			 * the highest vma of the mm. However in such
			 * case next->vm_end == "end" and the extended
			 * "vma" has vma->vm_end == next->vm_end so
			 * mm->highest_vm_end doesn't need any update
			 * in remove_next == 1 case.
			 */
			VM_WARN_ON(mm->highest_vm_end != vm_end_gap(vma));	//linux/mm.h
		}
	}
	validate_mm(mm);
	return 0;
}

/*
 * If the vma has a ->close operation then the driver probably needs to release
 * per-vma resources, so we don't attempt to merge those.
 */
static inline int is_mergeable_vma(struct vm_area_struct *vma,
				unsigned long *file, unsigned long vm_flags)
{
	/*
	 * VM_SOFTDIRTY should not prevent from VMA merging, if we
	 * match the flags but dirty bit -- the caller should mark
	 * merged VMA as dirty. If dirty bit won't be excluded from
	 * comparison, we increase pressue on the memory system forcing
	 * the kernel to generate new VMAs when old one could be
	 * extended instead.
	 */
	if ((vma->vm_flags ^ vm_flags) & ~VM_SOFTDIRTY)
		return 0;
	if (vma->vm_file != file)
		return 0;
	return 1;
}

/*
 * Return true if we can merge this (vm_flags,anon_vma,file,vm_pgoff)
 * in front of (at a lower virtual address and file offset than) the vma.
 *
 * We cannot merge two vmas if they have differently assigned (non-NULL)
 * anon_vmas, nor if same anon_vma is assigned but offsets incompatible.
 *
 * We don't check here for the merged mmap wrapping around the end of pagecache
 * indices (16TB on ia32) because do_mmap_pgoff() does not permit mmap's which
 * wrap, nor mmaps which cover the final page at index -1UL.
 */
static int
can_vma_merge_before(struct vm_area_struct *vma, unsigned long vm_flags,
					 unsigned long *file,
					 pgoff_t vm_pgoff, unsigned long size,
					 struct vm_area_struct *new)
{
	(void)vm_pgoff;	//unused

	if (is_mergeable_vma(vma, file, vm_flags) && size <= (DISAGG_VMA_MAX_SIZE / 2))
	{
		struct memory_node_mapping *mnmap = vma->vm_private_data;
		// check vma address can fit power of 2
		if (mnmap && (vma->vm_end - vma->vm_start == size) &&
			(get_next_pow_of_two_addr(vma->vm_start, size * 2) == vma->vm_end))
		{
			struct memory_node_mapping *mnmap_prev;
			if (new)
			{
				// merge with existing VMA: only check continuity
				if (vma->vm_prev == new)
				{
					mnmap_prev = (struct memory_node_mapping *)(new->vm_private_data);
					if (mnmap_prev && (mnmap_prev->base_addr + mnmap_prev->size == mnmap->base_addr))
						return 1;
				}
			}else{
				// check if is is the first one of the list and there is enough space
				if ((mnmap->node->prev == &mnmap->mn_stat->alloc_vma_list) &&
					((mnmap->base_addr - MN_VA_MIN_ADDR) >= size))
						return 1;

				mnmap_prev = (struct memory_node_mapping *)(mnmap->node->prev->data);
				if (mnmap_prev &&
					// check there is enough space after the previous vma
					(mnmap->base_addr - (mnmap_prev->base_addr + mnmap_prev->size) >= size))
					return 1;
			}
		}
	}
	return 0;
}

/*
 * Return true if we can merge this (vm_flags,anon_vma,file,vm_pgoff)
 * beyond (at a higher virtual address and file offset than) the vma.
 *
 * We cannot merge two vmas if they have differently assigned (non-NULL)
 * anon_vmas, nor if same anon_vma is assigned but offsets incompatible.
 * 
 * Here, the continuity bewteen vmas was chekced
 */
static int
can_vma_merge_after(struct vm_area_struct *vma, unsigned long vm_flags,
					unsigned long *file,
					pgoff_t vm_pgoff, unsigned long size,
					struct vm_area_struct *new)
{
	(void)vm_pgoff;	//unused

	if (is_mergeable_vma(vma, file, vm_flags) && size <= (DISAGG_VMA_MAX_SIZE / 2))
	{
		struct memory_node_mapping *mnmap = vma->vm_private_data;
		pr_rbtree("%s:L%d - mnmap 0x%lx\n", __func__, __LINE__, (unsigned long)mnmap);
		// Check vma address can fit power of 2
		if (mnmap && (vma->vm_end - vma->vm_start == size) && 
			(get_next_pow_of_two_addr(vma->vm_end, size * 2) == vma->vm_end + size))
		{
			if (new)
			{
				// Merge with existing VMA: only check continuity
				if (vma->vm_next == new)
				{
					struct memory_node_mapping *mnmap_next = (struct memory_node_mapping *)(new->vm_private_data);
					pr_rbtree("%s:L%d - mnmap_next 0x%lx\n", __func__, __LINE__, (unsigned long)mnmap_next);

					if (mnmap_next && (mnmap_next->base_addr == mnmap->base_addr + mnmap->size))
						return 1;
				}
			}else{
				pr_rbtree("%s:L%d\n", __func__, __LINE__);
				// Check enough space for DMA address
				// Check if is is the last one of the list and there is enough space
				if ((((!mnmap->node->next) || !mnmap->node->next->data) &&
					 (mnmap->mn_stat->node_info->size - (mnmap->base_addr + mnmap->size) >= size)) ||
					// Check there is enough space before the next vma
					((mnmap->node->next) && (mnmap->node->next->data) &&
					 (((struct memory_node_mapping *)(mnmap->node->next->data))->base_addr - (mnmap->base_addr + mnmap->size) >= size)))
					return 1;
			}
		}
	}
	return 0;
}

/*
 * Given a mapping request (addr,end,vm_flags,file,pgoff), figure out
 * whether that can be merged with its predecessor or its successor.
 * Or both (it neatly fills a hole).
 *
 * In most cases - when called for mmap, brk or mremap - [addr,end) is
 * certain not to be mapped by the time vma_merge is called; but when
 * called for mprotect, it is certain to be already mapped (either at
 * an offset within prev, or at the start of next), and the flags of
 * this area are about to be changed to vm_flags - and the no-change
 * case has already been eliminated.
 *
 * The following mprotect cases have to be considered, where AAAA is
 * the area passed down from mprotect_fixup, never extending beyond one
 * vma, PPPPPP is the prev vma specified, and NNNNNN the next vma after:
 *
 *     AAAA             AAAA                AAAA          AAAA
 *    PPPPPPNNNNNN    PPPPPPNNNNNN    PPPPPPNNNNNN    PPPPNNNNXXXX
 *    cannot merge    might become    might become    might become
 *                    PPNNNNNNNNNN    PPPPPPPPPPNN    PPPPPPPPPPPP 6 or
 *    mmap, brk or    case 4 below    case 5 below    PPPPPPPPXXXX 7 or
 *    mremap move:                                    PPPPXXXXXXXX 8
 *        AAAA
 *    PPPP    NNNN    PPPPPPPPPPPP    PPPPPPPPNNNN    PPPPNNNNNNNN
 *    might become    case 1 below    case 2 below    case 3 below
 *
 * It is important for case 8 that the the vma NNNN overlapping the
 * region AAAA is never going to extended over XXXX. Instead XXXX must
 * be extended in region AAAA and NNNN must be removed. This way in
 * all cases where vma_merge succeeds, the moment vma_adjust drops the
 * rmap_locks, the properties of the merged vma will be already
 * correct for the whole merged range. Some of those properties like
 * vm_page_prot/vm_flags may be accessed by rmap_walks and they must
 * be correct for the whole merged range immediately after the
 * rmap_locks are released. Otherwise if XXXX would be removed and
 * NNNN would be extended over the XXXX range, remove_migration_ptes
 * or other rmap walkers (if working on addresses beyond the "end"
 * parameter) may establish ptes with the wrong permissions of NNNN
 * instead of the right permissions of XXXX.
 */
// for the new addr and end, we assume power of two leng of (end - addr)
static struct vm_area_struct *_mn_vma_merge(struct mm_struct *mm,
			struct vm_area_struct *prev, unsigned long addr,
			unsigned long end, unsigned long vm_flags,
			unsigned long *file,
			pgoff_t pgoff)
{
	pgoff_t pglen = (end - addr) >> PAGE_SHIFT;
	struct vm_area_struct *next;	// *area
	int err;

	if (file)	// we will not merge file mappings
		return NULL;

	/*
	 * We later require that vma->vm_flags == vm_flags,
	 * so this tests vma->vm_flags & VM_SPECIAL, too.
	 */
	BUG_ON(vm_flags & VM_SPECIAL);

	if (prev)
		next = prev->vm_next;
	else
		next = mm->mmap;

	/* verify some invariant that must be enforced by the caller */
	BUG_ON(prev && addr <= prev->vm_start);
	BUG_ON(next && end > next->vm_end);
	BUG_ON(addr >= end);

	/*
	 * Can it merge with the predecessor?
	 */
	pr_rbtree("Check merge prev\n");
	if (prev && prev->vm_end == addr &&
			// mpol_equal(vma_policy(prev), policy) &&
			can_vma_merge_after(prev, vm_flags,
					    file, pgoff, (end - addr), NULL)) {
		/*
		 * OK, it can.  Can we now merge in the successor as well?
		 */
		/* cases 2, 5, 7 */
		unsigned prev_size = prev->vm_end - prev->vm_start;
		pr_rbtree("Merge prev\n");
		err = __vma_adjust(prev, prev->vm_start,
						   end, prev->vm_pgoff, NULL, prev);
		if (err)
			return NULL;
		else
		{
			// prev has been extended
			struct memory_node_mapping *mnmap = (struct memory_node_mapping *)prev->vm_private_data;
			pr_rbtree("Merge prev: mnmap 0x%lx\n", (unsigned long)mnmap);

			// Initialize extended region
			zeros_mem_range(get_node_idx(mnmap->node_id, prev_size),
							mnmap->base_addr + prev_size,				// start point of the extended range
							prev->vm_end - prev->vm_start - prev_size); // extended size = current size - previous size
																		// size in get_node_idx() is dummy here

			// Add new one (larger range) and delete previous one (smaller range)
			add_new_addr_trans_rule(prev->vm_start, prev->vm_end - prev->vm_start,
									mnmap, mm->owner->tgid, (unsigned int)(prev->vm_flags & 0xF),
									prev);
			del_addr_trans_rule(get_full_virtual_address(mm->owner->tgid, prev->vm_start),
								prev_size, prev);
		}
		return prev;
	}

	/*
	 * Can this new request be merged in front of next?
	 */
	pr_rbtree("Check merge next\n");
	if (next && end == next->vm_start &&
		// mpol_equal(policy, vma_policy(next)) &&
		can_vma_merge_before(next, vm_flags,
							 file, pgoff + pglen, (end - addr), NULL))
	{
		if (prev && addr < prev->vm_end)	/* case 4 */
			// ASSUME: we do not allow overlapping VMA merge
			return NULL;
		else
		{ /* cases 3, 8 */
			// We are using area = next
			unsigned prev_start = next->vm_start;
			err = __vma_adjust(next, addr, next->vm_end,
							   next->vm_pgoff - pglen, NULL, next);
			if (err)
				return NULL;
			else{
				// next has been extended
				struct memory_node_mapping *mnmap = (struct memory_node_mapping *)next->vm_private_data;
				// add new one (larger range) and delete previous one (smaller range)
				add_new_addr_trans_rule(next->vm_start, next->vm_end - next->vm_start,
										mnmap, mm->owner->tgid, (unsigned int)(next->vm_flags & 0xF),
										next);
				del_addr_trans_rule(get_full_virtual_address(mm->owner->tgid, prev_start),
									next->vm_end - prev_start, next);
			}
			/*
			 * In case 3 area is already equal to next and
			 * this is a noop, but in case 8 "area" has
			 * been removed and next was expanded over it.
			 */
			// area = next;
		}
		return next;
	}
	pr_rbtree("No merge\n");
	return NULL;
}

static void update_bfrt_addr_trans(struct mm_struct *mm,
								   struct vm_area_struct *vma,
								   unsigned long intersection)
{
	// Now next should be removed, we can only use vma
	// Previous: |vma->vm_start - vma->vm_end | next->vm_start - next->vm_end|
	// Adjusted: |vma->vm_start -------- intersection ----------- vma->vm_end|
	struct memory_node_mapping *mnmap = (struct memory_node_mapping *)vma->vm_private_data;
	// add new one (larger range) and delete previous two (smaller ranges)
	add_new_addr_trans_rule(vma->vm_start, vma->vm_end - vma->vm_start,
							mnmap, mm->owner->tgid, (unsigned int)(vma->vm_flags & 0xF),
							vma);
	// Left vma befor merge
	del_addr_trans_rule(get_full_virtual_address(mm->owner->tgid, vma->vm_start),
						intersection - vma->vm_start, vma);
	// Right vma befor merge
	del_addr_trans_rule(get_full_virtual_address(mm->owner->tgid, intersection),
						vma->vm_end - intersection, vma);
}

struct vm_area_struct *
_existing_vma_merge(struct mm_struct *mm,
					struct vm_area_struct *vma)
{
	struct vm_area_struct *prev, *next;
	int err;

	if (vma)
	{
		prev = vma->vm_prev;
		next = vma->vm_next;
	}else
		return NULL;

	/*
	 * Can it merge with the predecessor?
	 */
	if (prev)
	{
		pr_rbtree("EX-Check merge prev [0x%lx - 0x%lx] with vma [0x%lx - 0x%lx]\n",
				  prev->vm_start, prev->vm_end, vma->vm_start, vma->vm_end);
		if (prev->vm_end == vma->vm_start &&
			can_vma_merge_after(prev, vma->vm_flags,
								vma->vm_file, vma->vm_pgoff, 
								(vma->vm_end - vma->vm_start), vma))
		{
			unsigned long intersection = prev->vm_end;
			// Extend prev
			err = __vma_adjust(prev, prev->vm_start, vma->vm_end,
							prev->vm_pgoff, NULL, prev);
			if (err)
				return NULL;
			else
			{
				update_bfrt_addr_trans(mm, prev, intersection);
			}
			return prev;
		}
	}

	/*
	 * Can this new request be merged in front of next?
	 */
	if (next)
	{
		pr_rbtree("EX-Check merge next [0x%lx - 0x%lx] with vma [0x%lx - 0x%lx]\n",
				  next->vm_start, next->vm_end, vma->vm_start, vma->vm_end);
		if (vma->vm_end == next->vm_start &&
			can_vma_merge_before(next, vma->vm_flags,
								vma->vm_file, vma->vm_pgoff, 
								(vma->vm_end - vma->vm_start), vma))
		{
			unsigned long intersection = vma->vm_end;
			// Extend vma
			err = __vma_adjust(vma, vma->vm_start, next->vm_end,
							vma->vm_pgoff, NULL, vma);
			if (err)
				return NULL;
			else
			{
				update_bfrt_addr_trans(mm, vma, intersection);
			}
			return vma;
		}
	}
	pr_rbtree("EX-No merge\n");
	return NULL;
}

struct vm_area_struct *mn_vma_merge(struct mm_struct *mm,
			struct vm_area_struct *prev, unsigned long addr,
			unsigned long end, unsigned long vm_flags,
			unsigned long *file,
			pgoff_t pgoff)
{
	struct vm_area_struct *res;
	res = _mn_vma_merge(mm, prev, addr, end, vm_flags, file, pgoff);
	if (res)
	{
		// Try to merge further
		struct vm_area_struct *merged = res;
		while(merged)
		{
			merged = _existing_vma_merge(mm, merged);
			if (merged)
				res = merged;	// Update res for return
		}
	}
	return res;
}

/*
 * __split_vma() bypasses sysctl_max_map_count checking.  We use this where it
 * has already been checked or doesn't make sense to fail.
 */
static int ___mn_split_vma(struct mm_struct *mm, struct vm_area_struct *vma,
		unsigned long addr, int new_below)
{
	struct vm_area_struct *new;
	int err;
	struct memory_node_mapping *data_left = NULL, *data_right = NULL, *data_original = NULL;
	(void)mm;	//unused
	
	new = malloc(sizeof(*new));
	if (!new)
		return -ENOMEM;
	memset(new, 0, sizeof(*new));

	/* most fields are the same, copy all, and then fixup */
	*new = *vma;
	new->vm_rb.vma = new;
	barrier();

	if (new_below)
	{
		new->vm_end = addr;
	} else {
		new->vm_start = addr;
		new->vm_pgoff += ((addr - vma->vm_start) >> PAGE_SHIFT);
	}

	// Prepare memory space
	if (vma->vm_private_data && (vma->vm_end > addr) && (addr > vma->vm_start))
	{
		data_left = malloc(sizeof(struct memory_node_mapping));
		if (!data_left)
		{
			err = ENOMEM;
			goto out_free_vma;
		}
		memset(data_left, 0, sizeof(struct memory_node_mapping));

		data_right = malloc(sizeof(struct memory_node_mapping));
		if (!data_right)
		{
			free(data_left);
			err = ENOMEM;
			goto out_free_vma;
		}
		memset(data_right, 0, sizeof(struct memory_node_mapping));

		data_original = vma->vm_private_data;
		data_left->mn_stat = data_right->mn_stat = data_original->mn_stat;
		data_left->node_id = data_right->node_id = data_original->node_id;
		data_left->base_addr = data_original->base_addr;
		data_left->size = (addr - vma->vm_start);
		data_right->base_addr = data_original->base_addr + data_left->size;
		data_right->size = (vma->vm_end - addr);

		if (new_below)
		{
			new->vm_private_data = data_left;
			vma->vm_private_data = data_right;
		}
		else
		{
			new->vm_private_data = data_right;
			vma->vm_private_data = data_left;
		}
	}

	if (data_original)
	{
		pr_rbtree("* Before_split VA[0x%lx, 0x%lx] MNMAP[0x%lx, 0x%lx]\n",
				  vma->vm_start, vma->vm_end,
				  (unsigned long)data_original->base_addr,
				  (unsigned long)data_original->base_addr + data_original->size);
	}

	// mm.h but __vma_adjust is needed: directly use it
	if (new_below)
		err = __vma_adjust(vma, addr, vma->vm_end, vma->vm_pgoff +
			((addr - new->vm_start) >> PAGE_SHIFT), new, NULL);
	else
		err = __vma_adjust(vma, vma->vm_start, addr, vma->vm_pgoff, new, NULL);	

	/* Success. */
	if (!err)
	{
		if (data_original)
		{
			// data_original - data_left
			list_insert_after(data_original->node, data_left);
			data_left->node = data_original->node->next;
			pr_rbtree("** split_vma - added node: [0x%lx - 0x%lx]\n",
					  data_left->base_addr, data_left->base_addr + data_left->size);

			// data_original - data_left - data_right
			list_insert_after(data_left->node, data_right);
			data_right->node = data_left->node->next;
			pr_rbtree("** split_vma - added node: [0x%lx - 0x%lx]\n",
					  data_right->base_addr, data_right->base_addr + data_right->size);

			// ... - data_left - data right
			list_delete_node_no_header(data_original->node);
			free(data_original);
		}
		// Make sure that the flags are copies
		new->vm_flags = vma->vm_flags;

		return 0;
	}

	/* Clean everything up if vma_adjust failed. */
	if (data_left)
		free(data_left);
	if (data_right)
		free(data_right);
	vma->vm_private_data = data_original;

 out_free_vma:
	free(new);
	return err;
}

int __mn_split_vma(struct mm_struct *mm, struct vm_area_struct *vma,
				   unsigned long addr, int new_below)
{
	unsigned long mid_addr = (vma->vm_start + vma->vm_end) / 2;
	int ret;

	pr_rbtree("Split VMA [0x%lx - 0x%lx] at 0x%lx\n", vma->vm_start, vma->vm_end, addr);

	// Spliting file VMA is straightforward
	if (vma->vm_file)
	{
		return ___mn_split_vma(mm, vma, addr, new_below);
	}

	// Anonymous VMAs should be divided in power of two sizes
	if (mid_addr == addr)
		return ___mn_split_vma(mm, vma, addr, new_below);
	else if(mid_addr > addr)
	{
		ret = ___mn_split_vma(mm, vma, mid_addr, 0);
		if(ret)
			return ret;
		// Now VMA is left half
		return __mn_split_vma(mm, vma, addr, new_below);
	}else{
		ret = ___mn_split_vma(mm, vma, mid_addr, 1);
		if (ret)
			return ret;
		// Now VMA is right half
		return __mn_split_vma(mm, vma, addr, new_below);
	}
	return -1;	// Should not reach here
}

static int _decompose_vma_for_alignment(struct mm_struct *mm, struct vm_area_struct *vma,
										unsigned long lbound, unsigned long rbound)
{
	unsigned long pow_of_two_size = rbound - lbound;
	unsigned long mid_addr = (lbound + rbound) / 2;

	if (pow_of_two_size < PAGE_SIZE)
	{
		fprintf(stderr, "Err - Decompose: [0x%lx - 0x%lx] lb: 0x%lx, rb: 0x%lx\n",
			   vma->vm_start, vma->vm_end, lbound, rbound);
		return -1;	// Error
	}

	if (lbound == vma->vm_start)
	{
		if (rbound == vma->vm_end)
		{
			pr_rbtree("Decompose: [0x%lx - 0x%lx] lb: 0x%lx, rb: 0x%lx\n",
					  vma->vm_start, vma->vm_end, lbound, rbound);
			return 0;
		}

		// Do we need to split vma->vm_start to mid?
		if (mid_addr < vma->vm_end)
		{
			___mn_split_vma(mm, vma, mid_addr, 1);
			lbound = mid_addr;
		}else{
			rbound = mid_addr;
		}
		return _decompose_vma_for_alignment(mm, vma, lbound, rbound);

	}else if(rbound == vma->vm_end){
		// Do we need to split mid to vma->vm_end?
		if (vma->vm_start < mid_addr)
		{
			___mn_split_vma(mm, vma, mid_addr, 0);
			rbound = mid_addr;
		}else{
			lbound = mid_addr;
		}
		return _decompose_vma_for_alignment(mm, vma, lbound, rbound);
	}
	return 0;
}

int decompose_vma_for_alignment(struct mm_struct *mm, struct vm_area_struct *vma)
{
	unsigned long pow_of_two_size = get_pow_of_two_req_size(vma->vm_end - vma->vm_start);
	unsigned long end_pow_2 = get_next_pow_of_two_addr(vma->vm_end, pow_of_two_size);
	unsigned long start_pow_2 = end_pow_2 - pow_of_two_size;
	unsigned long mid_addr;
	struct vm_area_struct *next;

	// Already aligned?
	if (vma->vm_end == end_pow_2 && vma->vm_start == start_pow_2)
	{
		// printf("Decompose: [0x%lx - 0x%lx] lb: 0x%lx, rb: 0x%lx\n",
		// 	   vma->vm_start, vma->vm_end, start_pow_2, end_pow_2);
		return 0;
	}

	// If not aligned but having power of two size -> double it
	if (vma->vm_start < start_pow_2)
	{
		pow_of_two_size *= 2;
		start_pow_2 = end_pow_2 - pow_of_two_size;
	}
	mid_addr = (start_pow_2 + end_pow_2) / 2;

	// Divide them in the middle
	___mn_split_vma(mm, vma, mid_addr, 0);
	next = vma->vm_next;

	// Left half
	if (_decompose_vma_for_alignment(mm, vma, start_pow_2, mid_addr))
		return -1;

	// Right half
	if (_decompose_vma_for_alignment(mm, next, mid_addr, end_pow_2))
		return -1;

	return 0;
}

/*
 * Create a list of vma's touched by the unmap, removing them from the mm's
 * vma list as we go..
 */
void detach_vmas_to_be_unmapped(struct mm_struct *mm, struct vm_area_struct *vma,
								struct vm_area_struct *prev, unsigned long end)
{
	struct vm_area_struct **insertion_point;
	struct vm_area_struct *tail_vma = NULL;

	insertion_point = (prev ? &prev->vm_next : &mm->mmap);
	vma->vm_prev = NULL;
	do {
		vma_rb_erase(vma, &mm->mm_rb);
		mm->map_count--;
		tail_vma = vma;
		vma = vma->vm_next;
	} while (vma && vma->vm_start < end);
	*insertion_point = vma;
	if (vma) {
		vma->vm_prev = prev;
		vma_gap_update(vma);
	} else
		mm->highest_vm_end = prev ? vm_end_gap(prev) : 0;
	tail_vma->vm_next = NULL;
}
