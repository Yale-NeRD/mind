/*
 * Header file of exec and disaggregated exec functions
 */
#include <linux/exec.h>
#include <disagg/config.h>
#include <disagg/exec_disagg.h>
#include <disagg/network_disagg.h>
#include <disagg/fault_disagg.h>
#include <disagg/cnthread_disagg.h>
#include <disagg/profile_points_disagg.h>
#include <disagg/print_disagg.h>

#include <linux/mm.h>
#include <linux/rmap.h>
#include <linux/mempolicy.h>
#include <asm/pgtable_types.h>
#include <asm/pgtable.h>
#include <asm/mman.h>

int count_vm_field(struct task_struct *tsk)
{
	int tot_num = 0;
	struct vm_area_struct *mpnt;
	for (mpnt = tsk->mm->mmap; mpnt; mpnt = mpnt->vm_next)
	{
		tot_num ++;
	}
	return tot_num;
}

int init_vma_field(struct exec_vmainfo *_vma_buf, struct task_struct *tsk)
{
	int res = 0;
	struct vm_area_struct *mpnt;
	struct exec_vmainfo *vma_buf = _vma_buf;
	for (mpnt = tsk->mm->mmap; mpnt; vma_buf++, mpnt = mpnt->vm_next)
	{
		if (mpnt->vm_start <= tsk->mm->start_stack && tsk->mm->start_stack < mpnt->vm_end)
		{
			// Expand stack boundray
			unsigned long stack_size = 8 * 1024 * 1024;	// pre-allocatedc 8 MB
			if (expand_stack(mpnt, mpnt->vm_end - stack_size))
			{
				BUG();
			}
		}
		vma_buf->vm_start = mpnt->vm_start;
		vma_buf->vm_end = mpnt->vm_end;
		vma_buf->vm_flags = mpnt->vm_flags;
		vma_buf->vm_pgoff = mpnt->vm_pgoff;
		vma_buf->rb_substree_gap = mpnt->rb_subtree_gap;
		vma_buf->vm_page_prot = mpnt->vm_page_prot.pgprot;
		// Use the file pointer as an identifier
		vma_buf->file_id = (unsigned long)(mpnt->vm_file);
	}
	return res;
}

void disagg_print_va_layout(struct mm_struct *mm)
{
	pr_syscall("** CN-VA layout **\n");
	pr_syscall("-total: %lu pages\n", mm->total_vm);
	pr_syscall("-code: 0x%lx - 0x%lx\n", mm->start_code, mm->end_code);
	pr_syscall("-data: 0x%lx - 0x%lx\n", mm->start_data, mm->end_data);
	pr_syscall("-brk: 0x%lx - 0x%lx\n", mm->start_brk, mm->brk);
	pr_syscall("-stack: 0x%lx\n", mm->start_stack);
	pr_syscall("-arg: 0x%lx - 0x%lx\n", mm->arg_start, mm->arg_end);
	pr_syscall("-env: 0x%lx - 0x%lx\n", mm->env_start, mm->env_end);
}

#define CN_COPY_MM_VALUES(EXR, MM, F)	(EXR->F = MM->F)

static void cn_set_up_layout(struct exec_msg_struct* payload,
							 struct mm_struct *mm)
{
	CN_COPY_MM_VALUES(payload, mm, hiwater_rss);
	CN_COPY_MM_VALUES(payload, mm, hiwater_vm);
	CN_COPY_MM_VALUES(payload, mm, total_vm);
	CN_COPY_MM_VALUES(payload, mm, locked_vm);
	CN_COPY_MM_VALUES(payload, mm, pinned_vm);
	CN_COPY_MM_VALUES(payload, mm, data_vm);
	CN_COPY_MM_VALUES(payload, mm, exec_vm);
	CN_COPY_MM_VALUES(payload, mm, stack_vm);
	CN_COPY_MM_VALUES(payload, mm, def_flags);
	CN_COPY_MM_VALUES(payload, mm, start_code);
	CN_COPY_MM_VALUES(payload, mm, end_code);
	CN_COPY_MM_VALUES(payload, mm, start_data);
	CN_COPY_MM_VALUES(payload, mm, end_data);
	CN_COPY_MM_VALUES(payload, mm, start_brk);
	CN_COPY_MM_VALUES(payload, mm, brk);
	CN_COPY_MM_VALUES(payload, mm, start_stack);
	CN_COPY_MM_VALUES(payload, mm, arg_start);
	CN_COPY_MM_VALUES(payload, mm, arg_end);
	CN_COPY_MM_VALUES(payload, mm, env_start);
	CN_COPY_MM_VALUES(payload, mm, env_end);
	CN_COPY_MM_VALUES(payload, mm, mmap_base);
	CN_COPY_MM_VALUES(payload, mm, mmap_legacy_base);
}

int cn_copy_vma_to_mn(struct task_struct *tsk, u32 msg_type)
{
struct exec_msg_struct *payload;
    struct exec_reply_struct reply;
	int ret = 0;
	size_t tot_size = sizeof(struct exec_msg_struct);

	if (msg_type != DISSAGG_CHECK_VMA)
	{
		ret = count_vm_field(tsk);
		if (ret > 0)
			tot_size += sizeof(struct exec_vmainfo) * (ret-1);
	}
	
	// Calculate number of vmas
	// Allocate size: struct size + vmas
    payload = (struct exec_msg_struct*)kmalloc(tot_size, GFP_KERNEL);
	if (!payload)
        return -ENOMEM;
    payload->pid = tsk->pid;
	payload->tgid = tsk->tgid;
	memcpy(payload->comm, tsk->comm, TASK_COMM_LEN);
	cn_set_up_layout(payload, tsk->mm);

	// Put vma information
	payload->num_vma = (u32)ret;
	if (msg_type != DISSAGG_CHECK_VMA)
		init_vma_field(&payload->vmainfos, tsk);

	ret = send_msg_to_memory(msg_type, payload, tot_size, 
                             &reply, sizeof(reply));

	// Now, ret is the received length (may not for RDMA)
	if (ret < 0)
	{
		printk(KERN_ERR "Cannot send EXEC notification - err: %d [%s]\n", 
				ret, tsk->comm);
		goto cn_notify_out;
	}
	ret = 0;

cn_notify_out:
    kfree(payload);
    return ret;
}

static int exec_copy_page_data_to_mn(u16 tgid, struct mm_struct *mm, unsigned long addr,
									 pte_t *pte)
{
	return cn_copy_page_data_to_mn(tgid, mm, addr, pte, CN_ONLY_DATA, 0, NULL);
}

// This function send a request to the switch for checking the current status of the cache directory
static int _send_cache_dir_full_check(u16 tgid, u64 vaddr, u16 *state, u16 *sharer,
							  u16 *dir_size, u16 *dir_lock, u16 *inv_cnt, int sync_direction)
{
	struct exec_msg_struct *payload;
	struct exec_reply_struct reply;
	int ret = 0;
	size_t tot_size = sizeof(struct exec_msg_struct);

	payload = (struct exec_msg_struct *)kmalloc(tot_size, GFP_KERNEL);
	if (!payload)
		return -ENOMEM;

	payload->pid = tgid;
	payload->tgid = tgid;
	payload->brk = sync_direction;
	payload->stack_vm = (unsigned long)vaddr;
	ret = send_msg_to_memory(DISSAGG_CHECK_VMA, payload, tot_size,
							 &reply, sizeof(reply));
	if (ret < 0)
	{
		printk(KERN_ERR "Cannot send %s notification - err: %d\n",
			   __func__, ret);
		goto cn_notify_out;
	}else{
		*state = (u16)(reply.ret >> 16);	//first 16 digit
		*sharer = (u16)(reply.ret & 0xffff); //second 16 digit
		if (dir_size)
			*dir_size = (u16)(reply.vma_count >> 16);
		if (dir_lock)
			*dir_lock = (u16)(reply.vma_count & 0x8000);
		if (inv_cnt)
			*inv_cnt = (u16)(reply.vma_count & 0x7fff);
	}
	ret = 0;

cn_notify_out:
	kfree(payload);
	return ret;
}
#ifdef PRINT_SWITCH_STATUS
int send_cache_dir_full_check(u16 tgid, u64 vaddr, u16 *state, u16 *sharer,
							  u16 *dir_size, u16 *dir_lock, u16 *inv_cnt, int sync_direction)
{return _send_cache_dir_full_check(tgid, vaddr, state, sharer, dir_size, dir_lock, inv_cnt, sync_direction);}
#else
int send_cache_dir_full_check(u16 tgid, u64 vaddr, u16 *state, u16 *sharer,
							  u16 *dir_size, u16 *dir_lock, u16 *inv_cnt, int sync_direction)
{return 0;}
#endif
EXPORT_SYMBOL(send_cache_dir_full_check);

int send_cache_dir_full_always_check(u16 tgid, u64 vaddr, u16 *state, u16 *sharer,
							  u16 *dir_size, u16 *dir_lock, u16 *inv_cnt, int sync_direction)
{return _send_cache_dir_full_check(tgid, vaddr, state, sharer, dir_size, dir_lock, inv_cnt, sync_direction);}
EXPORT_SYMBOL(send_cache_dir_full_always_check);

int send_cache_dir_check(u16 tgid, u64 vaddr, u16 *state, u16 *sharer, int sync_direction)
{
	return send_cache_dir_full_check(tgid, vaddr, state, sharer, NULL, NULL, NULL, sync_direction);
}
EXPORT_SYMBOL(send_cache_dir_check);

// Send data from local memory to remote memory
// Used after fork/clone
int cn_copy_vma_data_to_mn(struct task_struct *tsk, struct vm_area_struct *vma, 
		unsigned long start_addr, unsigned long end_addr)
{
	pte_t *pte = NULL;

	if (end_addr <= start_addr)
	{
		return 0;	//no data to send
	}

	// Find pte
	pte = find_pte_target(tsk->mm, start_addr);
	if (pte && !pte_none(*pte)) // Check for tsk
	{
		// Forked, so same address but from the previous tsk (current)
		pte = find_pte_target(current->mm, start_addr);
		if (pte && !pte_none(*pte) && pte_present(*pte)) //check for cur
		{
			// CHECKME: we might need to grab pte lock before go into cn_copy_page_data_to_mn()
			//			and unlock it after return from cn_copy_page_data_to_mn()
			return exec_copy_page_data_to_mn(tsk->tgid, tsk->mm, start_addr, pte);
		}
	}
	return 0;	// No pte to send
}

DEFINE_PROFILE_POINT(exec_send_data_over_rdma)
/* It copy data from (maybe) other process' memory */
int cn_copy_page_data_to_mn(u16 tgid, struct mm_struct *mm, unsigned long addr,
							pte_t *pte, int is_target_data, u32 req_qp, void *dma_addr)
{
	struct fault_data_struct payload;
	struct fault_reply_struct reply;
	int ret;
	size_t data_size = PAGE_SIZE;
	struct page *page = NULL;
	unsigned long *data_ptr = NULL;
	u32 msg_type;
	PROFILE_POINT_TIME(exec_send_data_over_rdma)

	payload.req_qp = 0;
	payload.data = NULL;
	if (is_target_data == CN_ONLY_DATA)
	{
		msg_type = DISSAGG_DATA_PUSH;
	}
	else
	{
		if (is_target_data == CN_TARGET_PAGE)
		{
			msg_type = DISSAGG_DATA_PUSH_TARGET;
			payload.req_qp = req_qp;
		}else{
			msg_type = DISSAGG_DATA_PUSH_OTHER;
		}
		payload.data = dma_addr;
	}
	payload.pid = tgid;	// dummy
	payload.tgid = tgid;
	payload.address = addr;
	payload.data_size = (u32)data_size;

	if (!payload.data)
	{
		if (pte && !pte_none(*pte))
		{
			page = pte_page(*pte);
		}
		if (page)
		{
			data_ptr = (unsigned long *)kmap(page);
			payload.data = (void *)data_ptr;
		}
	}

	// Get local pte for the mmaping
	if (payload.data)
	{
		PROFILE_START(exec_send_data_over_rdma);
		barrier();
		ret = send_msg_to_memory_rdma(msg_type, &payload, PAGE_SIZE,
									  &reply, sizeof(reply));
		PROFILE_LEAVE(exec_send_data_over_rdma);
	}else{
		ret = -EINTR;
	}

	// Now, ret is the received length
	if (ret < 0)
	{
		pr_warn_ratelimited("Cannot send page data - err: %d, type: %d, dma: 0x%lx\n",
							ret, is_target_data, (unsigned long)dma_addr);
		goto cn_send_page_data_out;
	}
	ret = 0;

cn_send_page_data_out:
	if (data_ptr)
		kunmap(page);
    return ret;
}

/*
 * We assume that caller already holds write lock for mm->mmap_sem
 */
// @is_exec: if set, reset VMAs and clean up all the cachelines for this tgid
//			 to set up initial memory layout for EXEC-ed program
static int _cn_notify_exec(struct task_struct *tsk, int is_exec)
{
	int ret = 0;
	if (is_exec)
		ret = cn_copy_vma_to_mn(tsk, DISSAGG_EXEC);

	if (likely(!ret))
	{
		// No error, now all mapping are stored in memory node
		struct vm_area_struct *cur = tsk->mm->mmap;
		struct vm_area_struct *prev, *next;
		struct mm_struct *mm = tsk->mm;

		while (cur)
		{
			next = cur->vm_next;
			prev = cur->vm_prev;

			// Remove existing vma (anonymous & writable)
			if (vma_is_anonymous(cur) && 
				((cur->vm_flags & VM_WRITE) || // Writable page
				 !(cur->vm_flags & (VM_WRITE | VM_READ | VM_EXEC)))) // Pages in local DRAM page = no permission
			{
				int sent = -1;
				if (tsk->is_test)	// If it is special process for debugging or remote memory micro bench
				{
					if (!TEST_is_test_vma(cur->vm_start, cur->vm_end))
					{
						goto continue_to_next;
					}
					else
					{
						pr_syscall("== [TEST] MAGIC VMA detected: 0x%lx - 0x%lx [file:%d][flag:0x%lx] ==\n",
								   cur->vm_start, cur->vm_end, cur->vm_file ? 1 : 0, cur->vm_flags);
					}
				}

				// Send data to mn
				if (cur->vm_end >= cur->vm_start)
				{
					get_cpu();
					if (cur->vm_end - cur->vm_start > DISAGG_NET_MAX_SIZE_ONCE)
					{
						unsigned long offset = 0;
						while (cur->vm_start + offset < cur->vm_end)
						{
							sent = cn_copy_vma_data_to_mn(tsk, cur, cur->vm_start + offset, 
								min(cur->vm_start + offset + DISAGG_NET_MAX_SIZE_ONCE, 
									cur->vm_end));
							if(sent)
								break;
							offset += DISAGG_NET_MAX_SIZE_ONCE;
						}
					}else{
						sent = cn_copy_vma_data_to_mn(tsk, cur, cur->vm_start, cur->vm_end);
					}
					put_cpu();
				}

				// Remove previous mappings
				if (!sent) // 0: successfully sent, -EINTR: no pte populated
				{
					if((cur->vm_flags & (VM_SHARED | VM_PFNMAP)))
					{
						// Exceptional case: special flags
						pr_syscall("Do-not remove special writable vma: 0x%lx - 0x%lx [file:%d][flag:0x%lx]\n",
								   cur->vm_start, cur->vm_end, cur->vm_file ? 1 : 0, cur->vm_flags);
					}else if (cur->vm_file){
						printk(KERN_WARNING "File VMA must not be removed\n");
						BUG();
					}
					// else if (cur->vm_start >= mm->brk)
					// {
					// 	//TODO: before brk region - any special mapping? (same vma flag..)
					// }
					else
					{
						int stack = 0;
						unsigned long address = cur->vm_start;
						unsigned long res_addr = 0;
						unsigned long len = cur->vm_end - cur->vm_start;
						unsigned long vm_flags = (cur->vm_flags & ~(VM_WRITE | VM_READ | VM_EXEC)) | VM_DONTEXPAND;
						if (cur->vm_start <= mm->start_stack && mm->start_stack < cur->vm_end)
						{
								stack = 1;
						}

						if (!is_exec && tsk->is_test && TEST_is_test_vma(cur->vm_start, cur->vm_end))
						{
							// Keep the test mapping for the forked/child process
							;
						}
						else
						{
							if (unlikely(do_munmap(mm, cur->vm_start, cur->vm_end - cur->vm_start, NULL)))
							{
								ret = -ENOMEM;
								printk(KERN_DEFAULT "Failed to unmap vma: 0x%lx - 0x%lx\n",
										cur->vm_start, cur->vm_end);
								BUG();
							}
							else
							{
								unsigned long tmp_addr;
								pr_cache("Try to clean cacheline for tgid: %u, VA: 0x%lx - 0x%lx\n",
										tsk->tgid, address, address + len);
								for (tmp_addr = address; tmp_addr < address + len; tmp_addr += PAGE_SIZE)
								{
									cnthread_delete_from_list_no_lock(tsk->tgid, tmp_addr);
								}
							}

							res_addr = mmap_dummy_region(mm, address, len, vm_flags);
							if (likely(res_addr == address))
							{
								cur = find_vma(mm, address);
								if (unlikely(!cur || (cur && cur->vm_start > address)))
								{
									printk(KERN_ERR "Failed to initialize clean mmap [0x%lx]: 0x%lx, addr: 0x%lx, res_addr: 0x%lx\n",
										address, (unsigned long)cur, cur ? cur->vm_start : 0, res_addr);
									if (cur && cur->vm_prev)
									{
										printk(KERN_ERR "prev vma: 0x%lx - 0x%lx\n",
											cur->vm_prev->vm_start, cur->vm_prev->vm_end);
									}
									BUG();
								}
							}else{
								BUG();
							}
						}
					}
				}else{
					pr_syscall("**WARN: cannot send vma data: 0x%lx - 0x%lx [%lu]\n",
							   cur->vm_start, cur->vm_end, cur->vm_end - cur->vm_start);
				}

			}else if(cur->vm_flags & VM_WRITE){
				if (!cur->vm_file){	// print out errorous case only
					pr_syscall("**WARN: non-anon & non-file but wriatble (f:%d): 0x%lx - 0x%lx\n",
							   cur->vm_file ? 1 : 0, cur->vm_start, cur->vm_end);
				}
			}else if(vma_is_anonymous(cur)){
				pr_syscall("**WARN: anon but read-only: 0x%lx - 0x%lx\n",
						   cur->vm_start, cur->vm_end);
			}
continue_to_next:
			cur = next;
		}

		if (is_exec)
		{
			cnthread_clean_up_non_existing_entry(tsk->tgid, mm);
		}
	}else{
		BUG();
	}

	disagg_print_va_layout(tsk->mm);
	return ret;
}

int cn_notify_exec(struct task_struct *tsk)
{
	return _cn_notify_exec(tsk, 1);
}

int cn_notify_fork(struct task_struct *tsk)
{
	return _cn_notify_exec(tsk, 0);
}
