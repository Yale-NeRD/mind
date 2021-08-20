/* we may not need this header anymore */
#include "controller.h"
#include "memory_management.h"
#include "request_handler.h"
#include "rbtree_ftns.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>

#define COPY_MM_VALUES(MM, EXR, F) (MM->F = EXR->F)

#define mn_set_up_layout(D_MM, S_MM)                  \
	{                                                 \
		COPY_MM_VALUES(D_MM, S_MM, hiwater_rss);      \
		COPY_MM_VALUES(D_MM, S_MM, hiwater_vm);       \
		COPY_MM_VALUES(D_MM, S_MM, total_vm);         \
		COPY_MM_VALUES(D_MM, S_MM, locked_vm);        \
		COPY_MM_VALUES(D_MM, S_MM, pinned_vm);        \
		COPY_MM_VALUES(D_MM, S_MM, data_vm);          \
		COPY_MM_VALUES(D_MM, S_MM, exec_vm);          \
		COPY_MM_VALUES(D_MM, S_MM, stack_vm);         \
		COPY_MM_VALUES(D_MM, S_MM, def_flags);        \
		COPY_MM_VALUES(D_MM, S_MM, start_code);       \
		COPY_MM_VALUES(D_MM, S_MM, end_code);         \
		COPY_MM_VALUES(D_MM, S_MM, start_data);       \
		COPY_MM_VALUES(D_MM, S_MM, end_data);         \
		COPY_MM_VALUES(D_MM, S_MM, start_brk);        \
		COPY_MM_VALUES(D_MM, S_MM, brk);              \
		COPY_MM_VALUES(D_MM, S_MM, start_stack);      \
		COPY_MM_VALUES(D_MM, S_MM, arg_start);        \
		COPY_MM_VALUES(D_MM, S_MM, arg_end);          \
		COPY_MM_VALUES(D_MM, S_MM, env_start);        \
		COPY_MM_VALUES(D_MM, S_MM, env_end);          \
		COPY_MM_VALUES(D_MM, S_MM, mmap_base);        \
		COPY_MM_VALUES(D_MM, S_MM, mmap_legacy_base); \
	}

/* Initialize empty/zero-ed memory region for VMA */
static int initialize_vma_memory(struct vm_area_struct *vma)
{
	// Check vma (not file mapping, not existing mapping)
	if (vma && !vma->vm_file && !vma->vm_private_data)
	{
		vma->vm_private_data = malloc(sizeof(struct memory_node_mapping));
		if (!vma->vm_private_data)
		{
			printf("Cannot allocate memory for vm_private_data\n");
			return -ENOMEM;
		}
		memset(vma->vm_private_data, 0, sizeof(struct memory_node_mapping));

		if (get_virtual_address(-1, vma->vm_start, vma->vm_end - vma->vm_start,
								vma->vm_mm->owner->tgid, vma->vm_private_data,
								vma->vm_flags & 0xF, vma))
		{
			// If it is failed, then node is not added to the list
			free(vma->vm_private_data);
			vma->vm_private_data = NULL;
			return -ENOMEM;
		}
	}
	return 0;
}

void vma_gap_update(struct vm_area_struct *vma);

/* Create mmap from msg */
int mn_build_mmap_from_exec(struct mm_struct *mm, struct exec_msg_struct *exec_req)
{
	int i;
	struct vm_area_struct *prev, *mgnt, **pprev, *next;
	struct rb_node **rb_link, *rb_parent;
	struct vm_area_struct *old_mmap = mm->mmap;
	int error = 0;

	sem_wait(&mm->mmap_sem);

	// Copy field from exec_req
	mn_set_up_layout(mm, exec_req);

	rb_link = &mm->mm_rb.rb_node;
	rb_parent = NULL;
	prev = NULL;
	pprev = &mm->mmap;

	mm->addr_trans_rule = 0;
	mm->max_addr_trans_rule = 0;
	mm->acc_ctrl_rule = 0;
	mm->max_acc_ctrl_rule = 0;
	
	printf("Copying vma... (%u entries)\n", exec_req->num_vma);
	mgnt = old_mmap;
	mm->mmap = NULL;
	mm->map_count = 0;
	while(mgnt)
	{
		printf("Copying vma... 0x%lx [0x%lx - 0x%lx] next: 0x%lx\n", 
				(unsigned long)mgnt, mgnt->vm_start, mgnt->vm_end, (unsigned long)mgnt->vm_next);
		next = mgnt->vm_next;

		mgnt->vm_next = mgnt->vm_prev = NULL;
		memset(&mgnt->vm_rb, 0, sizeof(mgnt->vm_rb));
		mgnt->vm_mm = mm;
		mgnt->vm_rb.vma = mgnt;

		// Set up linked list
		*pprev = mgnt;
		pprev = &mgnt->vm_next;
		mgnt->vm_prev = prev;
		prev = mgnt;

		// We need to copy rb_tree
		__vma_link_rb(mm, mgnt, rb_link, rb_parent);
		rb_link = &mgnt->vm_rb.rb_right;
		rb_parent = &mgnt->vm_rb;

		mm->map_count++;
		mgnt = next;
	}

	for (i = 0; i < (int)exec_req->num_vma; i++)
	{
		mgnt = malloc(sizeof(*mgnt));
		if (!mgnt)
		{
			error = -ENOMEM; // Cannot allocate memory
			goto mn_build_mmap_error;
		}
		memset(mgnt, 0, sizeof(*mgnt));

		// Copy values
		mgnt->vm_start = (&exec_req->vmainfos)[i].vm_start;
		mgnt->vm_end = (&exec_req->vmainfos)[i].vm_end;
		mgnt->vm_flags = (&exec_req->vmainfos)[i].vm_flags;
		mgnt->vm_pgoff = (&exec_req->vmainfos)[i].vm_pgoff;
		mgnt->vm_private_data = NULL;

		// Flags: we added the last two flags referring LegoOS (might be no impact)
		mgnt->vm_flags &= ~(VM_LOCKED | VM_LOCKONFAULT | VM_UFFD_MISSING | VM_UFFD_WP);

		// Use file pointer as an identifier (no actual allocation)
		mgnt->vm_file = (unsigned long *)((&exec_req->vmainfos)[i].file_id);
		mgnt->vm_next = mgnt->vm_prev = NULL;
		mgnt->vm_mm = mm;
		mgnt->vm_rb.vma = mgnt;

		// Set up linked list
		*pprev = mgnt;
		pprev = &mgnt->vm_next;
		mgnt->vm_prev = prev;
		prev = mgnt;

		// We need to copy rb_tree
		__vma_link_rb(mm, mgnt, rb_link, rb_parent);
		rb_link = &mgnt->vm_rb.rb_right;
		rb_parent = &mgnt->vm_rb;

		mm->map_count++;
	}

	mgnt = mm->mmap;
	while (mgnt)
	{
		// Only for anonymous and not already assigned
		if (mgnt && !mgnt->vm_file && !mgnt->vm_private_data)
		{
			// Check alignment
			if (decompose_vma_for_alignment(mm, mgnt))
			{
				error = -EINVAL; // Cannot allocate memory
				goto mn_build_mmap_error;
			}
			
			// Initialize memory for vma
			if (initialize_vma_memory(mgnt))
			{
				error = -ENOMEM; // Cannot allocate memory
				goto mn_build_mmap_error;
			}
		}
		mgnt = mgnt->vm_next;
	}

	// Try merging VMAs
	mgnt = mm->mmap;
	while (mgnt)
	{
		if (!_existing_vma_merge(mm, mgnt))
		{
			mgnt = mgnt->vm_next;
		}
	}

	// DEBUG_print_vma(mm);
	sem_post(&mm->mmap_sem);
	return 0;

mn_build_mmap_error:
	if (mm->mmap)
		mn_remove_vmas(mm);

	sem_post(&mm->mmap_sem);

	fprintf(stderr, "Cannot initialize VMAs for EXEC\n");
	return error;
}

static int dup_data_vma(struct vm_area_struct *old, struct vm_area_struct *new,
						unsigned long alloc_len, unsigned long copy_len,
						unsigned long old_off, unsigned long new_off)
{
	int res = -1;
	int node_id;
	struct memory_node_mapping *mnmap = (struct memory_node_mapping *)old->vm_private_data;

	if (!new || !new->vm_private_data || !old->vm_private_data)
		return -1;

	node_id = mnmap->node_id;
	if (get_virtual_address(node_id, new->vm_start, alloc_len,
							new->vm_mm->owner->tgid, new->vm_private_data,
							(unsigned int)(new->vm_flags & 0xF), new))
	{
		return -1;
	}

	/*
	* ===========================================
	*  SEND REQ TO MEM TO DUPLICATE DATA FOR FORK
	* ===========================================
	*/
	old_off = mnmap->base_addr;
	new_off = ((struct memory_node_mapping *)new->vm_private_data)->base_addr;
	send_memcpy_request(mnmap->mn_stat->node_info, old_off, new_off, copy_len);
	res = 0;
	return res;
}

unsigned long vma_pages(struct vm_area_struct *vma)
{
	return (vma->vm_end - vma->vm_start) >> PAGE_SHIFT;
}

static int dup_mmap(struct mm_struct *mm, struct mm_struct *oldmm)
{
	struct vm_area_struct *prev, *mgnt, **pprev, *tmp;
	struct rb_node **rb_link, *rb_parent;
	int retval = 0;
	// int error = 0;

	if (!mm || !oldmm)
	{
		return -EINTR;
	}

	sem_wait(&oldmm->mmap_sem);
	sem_wait(&mm->mmap_sem);

	// Copy field from oldmm
	mm->total_vm = oldmm->total_vm;
	mm->data_vm = oldmm->data_vm;
	mm->exec_vm = oldmm->exec_vm;
	mm->stack_vm = oldmm->stack_vm;

	mm->addr_trans_rule = 0;
	mm->max_addr_trans_rule = 0;
	mm->acc_ctrl_rule = 0;
	mm->max_acc_ctrl_rule = 0;

	rb_link = &mm->mm_rb.rb_node;
	rb_parent = NULL;
	prev = NULL;
	pprev = &mm->mmap;

	for (mgnt = oldmm->mmap; mgnt; mgnt = mgnt->vm_next)
	{
		if (mgnt->vm_flags & VM_DONTCOPY)
		{
			//decrease stat counter, because it will not be copied
			vm_stat_account(mm, mgnt->vm_flags, -vma_pages(mgnt));
			continue;
		}

		tmp = malloc(sizeof(*tmp));
		if (!tmp)
		{
			retval = -ENOMEM; // cannot allocate memory
			goto mn_fork_mmap_error;
		}
		memset(tmp, 0, sizeof(*tmp));
		*tmp = *mgnt;
		tmp->vm_rb.vma = tmp;
		tmp->vm_mm = mm;
		tmp->vm_flags &= ~(VM_LOCKED | VM_LOCKONFAULT);
		tmp->vm_next = tmp->vm_prev = NULL;

		if (mgnt->vm_private_data)
		{
			unsigned long len = tmp->vm_end - tmp->vm_start;

			tmp->vm_private_data = malloc(sizeof(struct memory_node_mapping));
			if (!tmp->vm_private_data)
			{
				goto fail_nomem;
			}
			memset(tmp->vm_private_data, 0, sizeof(struct memory_node_mapping));

			// ASSUME it would be copy on write eventually
			if (dup_data_vma(mgnt, tmp, len, len, 0, 0))
				goto fail_nomem;
		}
		else
		{
			tmp->vm_private_data = NULL;
		}

		DEBUG_print_one_vma(tmp, mm->map_count);

		/*
		 * Link in the new vma and copy the page table entries.
		 */
		*pprev = tmp;
		pprev = &tmp->vm_next;
		tmp->vm_prev = prev;
		prev = tmp;

		__vma_link_rb(mm, tmp, rb_link, rb_parent); // update rb_tree gaps, insert new node
		rb_link = &tmp->vm_rb.rb_right;
		rb_parent = &tmp->vm_rb;

		mm->map_count++;
	}
	/* a new mm has just been created */
	sem_post(&mm->mmap_sem);
	sem_post(&oldmm->mmap_sem);
	return retval;

fail_nomem:
	retval = -ENOMEM; // cannot allocate memory
	if (tmp)
		free(tmp); // not linked yet, just free it
mn_fork_mmap_error:
	printf("mn_fork_mmap_error: mm: 0x%lx, map: 0x%lx\n",
		   (unsigned long)mm, (unsigned long)mm->mmap);

	if (mm->mmap)
		mn_remove_vmas(mm);

	sem_post(&mm->mmap_sem);
	sem_post(&oldmm->mmap_sem);
	return retval;
}

static void mm_init_owner(struct mm_struct *mm, struct task_struct *p)
{
	mm->owner = p;
}

struct mm_struct *mm_init(struct mm_struct *mm, struct task_struct *p)
{
	// Set initial values for mm_struct
	mm->mmap = NULL;
	mm->mm_rb.rb_node = NULL;
	mm->mm_users = 1;
	mm->mm_count = 1;
	sem_init(&mm->mmap_sem, 0, 1);
	mm->map_count = 0;
	mm->locked_vm = 0;
	mm->pinned_vm = 0;
	pthread_spin_init(&mm->page_table_lock, PTHREAD_PROCESS_PRIVATE);
	
	mm_init_owner(mm, p);
	mm->def_flags = 0;

	// Already set to zero, though (by memset)
	mm->addr_trans_rule = 0;
	mm->max_addr_trans_rule = 0;
	mm->acc_ctrl_rule = 0;
	mm->max_acc_ctrl_rule = 0;
	return mm;
}

static void check_mm(struct mm_struct *mm)
{
	(void)mm;
}

static inline void munlock_vma_pages_all(struct vm_area_struct *vma)
{
	(void)vma;
}

/* Release all mmaps. */
static void mn_exit_mmap(struct mm_struct *mm)
{
	struct vm_area_struct *vma;

	vma = mm->mmap;
	if (!vma) /* Can happen if dup_mmap() received an OOM */
		return;

	mn_remove_vmas(mm);

	return;
}

/*
 * Called when the last reference to the mm
 * is dropped: either by a lazy thread or by
 * mmput. Free the page directory and the mm.
 */
void __mn_mmdrop(struct mm_struct *mm)
{
	check_mm(mm);
	sem_destroy(&mm->mmap_sem);
	free(mm);
}

static inline void mn_mmdrop(struct mm_struct *mm)
{
	if ((--mm->mm_count) == 0)
		__mn_mmdrop(mm);
}

static inline void __mn_mmput(struct mm_struct *mm)
{
	mn_exit_mmap(mm);
	mn_mmdrop(mm);
}

void mn_mmput(struct mm_struct *mm)
{
	if ((--mm->mm_users) == 0)
	{
		__mn_mmput(mm);
	}
}

/*
 * Allocate a new mm structure and copy contents from the
 * mm structure of the passed in task structure.
 */
struct mm_struct *mn_dup_mm(struct task_struct *new_tsk,
							struct task_struct *old_tsk)
{
	struct mm_struct *mm = NULL, *oldmm = NULL;
	int err;
	// We assuem that we already hold spinlock for old_tsk

	if (old_tsk)
		oldmm = old_tsk->mm;

	// 1) Allocation
	mm = malloc(sizeof(*mm));
	if (!mm)
		return NULL;
	memset(mm, 0, sizeof(*mm));

	if (oldmm)
	{
		printf("Mn_dup_mm: copy mm meta-data: 0x%lx -> 0x%lx\n",
			   (unsigned long)oldmm, (unsigned long)mm);
		memcpy(mm, oldmm, sizeof(*mm));
	}

	// 2) Initialization
	if (!mm_init(mm, new_tsk))
		goto fail_nomem;

	// 3) Actual copy
	if (oldmm)
	{
		err = dup_mmap(mm, oldmm);
		if (err)
		{
			// Error
			printf("Mn_dup_mm: Cannot dup_mmap (err: %d)\n", err);
			goto free_pt;
		}

		mm->hiwater_rss = 0;
		mm->hiwater_vm = mm->total_vm;
	}
	else
	{
		mm->hiwater_rss = 0;			   // no rss stat
		mm->hiwater_vm = mm->total_vm = 0; // no vma
		mm->testing_vma.data_addr = 0;
		mm->testing_vma.meta_addr = 0;
	}
	return mm;

free_pt:
	mn_mmput(mm);

fail_nomem:
	return NULL;
}

/*
 * mm/internal.h
 * Data area - private, writable, not stack
 */
static inline int is_data_mapping(vm_flags_t flags)
{
	return (flags & (VM_WRITE | VM_SHARED | VM_STACK)) == VM_WRITE;
}

int may_expand_vm(struct mm_struct *mm, vm_flags_t flags, unsigned long npages)
{
	(void)mm;
	(void)flags;
	(void)npages;

	// ASSUME: always expandable
	return 1;
}

/* Munmap is split into 2 main parts -- this part which finds
 * what needs doing, and the areas themselves, which do the
 * work.  This now handles partial unmappings.
 * Jeremy Fitzhardinge <jeremy@goop.org>
 */
int mn_munmap(struct mm_struct *mm, unsigned long start, size_t len)
{
	unsigned long end;
	struct vm_area_struct *vma, *prev, *last;	//, *tmp_test;
	unsigned long tar_start = -1, tar_end = -1;
	struct vm_area_struct *affected_list = NULL;

	if ((offset_in_page(start)) || start > TASK_SIZE || len > TASK_SIZE - start) //linux/mm.h
		return -EINVAL;

	len = PAGE_ALIGN(len); //linux/mm.h
	if (len == 0)
		return -EINVAL;

	/* Find the first overlapping VMA */
	vma = mn_find_vma(mm, start);
	if (!vma)
		return 0;
	prev = vma->vm_prev;
	/* We have  start < vma->vm_end  */

	/* If it doesn't overlap, we have nothing.. */
	end = start + len;
	if (vma->vm_start >= end)
		return 0;

	// Remove translation for the affected VMAs
	if (!vma->vm_file)
	{
		struct vm_area_struct *tar = vma;
		struct vm_area_struct *tmp;
		tar_start = vma->vm_start;

		while (tar && tar->vm_start < end)
		{
			if (affected_list)
			{
				tmp->vm_next = malloc(sizeof(struct vm_area_struct));
				tmp = tmp->vm_next;
			}
			else
			{
				affected_list = malloc(sizeof(struct vm_area_struct));
				tmp = affected_list;
			}

			tmp->vm_next = NULL;
			tmp->vm_start = tar->vm_start;
			tmp->vm_end = tar->vm_end;

			tar_end = tar->vm_end;
			tar = tar->vm_next;
		}
	}

	/*
	 * If we need to split any vma, do it now to save pain later.
	 *
	 * Note: mremap's move_vma VM_ACCOUNT handling assumes a partially
	 * unmapped vm_area_struct will remain in use: so lower split_vma
	 * places tmp vma above, and higher split_vma places tmp vma below.
	 */
	if (start > vma->vm_start)
	{
		int error;

		error = __mn_split_vma(mm, vma, start, 0);
		if (error)
			return error;
		prev = vma;
	}

	/* Does it split the last one? */
	last = mn_find_vma(mm, end);
	if (last && end > last->vm_start)
	{
		int error = __mn_split_vma(mm, last, end, 1);
		if (error)
			return error;
	}
	vma = prev ? prev->vm_next : mm->mmap;

	/*
	 * unlock any mlock()ed ranges before detaching vmas
	 */
	if (mm->locked_vm)
	{
		struct vm_area_struct *tmp = vma;
		while (tmp && tmp->vm_start < end)
		{
			if (tmp->vm_flags & VM_LOCKED)
			{
				mm->locked_vm -= vma_pages(tmp); // linux/mm.h
				munlock_vma_pages_all(tmp);
			}
			tmp = tmp->vm_next;
		}
	}

	/*
	 * Remove the vma's, and unmap the actual pages
	 */
	detach_vmas_to_be_unmapped(mm, vma, prev, end);

	/* Fix up all other VM information */
	remove_vma_list(mm, vma);

	// Add rules for remaining VMAs
	if (tar_start > 0)
	{
		vma = mn_find_vma(mm, tar_start);
		while (vma && vma->vm_start < tar_end)
		{
			struct memory_node_mapping *mnmap = (struct memory_node_mapping *)vma->vm_private_data;
			add_new_addr_trans_rule(vma->vm_start, vma->vm_end - vma->vm_start, 
									mnmap, mm->owner->tgid, (unsigned int)(vma->vm_flags & 0xF),
									vma);
			vma = vma->vm_next;
		}
	}

	// Remove previous rules
	vma = affected_list;
	while (vma)
	{
		vma->vm_mm = mm;
		del_addr_trans_rule(
			get_full_virtual_address(mm->owner->tgid, vma->vm_start),
			vma->vm_end - vma->vm_start, vma);
		vma = vma->vm_next;
	}

	while (affected_list)
	{
		vma = affected_list;
		affected_list = affected_list->vm_next;
		free(vma);
	}
	return 0;
}

/*
 * Functions for mremap
 */
static int vma_is_anonymous(struct vm_area_struct *vma)
{
	if (vma->vm_file)
	{
		return 0;
	}else{
		return 1;
	}
}

/*
 * Copy the vma structure to a new location in the same mm,
 * prior to moving page table entries, to effect an mremap move.
 */
static struct vm_area_struct *
mn_copy_vma(struct vm_area_struct **vmap,
			unsigned long addr, unsigned long len, pgoff_t pgoff,
			int *need_rmap_locks)
{
	struct vm_area_struct *vma = *vmap;
	unsigned long vma_start = vma->vm_start;
	struct mm_struct *mm = vma->vm_mm;
	struct vm_area_struct *new_vma, *prev;
	struct rb_node **rb_link, *rb_parent;
	int faulted_in_anon_vma = 1;

	/*
	 * If anonymous vma has not yet been faulted, update new pgoff
	 * to match new location, to increase its chance of merging.
	 */
	if (vma_is_anonymous(vma))
	{ //mm.h
		pgoff = addr >> PAGE_SHIFT;
		faulted_in_anon_vma = 0;
	}

	if (mn_find_vma_links(mm, addr, addr + len, &prev, &rb_link, &rb_parent))
		return NULL; /* should never get here */
	new_vma = mn_vma_merge(mm, prev, addr, addr + len, vma->vm_flags,
						   vma->vm_file, pgoff);
	if (new_vma)
	{
		/*
		 * Source vma may have been merged into new_vma
		 */
		if ((vma_start >= new_vma->vm_start) &&
			(vma_start < new_vma->vm_end))
		{
			/*
			 * The only way we can get a vma_merge with
			 * self during an mremap is if the vma hasn't
			 * been faulted in yet and we were allowed to
			 * reset the dst vma->vm_pgoff to the
			 * destination address of the mremap to allow
			 * the merge to happen. mremap must change the
			 * vm_pgoff linearity between src and dst vmas
			 * (in turn preventing a vma_merge) to be
			 * safe. It is only safe to keep the vm_pgoff
			 * linear if there are no pages mapped yet.
			 */
			VM_BUG_ON_VMA(faulted_in_anon_vma, new_vma);
			*vmap = vma = new_vma;
		}
		*need_rmap_locks = (new_vma->vm_pgoff <= vma->vm_pgoff);
	}
	else
	{
		new_vma = malloc(sizeof(*new_vma));
		if (!new_vma)
			goto out;
		*new_vma = *vma;
		new_vma->vm_start = addr;
		new_vma->vm_end = addr + len;
		new_vma->vm_pgoff = pgoff;

		if (vma->vm_private_data)
		{
			unsigned long copy_len = min(len, vma->vm_end - vma->vm_start);
			new_vma->vm_private_data = malloc(sizeof(struct memory_node_mapping));
			if (!(new_vma->vm_private_data))
			{
				goto out_free_vma;
			}
			memset(new_vma->vm_private_data, 0, sizeof(struct memory_node_mapping));

			if (dup_data_vma(vma, new_vma, len, copy_len, 0, 0))
				goto out_free_vma;
		}
		else
		{
			new_vma->vm_private_data = NULL;
		}

		vma_link(mm, new_vma, prev, rb_link, rb_parent);
		*need_rmap_locks = 0;
	}
	return new_vma;

out_free_vma:
	free(new_vma);
out:
	return NULL;
}

static unsigned long move_vma(struct vm_area_struct *vma,
							  unsigned long old_addr, unsigned long old_len,
							  unsigned long new_len, unsigned long new_addr,
							  int *locked)
{
	struct mm_struct *mm = vma->vm_mm;
	struct vm_area_struct *new_vma;
	unsigned long vm_flags = vma->vm_flags;
	unsigned long new_pgoff;
	unsigned long excess = 0;
	unsigned long hiwater_vm;
	int split = 0;
	int need_rmap_locks;

	new_pgoff = vma->vm_pgoff + ((old_addr - vma->vm_start) >> PAGE_SHIFT);
	new_vma = mn_copy_vma(&vma, new_addr, new_len, new_pgoff,
						  &need_rmap_locks);
	if (!new_vma)
		return -ENOMEM;

	/* Conceal VM_ACCOUNT so old reservation is not undone */
	if (vm_flags & VM_ACCOUNT)
	{
		vma->vm_flags &= ~VM_ACCOUNT;
		excess = vma->vm_end - vma->vm_start - old_len;
		if (old_addr > vma->vm_start &&
			old_addr + old_len < vma->vm_end)
			split = 1;
	}

	/*
	 * If we failed to move page tables we still do total_vm increment
	 * since do_munmap() will decrement it by old_len == new_len.
	 *
	 * Since total_vm is about to be raised artificially high for a
	 * moment, we need to restore high watermark afterwards: if stats
	 * are taken meanwhile, total_vm and hiwater_vm appear too high.
	 * If this were a serious issue, we'd add a flag to do_munmap().
	 */
	hiwater_vm = mm->hiwater_vm;
	vm_stat_account(mm, vma->vm_flags, new_len >> PAGE_SHIFT);

	if (mn_munmap(mm, old_addr, old_len) < 0)
	{
		/* OOM: unable to split vma, just get accounts right */
		excess = 0;
	}
	mm->hiwater_vm = hiwater_vm;

	/* Restore VM_ACCOUNT if one or two pieces of vma left */
	if (excess)
	{
		vma->vm_flags |= VM_ACCOUNT;
		if (split)
			vma->vm_next->vm_flags |= VM_ACCOUNT;
	}

	if (vm_flags & VM_LOCKED)
	{
		mm->locked_vm += new_len >> PAGE_SHIFT;
		*locked = 1;
	}

	return new_addr;
}

static inline void *ERR_PTR(long error)
{
	return (void *)error;
}

static inline long PTR_ERR(const void *ptr)
{
	return (long)ptr;
}

static struct vm_area_struct *vma_to_resize(struct task_struct *tsk,
											unsigned long addr, unsigned long old_len, unsigned long new_len,
											unsigned long *p)
{
	struct mm_struct *mm = tsk->mm;
	struct vm_area_struct *vma = mn_find_vma(mm, addr);
	unsigned long pgoff;

	if (!vma || vma->vm_start > addr)
		return ERR_PTR(-EFAULT);

	/*
	 * !old_len is a special case where an attempt is made to 'duplicate'
	 * a mapping.  This makes no sense for private mappings as it will
	 * instead create a fresh/new mapping unrelated to the original.  This
	 * is contrary to the basic idea of mremap which creates new mappings
	 * based on the original.  There are no known use cases for this
	 * behavior.  As a result, fail such attempts.
	 */
	if (!old_len && !(vma->vm_flags & (VM_SHARED | VM_MAYSHARE)))
	{
		fprintf(stderr, "%s (%d): attempted to duplicate a private mapping with mremap.  This is not supported.\n", tsk->comm, tsk->pid);
		return ERR_PTR(-EINVAL);
	}

	/* We can't remap across vm area boundaries */
	if (old_len > vma->vm_end - addr)
		return ERR_PTR(-EFAULT);

	if (new_len == old_len)
		return vma;

	/* Need to be careful about a growing mapping */
	pgoff = (addr - vma->vm_start) >> PAGE_SHIFT;
	pgoff += vma->vm_pgoff;
	if (pgoff + (new_len >> PAGE_SHIFT) < pgoff)
		return ERR_PTR(-EINVAL);

	if (vma->vm_flags & (VM_DONTEXPAND | VM_PFNMAP))
		return ERR_PTR(-EFAULT);

	if (!may_expand_vm(mm, vma->vm_flags,
					   (new_len - old_len) >> PAGE_SHIFT))
		return ERR_PTR(-ENOMEM);

	if (vma->vm_flags & VM_ACCOUNT)
	{
		unsigned long charged = (new_len - old_len) >> PAGE_SHIFT;
		*p = charged;
	}

	return vma;
}

static unsigned long mremap_to(struct task_struct *tsk,
							   unsigned long addr, unsigned long old_len,
							   unsigned long new_addr, unsigned long new_len, int *locked)
{
	struct mm_struct *mm = tsk->mm;
	struct vm_area_struct *vma;
	unsigned long ret = -EINVAL;
	unsigned long charged = 0;
	unsigned long map_flags;

	if (offset_in_page(new_addr))
		goto out;

	if (new_len > TASK_SIZE || new_addr > TASK_SIZE - new_len)
		goto out;

	/* Ensure the old/new locations do not overlap */
	if (addr + old_len > new_addr && new_addr + new_len > addr)
		goto out;

	ret = mn_munmap(mm, new_addr, new_len);
	if (ret)
		goto out;

	if (old_len >= new_len)
	{
		ret = mn_munmap(mm, addr + new_len, old_len - new_len);
		if (ret && old_len != new_len)
			goto out;
		old_len = new_len;
	}

	vma = vma_to_resize(tsk, addr, old_len, new_len, &charged);
	if (IS_ERR_VALUE((unsigned long)vma))
	{
		ret = PTR_ERR(vma);
		goto out;
	}

	map_flags = MAP_FIXED;
	if (vma->vm_flags & VM_MAYSHARE)
		map_flags |= MAP_SHARED;

	ret = mn_get_unmapped_area(tsk, new_addr, new_len, vma->vm_pgoff + ((addr - vma->vm_start) >> PAGE_SHIFT),
							   map_flags, vma->vm_file);
	if (offset_in_page(ret))
		goto out1;

	ret = move_vma(vma, addr, old_len, new_len, new_addr, locked);
	if (!(offset_in_page(ret)))
		goto out;
out1:
out:
	return ret;
}

static int vma_expandable(struct task_struct *tsk,
						  struct vm_area_struct *vma, unsigned long delta)
{
	unsigned long end = vma->vm_end + delta;
	if (end < vma->vm_end) /* overflow */
		return 0;
	if (vma->vm_next && vma->vm_next->vm_start < end) /* intersection */
		return 0;
	if (mn_get_unmapped_area(tsk, vma->vm_start, end - vma->vm_start,
							 0, MAP_FIXED, NULL) &
		~PAGE_MASK)
		return 0;
	return 1;
}

unsigned long mn_mremap(struct task_struct *tsk, unsigned long addr, unsigned long old_len,
						unsigned long new_len, unsigned long flags, unsigned long new_addr)
{
	struct mm_struct *mm = tsk->mm;
	struct vm_area_struct *vma;
	unsigned long ret = 0;
	unsigned long charged = 0;
	int locked = 0;
	spinlock_t *mn_lock = NULL;

	if (flags & ~(MREMAP_FIXED | MREMAP_MAYMOVE))
		return ret;

	if (flags & MREMAP_FIXED && !(flags & MREMAP_MAYMOVE))
		return ret;

	if (offset_in_page(addr))
		return ret;

	old_len = PAGE_ALIGN(old_len);
	new_len = PAGE_ALIGN(new_len);

	/*
	 * We allow a zero old-len as a special case
	 * for DOS-emu "duplicate shm area" thing. But
	 * a zero new-len is nonsensical.
	 */
	if (!new_len)
		return ret;

	if (flags & MREMAP_FIXED)
	{
		ret = mremap_to(tsk, addr, old_len, new_addr, new_len, &locked);
		goto out;
	}

	/*
	 * Always allow a shrinking remap: that just unmaps
	 * the unnecessary pages..
	 * do_munmap does all the needed commit accounting
	 */
	if (old_len >= new_len)
	{
		ret = mn_munmap(mm, addr + new_len, old_len - new_len);
		if (ret && old_len != new_len)
			goto out;
		ret = addr;
		goto out;
	}

	/*
	 * Ok, we need to grow..
	 */
	vma = vma_to_resize(tsk, addr, old_len, new_len, &charged);
	if (IS_ERR_VALUE((unsigned long)vma))
	{
		ret = 0;
		goto out;
	}

	/* 
	 * old_len exactly to the end of the area..
	 */
	if (old_len == vma->vm_end - addr)
	{
		/* Can we just expand the current mapping? */
		if (vma_expandable(tsk, vma, new_len - old_len))
		{
			int pages = (new_len - old_len) >> PAGE_SHIFT;

			if (__vma_adjust(vma, vma->vm_start, addr + new_len,
							 vma->vm_pgoff, NULL, NULL))
			{
				ret = 0;
				goto out;
			}

			vm_stat_account(mm, vma->vm_flags, pages);
			ret = addr;
			goto out;
		}
	}

	/*
	 * We weren't able to just expand or shrink the area,
	 * we need to create a new one and move it..
	 */
	ret = 0;
	if (flags & MREMAP_MAYMOVE)
	{
		unsigned long map_flags = 0;
		if (vma->vm_flags & VM_MAYSHARE)
			map_flags |= MAP_SHARED;

		if (!vma->vm_file)
		{
			new_addr = get_available_virtual_address_lock(new_len, &mn_lock);
		}
		else
		{
			new_addr = mn_get_unmapped_area(tsk, 0, new_len,
											vma->vm_pgoff + ((addr - vma->vm_start) >> PAGE_SHIFT),
											map_flags, vma->vm_file);
		}
		if (offset_in_page(new_addr))
		{
			ret = new_addr;
			goto out;
		}

		ret = move_vma(vma, addr, old_len, new_len, new_addr, &locked);
	}
out:
	if (offset_in_page(ret))
	{
		locked = 0;
	}

	if (mn_lock)
	{
		pthread_spin_unlock(mn_lock);
	}

	return ret;
}

/*
 * We account for memory if it's a private writeable mapping,
 * not hugepages and VM_NORESERVE wasn't set.
 */
static inline int accountable_mapping(unsigned long *file, vm_flags_t vm_flags)
{
	(void)file;
	return (vm_flags & (VM_NORESERVE | VM_SHARED | VM_WRITE)) == VM_WRITE;
}

/*
 * Executable code area - executable, not writable, not stack
 */
static inline int is_exec_mapping(vm_flags_t flags)
{
	return (flags & (VM_EXEC | VM_WRITE | VM_STACK)) == VM_EXEC;
}

/*
 * Stack area - atomatically grows in one direction
 *
 * VM_GROWSUP / VM_GROWSDOWN VMAs are always private anonymous:
 * do_mmap() forbids all other combinations.
 */
static inline int is_stack_mapping(vm_flags_t flags)
{
	return (flags & VM_STACK) == VM_STACK;
}

void vm_stat_account(struct mm_struct *mm, vm_flags_t flags, long npages)
{
	mm->total_vm += npages;

	if (is_exec_mapping(flags))
		mm->exec_vm += npages;
	else if (is_stack_mapping(flags))
		mm->stack_vm += npages;
	else if (is_data_mapping(flags))
		mm->data_vm += npages;
}

static struct vm_area_struct *mn_allocate_vma(struct mm_struct *mm, unsigned long addr,
											  unsigned long len, vm_flags_t vm_flags, unsigned long pgoff)
{
	struct vm_area_struct *vma;
	vma = malloc(sizeof(*vma));
	if (!vma)
	{
		return NULL;
	}
	memset(vma, 0, sizeof(*vma));

	vma->vm_mm = mm;
	vma->vm_start = addr;
	vma->vm_end = addr + len;
	vma->vm_flags = vm_flags;
	vma->vm_pgoff = pgoff;
	vma->vm_private_data = NULL;
	vma->vm_rb.vma = vma;
	return vma;
}

/*
 * If a hint addr is less than mmap_min_addr change hint to be as
 * low as possible but still greater than mmap_min_addr
 */
#define mmap_min_addr MN_VA_MIN_ADDR
inline unsigned long mn_round_hint_to_min(unsigned long hint)
{
	hint &= PAGE_MASK;
	if (((void *)hint != NULL) &&
		(hint < mmap_min_addr))
		return PAGE_ALIGN(mmap_min_addr);
	return hint;
}

unsigned long _mn_do_mmap(struct task_struct *tsk, unsigned long addr,
						  unsigned long len, unsigned long prot,
						  unsigned long flags, vm_flags_t vm_flags,
						  unsigned long pgoff, unsigned long *file,
						  int is_test)
{
	spinlock_t *mn_lock = NULL;
	(void)prot;
	int fixed_addr = addr ? 1 : 0;	// given addr
	unsigned long tmp_addr;

	if (!(flags & MAP_FIXED))
	{
		addr = mn_round_hint_to_min(addr);
	}

	// Here we assume that the private AND anonymous maps are intended to use RW
	// if there are not shared and not executable
	if ((flags & MAP_PRIVATE) && (flags & MAP_ANONYMOUS) && !(flags & MAP_SHARED))
	{
		if (!(vm_flags & VM_EXEC))
			vm_flags |= (VM_READ | VM_WRITE);
	}

	/* 
	 * Obtain the address to map to. we verify (or select) it and ensure
	 * that it represents a valid section of the address space.
	 */
	if (!addr && !file)
	{
		addr = get_available_virtual_address_lock(len, &mn_lock);
	}
	else
	{
		addr = mn_get_unmapped_area(tsk, addr, len, pgoff, flags, file);
	}

	if (offset_in_page(addr))
		goto disagg_mmap_out; // error in addr (unaligned)
	
	if (!addr)
	{
		fprintf(stderr, "Cannot allocate address\n");
		if (mn_lock)
		{
			pthread_spin_unlock(mn_lock);
		}
		BUG();
	}

	switch (flags & MAP_TYPE)
	{
	case MAP_SHARED:
		if (vm_flags & (VM_GROWSDOWN | VM_GROWSUP))
		{
			addr = -EINVAL;
			goto disagg_mmap_out;
		}
		/*
		* Ignore pgoff.
		*/
		pgoff = 0;
		vm_flags |= VM_SHARED | VM_MAYSHARE;
		break;
	case MAP_PRIVATE:
		/*
		* Set pgoff according to addr for anon_vma.
		*/
		pgoff = addr >> PAGE_SHIFT;
		break;
	default:
		addr = -EINVAL;
		goto disagg_mmap_out;
	}

	if (!file)
	{
		tmp_addr = addr;
		while (len > 0)
		{
			unsigned long remain = min(len, DISAGG_VMA_MAX_SIZE);
			tmp_addr = mn_mmap_region(tsk, tmp_addr, remain,
									  vm_flags, pgoff, file, fixed_addr,
									  is_test);
			if (IS_ERR_VALUE(tmp_addr))
			{
				addr = tmp_addr;
				goto disagg_mmap_out;
			}
			tmp_addr += remain;
			len -= remain;
		}
	}else{
		addr = mn_mmap_region(tsk, addr, len,
							  vm_flags, pgoff, file, fixed_addr, 0);
	}

disagg_mmap_out:
	if (mn_lock)
	{
		pthread_spin_unlock(mn_lock);
	}

	return addr;
}

static int check_alignment_and_init_memory(struct vm_area_struct *vma, struct mm_struct *mm,
										   unsigned long addr, unsigned long len)
{
	struct vm_area_struct *tmp = vma;
	while (vma && vma->vm_end <= addr + len)
	{
		// Only for anonymous
		if (vma && !vma->vm_file)
		{
			// Check alignment
			if (decompose_vma_for_alignment(mm, vma))
			{
				fprintf(stderr, "Cannot align vma [0x%lx - 0x%lx]\n", vma->vm_start, vma->vm_end);
				free(vma);
				return -ENOMEM;
			}

			// Initialize memory for vma
			if (initialize_vma_memory(vma))
			{
				fprintf(stderr, "Cannot init vma [0x%lx - 0x%lx]\n", vma->vm_start, vma->vm_end);
				free(vma);
				return -ENOMEM;
			}
		}
		vma = vma->vm_next;
	}

	// Try merging VMAs
	vma = tmp;
	while (vma)
	{
		if (!_existing_vma_merge(mm, vma))
		{
			vma = vma->vm_next; // Go to the first VMA
		}
	}
	return 0;
}

unsigned long mn_mmap_region(struct task_struct *tsk, unsigned long addr,
							 unsigned long len, vm_flags_t vm_flags, unsigned long pgoff,
							 unsigned long *file, int fixed_addr, int is_test)
{
	struct mm_struct *mm = tsk->mm;
	struct vm_area_struct *vma, *prev;
	struct rb_node **rb_link, *rb_parent;
	(void)fixed_addr;

	// Power of 2 allocation for anonymous mapping
	if (!file && !fixed_addr)
		len = get_pow_of_two_req_size(len);

	/* Check against address space limit. */
	if (!may_expand_vm(mm, vm_flags, len >> PAGE_SHIFT))
	{
		unsigned long nr_pages;

		/*
		 * MAP_FIXED may remove pages of mappings that intersects with
		 * requested mapping. Account for the pages it would unmap.
		 */
		nr_pages = count_vma_pages_range(mm, addr, addr + len);

		if (!may_expand_vm(mm, vm_flags,
						   (len >> PAGE_SHIFT) - nr_pages))
			return -ENOMEM;
	}

	/* Clear old maps */
	if (mn_find_vma_links(mm, addr, addr + len, &prev, &rb_link,
							 &rb_parent))
	{
		if (mn_munmap(mm, addr, len))
		{
			fprintf(stderr, "Cannot unmap old vmas for [0x%lx - 0x%lx]\n",
					addr, addr + len);
			return -ENOMEM;
		}
	}

	/*
	 * Private writable mapping: check memory availability
	 */
	if (accountable_mapping(file, vm_flags))
	{
		vm_flags |= VM_ACCOUNT;
	}

	/*
	 * Can we just expand an old mapping?
	 */
	vma = mn_vma_merge(mm, prev, addr, addr + len, vm_flags,
					   file, pgoff);
	pr_vma("MMAP: vma after trying merge: 0x%lx\n", (unsigned long)vma);
	if (vma)
		goto out;

	/*
	 * Determine the object being mapped and call the appropriate
	 * specific mapper. the address has already been validated, but
	 * not unmapped, but the maps are removed from the list.
	 */
	vma = mn_allocate_vma(mm, addr, len, vm_flags, pgoff);
	if (!vma)
		goto unacct_error;
	vma->is_test = is_test;

	pr_vma("MMAP: vma allocated [0x%lx - 0x%lx]\n", vma->vm_start, vma->vm_end);
	// Dummy file
	if (file)
	{
		vma->vm_file = file; // just copy pointer, use it just as a identifier
							 // NOTE: No actual allcation for the file pointer
	}

	vma_link(mm, vma, prev, rb_link, rb_parent);

	// Initialize memory for vma
	if (check_alignment_and_init_memory(vma, mm, addr, len))
	{
		free(vma);
		goto unacct_error;
	}

out:
	vm_stat_account(mm, vm_flags, len >> PAGE_SHIFT); //increase total vm
	if (vm_flags & VM_LOCKED)
	{
		mm->locked_vm += (len >> PAGE_SHIFT);
	}

	return addr;

unacct_error:
	return -EINTR; //no address = NULL
}

/*
 *  this is really a simplified "do_mmap".  it only handles
 *  anonymous maps.  eventually we may be able to do some
 *  brk-specific accounting here.
 */
int mn_do_brk_flags(struct task_struct *tsk, unsigned long addr,
					unsigned long request, unsigned long flags)
{
	struct mm_struct *mm = tsk->mm;
	struct vm_area_struct *vma, *prev;
	unsigned long len;
	struct rb_node **rb_link, *rb_parent;
	pgoff_t pgoff = addr >> PAGE_SHIFT;
	int error;

	len = PAGE_ALIGN(request);
	if (len < request)
		return -ENOMEM;
	if (!len)
		return 0;

	/* Until we need other flags, refuse anything except VM_EXEC. */
	if ((flags & (~VM_EXEC)) != 0)
		return -EINVAL;
	flags |= VM_DATA_DEFAULT_FLAGS | VM_ACCOUNT | mm->def_flags;

	error = mn_get_unmapped_area(tsk, addr, len, 0, MAP_FIXED, NULL);
	if (offset_in_page(error))
		return error;

	/*
	 * Clear old maps.  this also does some error checking for us
	 */
	while (mn_find_vma_links(mm, addr, addr + len, &prev, &rb_link,
							 &rb_parent))
	{
		if (mn_munmap(mm, addr, len))
			return -ENOMEM;
	}

	/* Check against address space limits *after* clearing old maps... */
	if (!may_expand_vm(mm, flags, len >> PAGE_SHIFT))
		return -ENOMEM;

	vma = mn_vma_merge(mm, prev, addr, addr + len, flags, 0, pgoff);
	if (vma)
		goto out;

	/*
	 * create a vma struct for an anonymous mapping
	 */
	vma = mn_allocate_vma(mm, addr, len, flags, pgoff);
	if (!vma)
	{
		return -ENOMEM;
	}
	vma_link(mm, vma, prev, rb_link, rb_parent);

	check_alignment_and_init_memory(vma, mm, addr, len);

out:
	vm_stat_account(mm, flags, len >> PAGE_SHIFT);
	if (vma->vm_flags & VM_LOCKED)
	{
		mm->locked_vm += (len >> PAGE_SHIFT);
	}

	printf("BRK: tgid: %d, pid: %d, addr: 0x%lx, len: %lu, flag: 0x%lx\n",
		   (int)tsk->tgid, (int)tsk->pid, addr, len, flags);

	return 0;
}
