#include "controller.h"
#include "memory_management.h"
#include "request_handler.h"
#include "rbtree_ftns.h"
#include "cacheline_manager.h"
#include "cache_manager_thread.h"
#include "cacheline_def.h"
#include "config.h"
#include <stdio.h>

static int mn_common_new_task_mm(
	u16 sender_id, u16 tgid, u16 pid, struct task_struct *old_tsk)
{
	// Old_tsk can be NULL to generate dummy structures
	struct task_struct *tsk;
	int res = -1;
	int i;

	// Generate dummy task struct
	tsk = (struct task_struct*)malloc(sizeof(*tsk));
	if (!tsk)
	{
		fprintf(stderr, "Cannot allocate new task");
		goto new_task_mm_out;
	}

	// Initialize generated task with new mm
	tsk->tgid = tgid;
	tsk->pid = pid;
	tsk->primary_node = sender_id;
	for (i = 0; i < MAX_NUMBER_COMPUTE_NODE + 1; i++)
	{
		tsk->ref_cnt[i] = 0;
	}
	tsk->ref_cnt[sender_id] = 1;

	for (i = 0; i < MAX_NUMBER_COMPUTE_NODE + 1; i++)
	{
		tsk->tgids[i] = 0;
	}
	tsk->tgids[sender_id] = tgid;
	tsk->mm = mn_dup_mm(tsk, old_tsk);
	if (!tsk->mm)
	{
		fprintf(stderr, "Cannot allocate new mm");
		free(tsk);
		goto new_task_mm_out;
	}

	// Put the task struct
	res = mn_insert_new_task_mm(sender_id, tgid, tsk);

new_task_mm_out:
	return res;
}

// Memory management used in handlers
int mn_create_dummy_task_mm(u16 sender_id, u16 tgid, u16 pid)
{
	return mn_common_new_task_mm(sender_id, tgid, pid, NULL);
}

int mn_create_mm_from(u16 sender_id, u16 tgid, u16 pid, struct task_struct *old_tsk, u32 clone_flags)
{
	int ret = -1;
	(void)clone_flags;	// not used for now
	ret = mn_common_new_task_mm(sender_id, tgid, pid, old_tsk);
	return ret;
}

// From EXEC handler
int mn_update_mm(u16 sender_id, u16 tgid, struct exec_msg_struct* exec_req)
{
	int ret = -1;
	struct task_struct *tsk = mn_get_task(sender_id, tgid);

    if (!tsk)
    {
		return -1;	// Must be forked first
    }else{
		// Erase existing vmas and rb-tree
		if (sem_wait(&tsk->mm->mmap_sem)) {
			return -EINTR;
		}
		mn_remove_vmas_exec(tsk->mm);
		sem_post(&tsk->mm->mmap_sem);
	}

	// For debugging: print difference of mmap in mn and that in exec req
	// DEBUG_print_vma_diff(tsk->mm, exec_req);

	// Copy vmas and rb-tree
	ret = mn_build_mmap_from_exec(tsk->mm, exec_req);

	// For debugging: print the result--vma list
	// DEBUG_print_vma(tsk->mm);
	return ret;
}

int mn_check_vma(u16 sender_id, u16 tgid, struct exec_msg_struct* exec_req)
{
	struct task_struct *tsk = mn_get_task(sender_id, tgid);

	// If there is no existing entry, make a dummy one
    if (tsk)
    {
		(void)exec_req;
	}
	else
	{
		fprintf(stderr, "VMA_CHECK - Cannot find task_struct: %u:%u\n", 
				sender_id, tgid);
	}
	return 0;
}

static int is_test_vma_req(
	struct task_struct *tsk, unsigned long len)
{
	if (tsk && tsk->tgid == TEST_PROGRAM_TGID &&
		(len == TEST_INIT_ALLOC_SIZE || len == TEST_MACRO_ALLOC_SIZE))
		return 1;

	return 0;
}

static int is_test_meta_vma_req(
	struct task_struct *tsk, unsigned long len)
{
	if (tsk && tsk->tgid == TEST_PROGRAM_TGID && len == TEST_META_ALLOC_SIZE)
		return 1;

	return 0;
}

unsigned long mn_do_mmap(struct task_struct *tsk, unsigned long addr,
			unsigned long len, unsigned long prot, unsigned long flags, vm_flags_t vm_flags,
			unsigned long pgoff, unsigned long *file, int nid)
{
	struct mm_struct *mm = tsk->mm;	// we just use mm from tsk
	int is_test_req = is_test_vma_req(tsk, len);
	int is_test_meta = is_test_meta_vma_req(tsk, len);
	(void)prot;

	if (!mm)
	{
        return -EINVAL;
	}

	// Check testing VMA
	sem_wait(&mm->mmap_sem);
	if (is_test_req && mm->testing_vma.data_addr)
	{
		printf("Testing data VMA detected: tgid[%u] addr[0x%lx - 0x%lx]\n",
			   tsk->tgid, mm->testing_vma.data_addr, mm->testing_vma.data_addr + len);
		sem_post(&mm->mmap_sem);
		return mm->testing_vma.data_addr;
	}

	if (is_test_meta && mm->testing_vma.meta_addr)
	{
		printf("Testing meta VMA detected: tgid[%u] addr[0x%lx - 0x%lx]\n",
			   tsk->tgid, mm->testing_vma.meta_addr, mm->testing_vma.meta_addr + len);
		sem_post(&mm->mmap_sem);
		return mm->testing_vma.meta_addr;
	}

	if (len % CACHELINE_MAX_SIZE)
		len = ((len / (unsigned long)CACHELINE_MAX_SIZE) + 1) * CACHELINE_MAX_SIZE;		// Make it cacheline aligned
	addr = _mn_do_mmap(tsk, addr, len, prot, flags, vm_flags, pgoff, file, is_test_req);

	if (is_test_req && !IS_ERR_VALUE(addr))
	{
		mm->testing_vma.data_addr = addr;
	}
	else if (is_test_meta && !IS_ERR_VALUE(addr))
	{
		mm->testing_vma.meta_addr = addr;
	}

#ifdef CACHE_DIR_PRE_OPT
	if (!IS_ERR_VALUE(addr) && !file)
	{
		unsigned long tmp_addr = addr;
		int populated = 1;
#ifndef CACHE_DIR_PRE_POP_IDLE
		int state = CACHELINE_SHARED;
#else
		int state = CACHELINE_IDLE;
#endif
#ifdef CACHE_OWNERSHIP_OPT
			state = CACHELINE_MODIFIED;
#endif

		printf("MMAP-Cachline Init: tgid: %d, pid: %d, addr: 0x%lx - 0x%lx, vmflag: 0x%lx, flag: 0x%lx, file: %lu\n",
			   (int)tsk->tgid, (int)tsk->pid, tmp_addr, tmp_addr + len,
			   vm_flags, flags, (unsigned long)file);
		
		if (is_test_meta || is_test_req)
		{
			cacheman_request_unlock();
			cacheman_run_lock();	// To prevent confliction with cache manager thread
			// 16 KB for the first TEST_DEBUG_SIZE_LIMIT entries
			for (; tmp_addr < addr + min(len, TEST_DEBUG_SIZE_LIMIT); tmp_addr += INITIAL_REGION_SIZE)
			{
				if (!create_new_cache_dir(get_full_virtual_address(tsk->tgid, tmp_addr),
										state, nid, INITIAL_REGION_INDEX))
				{
					printf("MMAP-Cachline Init Err: tgid: %d, pid: %d, addr: 0x%lx - 0x%lx, vmflag: 0x%lx, flag: 0x%lx, file: %lu\n",
						(int)tsk->tgid, (int)tsk->pid, tmp_addr, tmp_addr + INITIAL_REGION_SIZE,
						vm_flags, flags, (unsigned long)file);
					populated = 0;
					break;
				}
			}
			// 2 MB regions for the remaining ranges
			if (populated)
			{
				for (; tmp_addr < addr + len; tmp_addr += CACHELINE_MAX_SIZE)
				{
					if (!create_new_cache_dir(get_full_virtual_address(tsk->tgid, tmp_addr),
											state, nid, REGION_SIZE_TOTAL))
					{
						printf("MMAP-Cachline Init Err: tgid: %d, pid: %d, addr: 0x%lx - 0x%lx, vmflag: 0x%lx, flag: 0x%lx, file: %lu\n",
							(int)tsk->tgid, (int)tsk->pid, tmp_addr, tmp_addr + INITIAL_REGION_SIZE,
							vm_flags, flags, (unsigned long)file);
						populated = 0;
						break;
					}
				}
			}
			cacheman_run_unlock();
		}

		if (populated)
		{
			addr |= MMAP_CACHE_DIR_POPULATION_FLAG;
			usleep(1000); // 1 ms
			barrier();
		}
	}
#endif
	sem_post(&mm->mmap_sem);
	return addr;
}

int mn_do_munmap(struct mm_struct *mm, unsigned long start, size_t len)
{
	int res;
	
	sem_wait(&mm->mmap_sem);
	res = mn_munmap(mm, start, len);
	sem_post(&mm->mmap_sem);

	if (is_test_vma_req(mm->owner, len))
	{
		mm->testing_vma.data_addr = 0;
	}
	else if (is_test_meta_vma_req(mm->owner, len))
	{
		mm->testing_vma.meta_addr = 0;
	}

	return res;
}

unsigned long mn_do_mremap(struct task_struct *tsk, unsigned long addr, unsigned long old_len,
			unsigned long new_len, unsigned long flags,	unsigned long new_addr)
{
	unsigned long res;

	sem_wait(&tsk->mm->mmap_sem);
	res = mn_mremap(tsk, addr, old_len, new_len, flags, new_addr);
	sem_post(&tsk->mm->mmap_sem);
	return res;
}

unsigned long mn_do_brk(struct task_struct *tsk, unsigned long brk)
{
	unsigned long newbrk, oldbrk;
	struct mm_struct *mm = tsk->mm;
	struct vm_area_struct *next;
	unsigned long min_brk;

	sem_wait(&mm->mmap_sem);

	min_brk = mm->start_brk;
	if (brk < min_brk)
		goto out;

	printf("BRK: addr: 0x%lx, mm->brk: 0x%lx\n", brk, mm->brk);

	/*
	 * Check against rlimit here. If this check is done later after the test
	 * of oldbrk with newbrk then it can escape the test and let the data
	 * segment grow beyond its set limit the in case where the limit is
	 * not page aligned -Ram Gupta
	 */
	newbrk = PAGE_ALIGN(brk);
	oldbrk = PAGE_ALIGN(mm->brk);
	if (oldbrk == newbrk)
		goto set_brk;

	/* Always allow shrinking brk. */
	if (brk <= mm->brk) {
		if (!mn_munmap(mm, newbrk, oldbrk-newbrk))
			goto set_brk;
		goto out;
	}

	/* Check against existing mmap mappings. */
	next = mn_find_vma(mm, oldbrk);
	if (next && newbrk + PAGE_SIZE > vm_start_gap(next))
		goto out;

	/* Ok, looks good - let it rip. */
	if (mn_do_brk_flags(tsk, oldbrk, newbrk-oldbrk, 0) < 0)
		goto out;	// error case
	
set_brk:
	mm->brk = brk;
	sem_post(&mm->mmap_sem);
	return brk;

out:
	sem_post(&mm->mmap_sem);
	return -ENOMEM;
}
