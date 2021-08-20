#ifndef __MN_MEMORY_MANAGEMENT_MODULE_H__
#define __MN_MEMORY_MANAGEMENT_MODULE_H__

// @linux/sched.h
#define TASK_COMM_LEN 16
#define TASK_FILE_NAME 128
#define TASK_LOG_DIR "./program_logs/"
// @include/uapi/linux/if_ether.h
#define ETH_ALEN 6

/* Spinlock */
#include <pthread.h>
typedef pthread_spinlock_t spinlock_t;

#include "types.h"
#include "request_handler.h"
#include "vm_flags.h"
#include "rbtree_ftns.h"

#include "config.h"
#include "./include/disagg/config.h"
#include "./include/disagg/network_rdma_disagg.h"
#include "./include/disagg/fork_disagg.h"
#include "./include/disagg/exec_disagg.h"
#include "./include/disagg/exit_disagg.h"
#include "./include/disagg/mmap_disagg.h"
#include "./include/disagg/fault_disagg.h"
#include "./include/disagg/mem_req_disagg.h"

#include <unistd.h>
#include <semaphore.h>

// For x86 64 pagesize = 4KB
#define __AC(X, Y) (X##Y)
#define _AC(X, Y) __AC(X, Y)
#define PAGE_SHIFT 12
#define PAGE_SIZE (_AC(1, UL) << PAGE_SHIFT)
#define PAGE_MASK (~(PAGE_SIZE - 1))
#define offset_in_page(p) ((unsigned long)(p) & ~PAGE_MASK)

/* To align the pointer to the (next) page boundary */
#define __ALIGN_KERNEL_MASK(x, mask) (((x) + (mask)) & ~(mask))
#define __ALIGN_KERNEL(x, a) __ALIGN_KERNEL_MASK(x, (typeof(x))(a)-1)
#define ALIGN(x, a) __ALIGN_KERNEL((x), (a))
#define PAGE_ALIGN(addr) ALIGN(addr, PAGE_SIZE)

#define __PHYSICAL_MASK_SHIFT 46
#define __VIRTUAL_MASK_SHIFT 47
#define TASK_SIZE_MAX ((1UL << __VIRTUAL_MASK_SHIFT) - PAGE_SIZE)
#define TASK_SIZE TASK_SIZE_MAX
#define DEFAULT_MAP_WINDOW ((1UL << 47) - PAGE_SIZE)

/* BUG reporting */
#define VM_BUG_ON_VMA(X, Y)                              \
	{if ((X)){                                           \
		fprintf(stderr, "Bug VMA 0x%lx at %s:%d\n",      \
				(unsigned long)(Y), __func__, __LINE__); \
	while (1)                                            \
	{                                                    \
		sleep(1000);                                     \
	}}}

#define VM_BUG_ON(X)                         \
	{if ((X)){                               \
		fprintf(stderr, "Bug VM at %s:%d\n", \
				__func__, __LINE__);         \
	while (1)                                \
	{                                        \
		sleep(1000);                         \
	}}}

#define BUG_ON(X)                         \
	{if ((X)){                            \
		fprintf(stderr, "Bug at %s:%d\n", \
				__func__, __LINE__);      \
	while (1)                             \
	{                                     \
		sleep(1000);                      \
	}}}

#define BUG()                          \
	{fprintf(stderr, "Bug at %s:%d\n", \
			__func__, __LINE__);       \
	while (1)                          \
	{                                  \
		sleep(1000);                   \
	}}

#define VM_WARN_ON(X)	{VM_BUG_ON(X)}

/* Data structures for memory management */
typedef unsigned long vm_flags_t;

struct task_struct;

struct vm_area_struct
{
	struct mm_struct *vm_mm;
	struct vm_area_struct *vm_prev, *vm_next;
	unsigned long vm_start;
	unsigned long vm_end;
	unsigned long vm_flags;
	unsigned long vm_pgoff; // used for stack
	unsigned long rb_subtree_gap;
	unsigned long vm_page_prot;
	unsigned long *vm_file;
	void *vm_private_data;
	struct rb_node vm_rb;
	int map_count;
	int is_aligned;
	int is_test;
};

struct mm_test_vmas
{
	unsigned long data_addr;
	unsigned long meta_addr;
};

struct mm_struct
{
	struct vm_area_struct *mmap;
	struct rb_root mm_rb;
	struct task_struct *owner;
	struct mm_test_vmas testing_vma;

	//locks
	sem_t mmap_sem;
	pthread_spinlock_t page_table_lock;

	// mm related params
	unsigned long mm_users;
	unsigned long mm_count;

	unsigned long map_count;

	unsigned long highest_vm_end;	// VM ranges for new allocation

	unsigned long hiwater_rss; /* High-watermark of RSS usage */
	unsigned long hiwater_vm;  /* High-water virtual memory usage */

	unsigned long total_vm;	 /* Total pages mapped */
	unsigned long locked_vm; /* Pages that have PG_mlocked set */
	unsigned long pinned_vm; /* Refcount permanently increased */
	unsigned long data_vm;	 /* VM_WRITE & ~VM_SHARED & ~VM_STACK */
	unsigned long exec_vm;	 /* VM_EXEC & ~VM_WRITE & ~VM_STACK */
	unsigned long stack_vm;	 /* VM_STACK */
	unsigned long def_flags;
	unsigned long start_code, end_code, start_data, end_data;
	unsigned long start_brk, brk, start_stack;
	unsigned long arg_start, arg_end, env_start, env_end;
	unsigned long mmap_base, mmap_legacy_base;

	// Stat for address translation and access control rules
	unsigned long addr_trans_rule, max_addr_trans_rule;
	unsigned long acc_ctrl_rule, max_acc_ctrl_rule;
};

#define MAX_NUMBER_THREAD_PER_NODE_PER_PROGRAM 	32
#define MAX_NUMBER_THREAD_PER_PROGRAM 			1024

struct task_struct
{
	struct mm_struct *mm;
	char comm[TASK_COMM_LEN];
	u16 tgid;
	u16 pid;
	int primary_node;
	u16 ref_cnt[MAX_NUMBER_COMPUTE_NODE + 1];
	u16 tgids[MAX_NUMBER_COMPUTE_NODE + 1];
};

struct file 
{
	char name[TASK_COMM_LEN];
};

struct vm_unmapped_area_info
{
#define VM_UNMAPPED_AREA_TOPDOWN 1
	unsigned long flags;
	unsigned long length;
	unsigned long low_limit;
	unsigned long high_limit;
	unsigned long align_mask;
	unsigned long align_offset;
};

struct mn_status;
struct memory_node_mapping
{
	int node_id;
	u64 base_addr;	// base address of this VMA mapping (=offset from the base address the memory node to this VMA)
	u64 size;
	struct list_node *node;
	struct mn_status *mn_stat;
};

// Memory management used in handlers
int mn_create_dummy_task_mm(u16 sender_id, u16 tgid, u16 pid);
int mn_create_mm_from(u16 sender_id, u16 tgid, u16 pid, struct task_struct *old_tsk, u32 clone_flags);
int mn_update_mm(u16 sender_id, u16 tgid, struct exec_msg_struct* exec_req);
int mn_check_vma(u16 sender_id, u16 tgid, struct exec_msg_struct* exec_req);

// Memory management functions
// - mm_struct related functions
struct mm_struct *mm_init(struct mm_struct *mm, struct task_struct *p);
void mn_mmput(struct mm_struct *mm);
int mn_build_mmap_from_exec(struct mm_struct *mm, struct exec_msg_struct* exec_req);
struct mm_struct *mn_dup_mm(struct task_struct *new_tsk, struct task_struct *old_tsk);
void mn_remove_vmas(struct mm_struct *mm);
void mn_remove_vmas_exec(struct mm_struct *mm);

// - mmap related functions
#define MMAP_CACHE_DIR_POPULATION_FLAG 0x1
unsigned long mn_do_mmap(struct task_struct *tsk, unsigned long addr,
						 unsigned long len, unsigned long prot, unsigned long flags, vm_flags_t vm_flags,
						 unsigned long pgoff, unsigned long *file, int nid);

unsigned long _mn_do_mmap(
	struct task_struct *tsk, unsigned long addr,
	unsigned long len, unsigned long prot,
	unsigned long flags, vm_flags_t vm_flags,
	unsigned long pgoff, unsigned long *file,
	int is_test);

unsigned long mn_mmap_region(
	struct task_struct *tsk, unsigned long addr,
	unsigned long len, vm_flags_t vm_flags, unsigned long pgoff,
	unsigned long *file, int fixed_addr, int is_test);

unsigned long mn_round_hint_to_min(unsigned long hint);

unsigned long
mn_get_unmapped_area(struct task_struct *tsk, unsigned long addr, unsigned long len,
			unsigned long pgoff, unsigned long flags, unsigned long *file);

// - brk
unsigned long mn_do_brk(struct task_struct *tsk, unsigned long addr);

int mn_do_brk_flags(struct task_struct *tsk, unsigned long addr, 
            unsigned long request, unsigned long flags);

// - munmap
int mn_do_munmap(struct mm_struct *mm, unsigned long start, size_t len);
int mn_munmap(struct mm_struct *mm, unsigned long start, size_t len);

// - mremap
unsigned long mn_do_mremap(struct task_struct *tsk, unsigned long addr, unsigned long old_len,
			unsigned long new_len, unsigned long flags,	unsigned long new_addr);
unsigned long mn_mremap(struct task_struct *tsk, unsigned long addr, unsigned long old_len,
			unsigned long new_len, unsigned long flags,	unsigned long new_addr);

// == VMA management == //
// Accounting
unsigned long vm_start_gap(struct vm_area_struct *vma);
void vm_stat_account(struct mm_struct *mm, vm_flags_t flags, long npages);
int mn_mlock_future_check(struct mm_struct *mm,
			unsigned long flags, unsigned long len);
unsigned long vma_pages(struct vm_area_struct *vma);
// Debug print functions
u64 DEBUG_count_anon_vma(struct mm_struct *mm);
void DEBUG_print_one_vma(struct vm_area_struct *cur, int i);
void DEBUG_print_exec_vma( struct exec_msg_struct* exec_req);
void DEBUG_print_vma(struct mm_struct *mm);
void DEBUG_print_vma_diff(struct mm_struct *mm, struct exec_msg_struct* exec_req);

// Utility functions
unsigned long get_next_pow_of_two_addr(u64 start, u64 size);
unsigned int get_prefix_from_size(unsigned long size);
uint64_t get_full_virtual_address(uint16_t tgid, unsigned long addr);
unsigned long get_pow_of_two_req_size(unsigned long size);
void add_new_addr_trans_rule(unsigned long addr, unsigned long size,
							 struct memory_node_mapping *mnmap, unsigned int tgid,
							 unsigned int permission, struct vm_area_struct *vma);
void modify_addr_trans_rule(unsigned long addr, unsigned long size,
							struct memory_node_mapping *mnmap, unsigned int tgid,
							unsigned int permission);
void del_addr_trans_rule(uint64_t addr, unsigned long size, struct vm_area_struct *vma);

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
unsigned long vm_end_gap(struct vm_area_struct *vma);

unsigned long mn_arch_get_unmapped_area(struct task_struct *, unsigned long *, unsigned long,
										unsigned long, unsigned long, unsigned long);

unsigned long
mn_arch_get_unmapped_area_topdown(struct task_struct *, unsigned long *, unsigned long, 
            unsigned long, unsigned long, unsigned long);

unsigned long mn_unmapped_area(struct task_struct *tsk, 
		struct vm_unmapped_area_info *info);

unsigned long mn_unmapped_area_topdown(struct task_struct *tsk, 
		struct vm_unmapped_area_info *info);

unsigned long
mn_vm_unmapped_area(struct task_struct *tsk, struct vm_unmapped_area_info *info);

/*
 * Talk to memory node: control plane routines (for memory initialization)
 */
int get_node_idx(int mn_node_id, unsigned long size);
int zeros_mem_range(int mn_node_idx, unsigned long offset, unsigned long size);

int get_virtual_address(int mn_node_id, unsigned long addr, unsigned long size,
						unsigned int tgid, struct memory_node_mapping *mnmap,
						unsigned int permission, struct vm_area_struct *vma);

unsigned long get_available_virtual_address_lock(unsigned long size, spinlock_t **mn_lock_ptr);

int read_data_from_mn(struct memory_node_mapping *mnmap, unsigned long offset, unsigned long size, void *buf);

int write_data_to_mn(struct memory_node_mapping *mnmap, unsigned long offset, unsigned long size, void *buf);

int send_meminit_request(struct dest_info *mnmap,
						 unsigned long offset,
						 unsigned long len);

int send_memcpy_request(struct dest_info *mnmap,
						unsigned long old_off, unsigned long new_off,
						unsigned long copy_len);

#endif /* __MN_MEMORY_MANAGEMENT_MODULE_H__ */
