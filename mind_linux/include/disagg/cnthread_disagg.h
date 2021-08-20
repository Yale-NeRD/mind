#ifndef __CNTHREAD_DISAGGREGATION_H__
#define __CNTHREAD_DISAGGREGATION_H__

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/in.h>

#include <linux/kthread.h>
#include <linux/sched/signal.h>
#include <disagg/network_disagg.h>

#include <asm/signal.h>
#include <disagg/cache_config.h>

// in number of pages: 32768 pages = 128 MB, 131072 = 512 MB
// (106240)    // 0.415 GB
// (109445)    // 0.4175 GB 
// (131072)    // 0.5 GB
// (471204)    // 1.7975 GB 
// (524288)    // 2 GB (for sharing test)
// (2097152)   // 8 GB
#define CNTHREAD_MAX_CACHE_BLOCK_NUMBER (131072)
#define CNTHREAD_HIGH_CACHE_PERSSURE    0.9
#define CNTHREAD_CACHED_PERSSURE        0.85
#define CNTHREAD_TRY_EVICTION_AGAIN     1
#define CNTHREAD_FETCH_PAGE_BATCH       (CNTHREAD_MAX_CACHE_BLOCK_NUMBER / 100) // 1 %
#define CNTHREAD_CACHELINE_SIZE_IN_PAGES (CACHELINE_MAX_SIZE/PAGE_SIZE)         // 2 MB
#define CNTHREAD_CACHELINE_OVERSUB (CNTHREAD_MAX_CACHE_BLOCK_NUMBER / 16)
#define NUM_HASH_BUCKETS 65536
#define NUM_HASH_WAIT_LIST 256
#define CNTHREAD_LOCAL_POLLING_SKIP_COUNTER 10000000
#define CNTHREAD_CACHLINE_MASK (~(u64)((CNTHREAD_CACHELINE_SIZE_IN_PAGES * PAGE_SIZE) - 1))
#define CNTHREAD_BATCH_IN_CACHELINE (CACHELINE_MAX_SIZE / PAGE_SIZE)
#define CNTHREAD_HEARTBEAT_IN_MS 1000
#define CNTHREAD_DEBUG_BUFFER_PRINT_RANGE (16)
#define PG_FAULT_HELPER_CPU (DISAGG_QP_NUM_INVAL_DATA - 2)
#define ROCE_FIN_ACK    // send final ack over ROCE

#define EVICT_HANDLER_CPU (DISAGG_NUM_CPU_CORE_IN_COMPUTING_BLADE - 1)
#define INVAL_HANDLER_CPU (DISAGG_NUM_CPU_CORE_IN_COMPUTING_BLADE - 2)
struct cnthread_stat
{
    int read_count;
    int write_count;
};

struct cnthread_inval_pte
{
    pte_t *pte;
    spinlock_t *ptl;
    int need_push_back;
    int skip;
};

struct cnthread_handler_data
{
    int                  *init_stage;
    struct cnthread_stat *stat;
};

// NOT USED NOW, left here just for compilation
// Cache state for local version of cache coherence protocol
#define CACHE_STATE_IS 0
#define CACHE_STATE_IM 1
#define CACHE_STATE_SM 2
#define CACHE_STATE_SI 3
#define CACHE_STATE_MI 4
#define CACHE_STATE_II 5
#define CACHE_STATE_S 6
#define CACHE_STATE_M 7
#define CACHE_STATE_I 8
#define CACHE_STATE_MD 9
#define CACHE_STATE_SUCCESS 0xf
#define CACHE_STATE_FAIL    0x0
#define CACHE_STATE_INV_ACK 0xd

enum
{
    IS_PAGE_UNUSED = 0,     // not used
    IS_PAGE_USED = 1,       // currently used
    IS_PAGE_RECEIVED = 2,   // received, so will become IS_PAGE_USED
};

enum
{
    MSG_OVER_RDMA_INV_REQ = 0,
    MSG_OVER_RDMA_ACK = 1,
    MSG_OVER_RDMA_SELF_INV = 2,
};

enum
{
    TRY_INVALIDATION = 1,
    EVICT_BUT_RETRYING = 2,
    EVICT_FORCED = 3,
};

enum
{
    PROFILE_CNTHREAD_INV_ACK_SERV_FROM_EVICT = 0,
    PROFILE_CNTHREAD_INV_ACK_SERV_FROM_INVAL = 1,
    PROFILE_CNTHREAD_INV_ACK_SERV_FROM_PRMPT = 2,
};

struct cnthread_cacheline;
struct cnthread_req
{
    // unsigned long addr;
    struct vm_area_struct       *vma;
    pte_t                       *pte_ptr;
    struct page                 *kpage;
    unsigned long               dma_addr;
    // unsigned long           pfn;
    void                        *kmap;
    atomic_t                    is_used;
    struct cnthread_cacheline   *cacheline;
    int                         page_idx;
    spinlock_t                  pgfault_lock;
    struct list_head            node;
};

struct cnthread_cacheline
{
    unsigned int tgid;
    unsigned long addr;
    struct mm_struct        *mm;
    struct list_head        node;
    struct hlist_node       hnode;
    struct cnthread_req     *pages[CNTHREAD_CACHELINE_SIZE_IN_PAGES];
    atomic_t                state;
    spinlock_t              evict_lock;
    spinlock_t              on_going_lock;
    struct rw_semaphore     access_sem; // down write for eviction, down read for add new pages
    atomic_t                used_page; // must be protected by evict_lock
    int                     ownership; // exclusive ownership: 1 for true, 0 for false
};


struct cnthread_rdma_msg_ctx
{
    u64 fva, vaddr;
    u32 ret, state, ip_val, qp_val, sharer, psn;
    u16 pid, requester;
};

struct cnthread_inv_msg_ctx
{
    struct socket *sk;
    char *rdma_buf;
    char inval_buf[CACHELINE_ROCE_HEADER_LENGTH];
    char dummy_buf[CACHELINE_ROCE_HEADER_LENGTH];
    int recv_len;
    struct sockaddr_in addr_in;
    u32 original_qp;
    struct cnthread_rdma_msg_ctx rdma_ctx;
    struct cnthread_cacheline *cnline_ptr;
    int is_locked;
    struct list_head node;
    unsigned long last_update;
    int is_target_data, remove_data, is_invalid;
};

enum
{
    RET_CNLINE_LOCKED = 0,
    RET_CNLINE_NOT_LOCKED = 1,
    RET_NO_CNLINE = 2,
};

void disagg_cnth_end_init_phase(void);
spinlock_t *get_per_core_lock(int cid);
unsigned long get_dummy_page_dma_addr(int cpu_id);
void *get_dummy_page_buf_addr(int cpu_id);
unsigned long get_dummy_inv_page_dma_addr(void);

// Functions to get / release preallocated pages
struct cnthread_req *cnthread_get_new_page(unsigned int tgid, unsigned long addr, struct mm_struct *mm, int *was_cacheline_exist);
struct cnthread_req *cnthread_get_page(unsigned int tgid, unsigned long addr, struct mm_struct *mm, int *was_cacheline_exist);
void cnthread_put_page(struct cnthread_req *cnreq);
void cnthread_create_owner_cacheline(unsigned int tgid, unsigned long addr, struct mm_struct *mm);
void cnthread_set_page_received(struct cnthread_req *cnreq);
int cnthread_rollback_page_received(struct cnthread_req *cnreq);

// Functions to add pte to the cache list
inline int cnthread_add_pte_to_list_with_cnreq(
    pte_t * pte, unsigned long address, struct vm_area_struct *vma,
    struct cnthread_req *cnreq, int new_page);
inline u64 generate_full_addr(u16 tgid, unsigned long addr);
int is_owner_address(unsigned int tgid, unsigned long addr);
int cnthread_evict_one(int threshold, int high_threshold);
int cnthread_check_free_space(void);
int cnthread_delete_from_list(u16 tgid, unsigned long address);
int cnthread_delete_from_list_no_lock(u16 tgid, unsigned long address);
int cnthread_delete_all_request(u16 tgid);
int cnthread_flush_all_request(u16 tgid);
int cnthread_clean_up_non_existing_entry(u16 tgid, struct mm_struct *mm);

// DEBUG functions
int DEBUG_try_evict(u32 tgid, u64 address);
void DEBUG_trigger_evict(unsigned int tgid, unsigned int addr);

struct cache_waiting_node
{
    unsigned int tgid;
    unsigned long addr;
    atomic_t ack_counter;
    struct hlist_node hnode;
    atomic_t target_counter;
    atomic_t unlock_requested;
    char *ack_buf;
};

struct cache_waiting_node *add_waiting_node(unsigned int tgid, unsigned long addr, struct cnthread_req *cnreq);
int wait_until_counter(struct cache_waiting_node *node, spinlock_t *pte_lock, struct rw_semaphore *mmap_sem, struct cnthread_req *cnreq);
void cancel_waiting_for_nack(struct cache_waiting_node *w_node);
void try_invalidation_lookahead(int from_inv, int lookahead);
void try_invalidation_processing_from_page_fault_handler(struct cnthread_req *cnreq);
int cnthread_is_pre_owned(struct cnthread_req *cnreq, unsigned int tgid, unsigned long address);

#endif  /* __CNTHREAD_DISAGGREGATION_H__ */
