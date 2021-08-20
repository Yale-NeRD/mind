#ifndef __MN_MEMORY_MANAGEMENT_MODULE_H__
#define __MN_MEMORY_MANAGEMENT_MODULE_H__

#include "../../include/disagg/network_disagg.h"
#include "../../include/disagg/network_rdma_disagg.h"
#include "../../include/disagg/network_fit_disagg.h"
#include "../../include/disagg/fork_disagg.h"
#include "../../include/disagg/exec_disagg.h"
#include "../../include/disagg/exit_disagg.h"
#include "../../include/disagg/mmap_disagg.h"
#include "../../include/disagg/fault_disagg.h"
#include "../../include/disagg/mem_req_disagg.h"

#include <linux/sched/mm.h>
#include <linux/slab.h>
#include <linux/atomic.h>
#include <linux/rwsem.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/rcupdate.h>
#include <linux/mmu_notifier.h>
#include <linux/hmm.h>
#include <linux/percpu_counter.h>
#include <linux/cache.h>
#include <linux/rmap.h>
#include <linux/file.h>
#include <linux/sched/signal.h>
#include <linux/security.h>
#include <linux/mman.h>

#include <asm/pgalloc.h>
#include <asm/mmu_context.h>
#include <asm/processor.h>
#include <asm/atomic.h>
#include <asm/mman.h>

// Memory management used in handlers
int mn_create_dummy_task_mm(u16 sender_id, u16 tgid);
int mn_create_mm_from(u16 sender_id, u16 tgid, struct task_struct *old_tsk, u32 clone_flags);
int mn_update_mm(u16 sender_id, u16 tgid, struct exec_msg_struct* exec_req);
int mn_check_vma(u16 sender_id, u16 tgid, struct exec_msg_struct* exec_req);
int mn_push_data(u16 sender_id, u16 tgid, struct fault_data_struct* data_req);
// int mn_exit(u16 sender_id, u16 tgid, struct task_struct *tsk);

// Memory management functions
// int mn_copy_mm(unsigned long clone_flags, struct task_struct *new_tsk,
//                 struct task_struct *old_tsk);

struct mm_struct *mm_init(struct mm_struct *mm, struct task_struct *p);
	// struct user_namespace *user_ns, struct task_struct *old_tsk);

void mn_mmput(struct mm_struct *mm);

int mn_build_mmap_from_exec(struct mm_struct *mm, struct exec_msg_struct* exec_req);

struct mm_struct *mn_dup_mm(struct task_struct *new_tsk,
            struct task_struct *old_tsk);

void mn_remove_vmas(struct mm_struct *mm);

unsigned long mn_do_mmap(struct task_struct *tsk, unsigned long addr,
			unsigned long len, unsigned long prot, unsigned long flags, vm_flags_t vm_flags,
			unsigned long pgoff, struct file *file);

unsigned long mn_do_brk(struct task_struct *tsk, unsigned long addr);

int mn_do_brk_flags(struct task_struct *tsk, unsigned long addr, 
            unsigned long request, unsigned long flags);

int mn_do_munmap(struct mm_struct *mm, unsigned long start, size_t len);
int mn_munmap(struct mm_struct *mm, unsigned long start, size_t len);

unsigned long mn_do_mremap(struct task_struct *tsk, unsigned long addr, unsigned long old_len,
			unsigned long new_len, unsigned long flags,	unsigned long new_addr);
unsigned long mn_mremap(struct task_struct *tsk, unsigned long addr, unsigned long old_len,
			unsigned long new_len, unsigned long flags,	unsigned long new_addr);

inline unsigned long mn_round_hint_to_min(unsigned long hint);

inline int mn_mlock_future_check(struct mm_struct *mm,
			unsigned long flags, unsigned long len);

unsigned long
mn_get_unmapped_area(struct task_struct *tsk, unsigned long addr, unsigned long len,
		    unsigned long pgoff, unsigned long flags, struct file *file);

unsigned long mn_mmap_region(struct task_struct* tsk, unsigned long addr,
		    unsigned long len, vm_flags_t vm_flags, unsigned long pgoff, struct file *file);

//void proc_caches_init(void);

unsigned long
mn_arch_get_unmapped_area(struct task_struct *, struct file *, unsigned long, 
            unsigned long, unsigned long, unsigned long);

unsigned long
mn_arch_get_unmapped_area_topdown(struct task_struct *, struct file *, unsigned long, 
            unsigned long, unsigned long, unsigned long);

unsigned long
mn_push_data_to_vmas(struct task_struct *tsk, char* data, unsigned long addr, unsigned long len);

int mn_expand_downwards(struct task_struct* tsk, struct vm_area_struct *vma,
				   		unsigned long address);

void DEBUG_print_one_vma(struct vm_area_struct *cur, int i);
void DEBUG_print_exec_vma( struct exec_msg_struct* exec_req);
void DEBUG_print_vma(struct mm_struct *mm);

void DEBUG_print_vma_diff(struct mm_struct *mm, struct exec_msg_struct* exec_req);

/*
 * Search for an unmapped address range.
 *
 * We are looking for a range that:
 * - does not intersect with any VMA;
 * - is contained within the [low_limit, high_limit) interval;
 * - is at least the desired size.
 * - satisfies (begin_addr & align_mask) == (align_offset & align_mask)
 */
#define VM_UNMAPPED_AREA_TOPDOWN 1

unsigned long mn_unmapped_area(struct task_struct *tsk, 
		struct vm_unmapped_area_info *info);

unsigned long mn_unmapped_area_topdown(struct task_struct *tsk, 
		struct vm_unmapped_area_info *info);

inline unsigned long
mn_vm_unmapped_area(struct task_struct *tsk, struct vm_unmapped_area_info *info);

#endif /* __MN_MEMORY_MANAGEMENT_MODULE_H__ */