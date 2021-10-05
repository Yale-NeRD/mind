#include <disagg/cnthread_disagg.h>
#include <disagg/exec_disagg.h>
#include <disagg/fault_disagg.h>
#include <disagg/profile_points_disagg.h>
#include <disagg/network_disagg.h>
#include <disagg/network_fit_disagg.h>
#include <disagg/print_disagg.h>
#include <disagg/config.h>

#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/uaccess.h>		/* probe_kernel_address()	*/
#include <linux/mempolicy.h>
#include <linux/mmu_notifier.h>
#include <linux/inet.h>
#include <linux/list.h>

#include <asm/tlb.h>
#include <asm/pgtable_types.h>
#include <asm/pgtable.h>
#include <asm/page_types.h>
#include <asm/tlbflush.h>
#include <asm/cacheflush.h>
#include <asm/byteorder.h>

static spinlock_t cnthread_lock;
static spinlock_t free_page_put_lock;   // Aadd to the list
static spinlock_t free_page_get_lock;   // Get one from the list
static spinlock_t free_cacheline_put_lock;   // Add to the list
static spinlock_t free_cacheline_get_lock;   // Get one from the list
static spinlock_t cache_waiting_lock;
static spinlock_t hash_list_lock;
static spinlock_t cnthread_udp_lock;
static spinlock_t cnthread_udp_send_lock;
static spinlock_t cnthread_inval_send_ack_lock[DISAGG_NUM_CPU_CORE_IN_COMPUTING_BLADE];
static spinlock_t cnthread_evict_range_lock;
static spinlock_t cn_fh_per_core_lock[DISAGG_NUM_CPU_CORE_IN_COMPUTING_BLADE];
static spinlock_t cnthread_inval_ack_lock;
static spinlock_t cnthread_inval_buf_write_lock;
static spinlock_t cnthread_inval_req_list_lock;
static spinlock_t cnthread_pgfault_helper_lock[PG_FAULT_HELPER_CPU];

static LIST_HEAD(cn_handler_lru_list);
static LIST_HEAD(cn_free_page_list);
static LIST_HEAD(cn_free_cacheline_list);
static LIST_HEAD(cn_inval_req_list);

static struct hlist_head lru_list_hashtable[NUM_HASH_BUCKETS];
static struct hlist_head cn_cache_waiting_list[NUM_HASH_WAIT_LIST];

static atomic_t cnthread_list_counter;
static atomic_t cnthread_evict_counter;
static atomic_t cn_free_page_counter;
static atomic_t cn_free_cacheline_counter;
static atomic_long_t cn_total_eviction_counter;
static atomic_t cnthread_inv_req_counter;
static atomic_t cnthread_buf_head_inv_splitter;
static atomic_t cnthread_pgfault_helper_idx;

static int _threshold = (int)(CNTHREAD_CACHED_PERSSURE * (float)CNTHREAD_CACHELINE_OVERSUB);
static int _high_threshold = (int)(CNTHREAD_HIGH_CACHE_PERSSURE * (float)CNTHREAD_MAX_CACHE_BLOCK_NUMBER / CNTHREAD_CACHELINE_SIZE_IN_PAGES);
static int _page_free_threshold = (int)((float)(1.0 - CNTHREAD_CACHED_PERSSURE) * (float)CNTHREAD_MAX_CACHE_BLOCK_NUMBER);
static int _page_high_free_threshold = (int)((float)(1.0 - CNTHREAD_HIGH_CACHE_PERSSURE) * (float)CNTHREAD_MAX_CACHE_BLOCK_NUMBER);

// Buffer for RDMA-based invalidation
static unsigned char* base_inval_buf[DISAGG_QP_NUM_INVAL_BUF] = {NULL};
static unsigned int inval_buf_head = 0;
static unsigned int inval_buf_ack_head[DISAGG_QP_NUM_INVAL_BUF] = {0};
static spinlock_t cnthread_inval_head_buf_lock[DISAGG_QP_NUM_INVAL_BUF];

int cnthread_handler(void *data);
int cache_ack_handler(void *data);
static int check_inval_req_list_and_try(int from_inv, struct cnthread_cacheline *cnline);
#ifndef MIND_USE_TSO
static void _cnthread_send_inval_ack(u16 tgid, unsigned long vaddr, char *inval_buf);
#endif

static int __cn_handler_init_phase;

// Functions for each page
static __always_inline void _add_page_to_free_list(struct cnthread_req *cnreq)
{
    if (unlikely(cnreq->node.next || cnreq->node.prev))
    {
        printk(KERN_ERR "ERROR:: already in the list!! cnreq: 0x%lx\n", (unsigned long)cnreq);
        return;
    }
    atomic_inc(&cn_free_page_counter);
    list_add(&cnreq->node, &cn_free_page_list);
}

void clean_cnthread_req(struct cnthread_req *cnreq)
{
    if (unlikely(!cnreq || !cnreq->kpage))
    {
        printk(KERN_WARNING "Null struct cnthread_req *\n");
        BUG();
    }
    cnreq->vma = NULL;
    cnreq->pte_ptr = NULL;
    atomic_set(&cnreq->is_used, IS_PAGE_UNUSED);
    atomic_set(&cnreq->kpage->_mapcount, 0); // idle: 0 -> mapped: 1
    atomic_set(&cnreq->kpage->_refcount, 2); // idle: 2 -> mapped: 3
    if (cnreq->cacheline && (cnreq->page_idx >= 0))
        cnreq->cacheline->pages[cnreq->page_idx] = NULL;
    cnreq->cacheline = NULL;
    cnreq->page_idx = -1;
    smp_wmb();
    spin_lock_init(&cnreq->pgfault_lock);   // Simply set to unlocked
}

void cnthread_put_page(struct cnthread_req *cnreq)
{
    if (unlikely(!cnreq || !cnreq->kpage))
    {
        printk(KERN_WARNING "Null struct cnthread_req *\n");
        BUG();
    }
    // Put it back to the free list
    // FIXME: do we need to make the page clean (as zeros)?
    spin_lock(&free_page_put_lock);
    clean_cnthread_req(cnreq);
    _add_page_to_free_list(cnreq);
    spin_unlock(&free_page_put_lock);
}

static void cnthread_put_page_no_lock(struct cnthread_req *cnreq)
{
    if (unlikely(!cnreq || !cnreq->kpage))
    {
        printk(KERN_WARNING "Null struct cnthread_req **\n");
        BUG();
    }
    clean_cnthread_req(cnreq);
    _add_page_to_free_list(cnreq);
}

static struct cnthread_req *_cnthread_get_page(void)
{
    struct cnthread_req *cnreq = NULL;
    int put_lock = 0;
    spin_lock(&free_page_get_lock);
    if (atomic_read(&cn_free_page_counter) == 1)
    {
        spin_lock(&free_page_put_lock);
        put_lock = 1;
    }
    cnreq = (struct cnthread_req *)container_of(cn_free_page_list.prev, struct cnthread_req, node);
    list_del(&cnreq->node);
    cnreq->node.next = cnreq->node.prev = NULL;
    atomic_dec(&cn_free_page_counter);
    smp_wmb();
    if (put_lock)
    {
        spin_unlock(&free_page_put_lock);
    }
    spin_unlock(&free_page_get_lock);
    return cnreq;
}

static void __always_inline init_cnreq(struct cnthread_req *cnreq, struct page *page)
{
    cnreq->kpage = page;
    cnreq->cacheline = NULL;
    cnreq->node.next = cnreq->node.prev = NULL;
    clean_cnthread_req(cnreq);
    spin_lock_init(&cnreq->pgfault_lock);
}

// Functions for cache lines
static void clean_cnthread_cacheline(struct cnthread_cacheline *cnline)
{
    init_rwsem(&cnline->access_sem);
    cnline->addr = 0;
    cnline->tgid = 0;
    cnline->mm = NULL;
    cnline->ownership = 0;
    memset(cnline->pages, 0, sizeof(struct cnthread_req *) * CNTHREAD_CACHELINE_SIZE_IN_PAGES);
    atomic_set(&cnline->used_page, 0);
}

static void init_cnthread_cacheline(struct cnthread_cacheline *cnline)
{
    clean_cnthread_cacheline(cnline);
    spin_lock_init(&cnline->evict_lock);
    spin_lock_init(&cnline->on_going_lock);
}

static void __always_inline add_cacheline_to_free_list_no_lock(struct cnthread_cacheline *cnline)
{
    int j = 0;
    struct cnthread_req *cnreq = NULL;
    for (j = 0; j < CNTHREAD_CACHELINE_SIZE_IN_PAGES; j++)
    {
        cnreq = cnline->pages[j];
        cnline->pages[j] = NULL;    // This page cannot be found anymore
        smp_wmb();
        if (cnreq)
        {
            if ((atomic_read(&cnreq->is_used) == IS_PAGE_USED))
            {
                cnthread_put_page(cnreq);
            }
            else
            {
                // There can be on-going cnthread_put_page in page fault handler
                // So, simply detach the cnline
                cnreq->cacheline = NULL; // since we are removing this cacheline);
                // => now cleaning up is page fault handler's reponsibility
            }
        }
    }
    clean_cnthread_cacheline(cnline);
    list_add_tail(&cnline->node, &cn_free_cacheline_list);
    atomic_inc(&cn_free_cacheline_counter);
}

static void __always_inline add_cacheline_to_free_list(struct cnthread_cacheline *cnline)
{
    spin_lock(&free_cacheline_put_lock);
    add_cacheline_to_free_list_no_lock(cnline);
    spin_unlock(&free_cacheline_put_lock);
}

spinlock_t *get_per_core_lock(int cid)
{
    return &cn_fh_per_core_lock[cid];
}
EXPORT_SYMBOL(get_per_core_lock);

void disagg_cn_thread_init(void)
{
    // unsigned long pfn;
    struct page *page = NULL, *first_page = NULL, *last_page = NULL;
    long i = 0, j = 0;
    struct cnthread_handler_data *data = NULL;
    spin_lock_init(&cnthread_lock);
    spin_lock_init(&free_cacheline_put_lock);
    spin_lock_init(&free_cacheline_get_lock);
    spin_lock_init(&cache_waiting_lock);
    spin_lock_init(&hash_list_lock);
    spin_lock_init(&cnthread_udp_lock);
    spin_lock_init(&cnthread_udp_send_lock);
    spin_lock_init(&cnthread_evict_range_lock);
    spin_lock_init(&cnthread_inval_ack_lock);
    spin_lock_init(&cnthread_inval_buf_write_lock);
    spin_lock_init(&cnthread_inval_req_list_lock);
    spin_lock_init(&free_page_put_lock);
    spin_lock_init(&free_page_get_lock);
    __cn_handler_init_phase = 0;
    atomic_set(&cnthread_list_counter, 0);
    atomic_set(&cnthread_evict_counter, 0);
    atomic_set(&cn_free_page_counter, 0);
    atomic_set(&cn_free_cacheline_counter, 0);
    atomic_long_set(&cn_total_eviction_counter, 0);
    atomic_set(&cnthread_inv_req_counter, 0);
    atomic_set(&cnthread_buf_head_inv_splitter, 0);
    atomic_set(&cnthread_pgfault_helper_idx, 0);

    hash_init(lru_list_hashtable);
    hash_init(cn_cache_waiting_list);

    for (i = 0; i < DISAGG_NUM_CPU_CORE_IN_COMPUTING_BLADE; i++)
    {
        spin_lock_init(&cn_fh_per_core_lock[i]);
        spin_lock_init(&cnthread_inval_send_ack_lock[i]);
    }

    for (i = 0; i < DISAGG_QP_NUM_INVAL_BUF; i++)
    {
        inval_buf_ack_head[i] = 0;  // start from 0
        spin_lock_init(&cnthread_inval_head_buf_lock[i]);
    }

    for (i = 0; i < PG_FAULT_HELPER_CPU; i++)
    {
        spin_lock_init(&cnthread_pgfault_helper_lock[i]);
    }

    data = kzalloc(sizeof(*data), GFP_KERNEL);
    if (!data)
    {
        pr_err("Cannot start cnthread handler daemon!\n");
    }

    data->init_stage = &__cn_handler_init_phase;
    smp_wmb();

    // Initialize cache regions (=cacheline structures)
    for (i = 0; i < CNTHREAD_CACHELINE_OVERSUB; i++)
    {
        struct cnthread_cacheline *cacheline = kmalloc(sizeof(struct cnthread_cacheline),
                                                       GFP_KERNEL);
        if (!cacheline)
        {
            pr_err("Disagg_Cached: Cannot initialize cacheline: %ld\n", i);
            BUG();
        }
        init_cnthread_cacheline(cacheline);
        add_cacheline_to_free_list(cacheline);
    }

    // Initialize pages (=local memory as a cache for disaggregated memory)
    for (j = 0; j < CNTHREAD_MAX_CACHE_BLOCK_NUMBER; j++)
    {
        struct cnthread_req *cnreq = kzalloc(sizeof(struct cnthread_req), GFP_KERNEL);
        if (!cnreq)
        {
            pr_err("Disagg_Cached: Cannot initialize page: %ld\n", i);
            BUG();
        }
        page = alloc_page_vma((~__GFP_RECLAIM) & (GFP_USER | __GFP_ZERO), NULL, 0); // It will increase ref count
        if (unlikely(!page))
        {
            pr_err("Cannot allocate new page [%ld, %ld]\n", i, j);
            BUG();
        }
        init_cnreq(cnreq, page);  // Initial assignement of page
        cnthread_put_page_no_lock(cnreq);   // It is in the initialization, we just skip locking
        if (unlikely(j == 0))
            first_page = page;
        if (unlikely(j == CNTHREAD_MAX_CACHE_BLOCK_NUMBER - 1))
            last_page = page;
    }
    printk(KERN_DEFAULT "Disagg_Cached: Metadata for software cache has been initialized [%ld entries]\n", i);
    printk(KERN_DEFAULT "Disagg_Cached: Assigned page range-kernel VA [0x%lx - 0x%lx]\n",
           (unsigned long)first_page, (unsigned long)last_page);

    init_test_program_thread_cnt();

    kthread_run((void *)cnthread_handler, (void *)data, "disagg_evictd");
    kthread_run((void *)cache_ack_handler, (void *)data, "disagg_invald");
}

static int pin_current_thread_to_cpu(int cpu)
{
    struct task_struct *p = current;
    set_cpus_allowed_ptr(p, get_cpu_mask(cpu));
    set_cpu_active(cpu, false);
    return 0;
}

int pin_current_thread(int cpu)
{
    int res;
    res = pin_current_thread_to_cpu(cpu);

    if (!res)
    {
        pr_info("%s - cpu %d(+1) pinned\n", __func__, cpu);
    }
    else
    {
        pr_info("%s - cpu %d(+1) cannot be pinned\n", __func__, cpu);
    }

    return res;
}

static int is_cache_ack_pinned = 0;

void disagg_cnth_end_init_phase()
{
    __cn_handler_init_phase = 1;
    printk(KERN_DEFAULT "Disagg_Cached: Initial phase has been ended\n");
}
EXPORT_SYMBOL(disagg_cnth_end_init_phase);

uint64_t size_index_to_size(uint16_t s_idx)
{
    return ((uint64_t)DYN_MIN_DIR_SIZE) << s_idx;
}

static u16 hash_ftn(u16 tgid, u64 addr)
{
    return ((tgid + addr) >> CACHELINE_MIN_SHIFT) & 0xffff;
}

static u16 hash_ftn_wait_list(u16 tgid, u64 addr)
{
    return ((tgid + addr) >> 12) & 0xff;
}

int is_transient_state(int state)
{
    return state == CACHE_STATE_IS ||
           state == CACHE_STATE_IM ||
           state == CACHE_STATE_SM ||
           state == CACHE_STATE_SI ||
           state == CACHE_STATE_MI ||
           state == CACHE_STATE_II;
}

static __always_inline struct cnthread_cacheline *find_cacheline_no_lock(unsigned int tgid, unsigned long addr)
{
    struct cnthread_cacheline *cnline = NULL;
    addr &= CNTHREAD_CACHLINE_MASK;

    hash_for_each_possible(lru_list_hashtable, cnline, hnode, hash_ftn(tgid, (u64)addr))
    {
        if (cnline->tgid == tgid && cnline->addr == addr)
            return cnline;
    }
    return NULL;
}

static __always_inline int get_page_index(struct cnthread_cacheline *cnline, unsigned long addr)
{
    return (addr - cnline->addr) / PAGE_SIZE;
}

static __always_inline struct cnthread_req *find_page_from_cacheline(struct cnthread_cacheline *cnline, unsigned long addr)
{
    int idx = get_page_index(cnline, addr);
    if (idx >= 0 && idx < CNTHREAD_CACHELINE_SIZE_IN_PAGES)
        return cnline->pages[idx];
    else
        return NULL;
}

static __always_inline struct cnthread_req *find_page_no_lock(unsigned int tgid, unsigned long addr)
{
    struct cnthread_cacheline *cnline = find_cacheline_no_lock(tgid, addr);
    if (cnline)
    {
        return find_page_from_cacheline(cnline, addr);
    }
    return NULL;
}

static struct cnthread_cacheline *_cnthread_allocate_cacheline(
    unsigned int tgid, unsigned long addr, struct mm_struct *mm)
{
    int locked = 0;
    struct cnthread_cacheline *cnline;
    if (unlikely(atomic_read(&cn_free_cacheline_counter) <= 1))
    {
        spin_lock(&free_cacheline_put_lock);
        locked = 1;
    }
    cnline = container_of(cn_free_cacheline_list.next, struct cnthread_cacheline, node);
    list_del(&cnline->node);
    if (unlikely(locked))
    {
        spin_unlock(&free_cacheline_put_lock);
    }
    atomic_dec(&cn_free_cacheline_counter);
    spin_unlock(&free_cacheline_get_lock);

    // initialize cacheline and add to the hash and lru list
    cnline->addr = addr & CNTHREAD_CACHLINE_MASK;
    cnline->tgid = tgid;
    cnline->mm = mm;
    atomic_set(&cnline->used_page, 0);
    pr_cache("Assigned new cacheline: tgid[%u] addr[0x%llx] mm[0x%lx]\n",
                tgid, (addr & CNTHREAD_CACHLINE_MASK), (unsigned long)mm);
    // by adding to the hash, now this cacheline is visible from cache eviction handler and page fault handler
    atomic_inc(&cnthread_list_counter);
    hash_add(lru_list_hashtable, &cnline->hnode, hash_ftn(tgid, (u64)cnline->addr));
    list_add(&cnline->node, &cn_handler_lru_list);
    return cnline;
}

// @was_cacheline_exist: return 1 if there is existing cacheline and page, return 0 otherwise.
static struct cnthread_req *_cnthread_get_new_page(unsigned int tgid, unsigned long addr, struct mm_struct *mm,
                                                   int new_page, int *was_cacheline_exist)
{
    struct cnthread_req *free_req = NULL;
    struct cnthread_cacheline *cnline;
    (void)new_page;

    if (was_cacheline_exist)
        (*was_cacheline_exist) = 0;

retry_get_free_page:
    // Check there is already populated cacheline
    spin_lock(&cnthread_lock);
    spin_lock(&hash_list_lock);
    cnline = find_cacheline_no_lock(tgid, addr);
    if (cnline)
    {
        free_req = find_page_no_lock(tgid, addr);
        if (free_req)   // It already have an allocated page
        {
            *was_cacheline_exist = atomic_read(&free_req->is_used);
            if (spin_trylock(&cnline->evict_lock)) // Not being evicted
            {
                if (spin_trylock(&free_req->pgfault_lock)) // Not being evicted
                {
                    // Check consistency
                    if (unlikely((!free_req->cacheline) || (free_req->cacheline->tgid != tgid) ||
                                 (free_req->cacheline->addr != (addr & CNTHREAD_CACHLINE_MASK)) ||
                                 (free_req->cacheline->mm != mm)))
                    {
                        pr_info_ratelimited("Given: tgid[%u] addr[0x%llx] mm[0x%lx] is_used[%d] <-> found: tgid[%u] addr[0x%lx] mm[0x%lx]\n",
                                            tgid, (addr & CNTHREAD_CACHLINE_MASK), (unsigned long)mm,
                                            atomic_read(&free_req->is_used),
                                            free_req->cacheline ? free_req->cacheline->tgid : (unsigned int)-1,
                                            free_req->cacheline ? free_req->cacheline->addr : 0,
                                            free_req->cacheline ? (unsigned long)cnline->mm : 0);
                        spin_unlock(&hash_list_lock);
                        spin_unlock(&cnthread_lock);
                        spin_unlock(&free_req->pgfault_lock);
                        spin_unlock(&cnline->evict_lock);
                        return NULL;
                    }
                    spin_unlock(&cnthread_lock);
                    spin_unlock(&hash_list_lock);
                    spin_unlock(&cnline->evict_lock);
                    return free_req;
                }
                spin_unlock(&cnline->evict_lock);
            }
            spin_unlock(&cnthread_lock);
            spin_unlock(&hash_list_lock);
            return NULL; // Under eviction or parallel access to the same cacheline
        }
    }
    // We hold cnthread_lock, hast_list_lock here!!

    // Find new cacheline
    spin_lock(&free_cacheline_get_lock);
    if (unlikely(list_empty(&cn_free_cacheline_list) || list_empty(&cn_free_page_list)))
    {
        // cnthread_out_of_cache();    // this will kill itself
        printk(KERN_WARNING "Out of DRAM software cache!! (free page: %d [%d], regions: %d[%d])\n",
               atomic_read(&cn_free_page_counter), list_empty(&cn_free_cacheline_list), 
               atomic_read(&cn_free_cacheline_counter), list_empty(&cn_free_page_list));
        spin_unlock(&free_cacheline_get_lock);
        spin_unlock(&hash_list_lock);
        spin_unlock(&cnthread_lock);
        msleep(100);
        goto retry_get_free_page;
    }
    else
    {
        if (!cnline)
            cnline = _cnthread_allocate_cacheline(tgid, addr, mm); // Release free_cacheline_get_lock
        else
            spin_unlock(&free_cacheline_get_lock);
        if (unlikely(!cnline))
        {
            BUG();
        }
        if (spin_trylock(&cnline->evict_lock)) // Not being evicted
        {
            free_req = _cnthread_get_page();
            if (unlikely(!free_req))
            {
                spin_unlock(&cnline->evict_lock);
                BUG();
            }
            spin_lock(&free_req->pgfault_lock);
            free_req->cacheline = cnline;
            barrier();
            free_req->page_idx = get_page_index(cnline, addr);
            cnline->pages[free_req->page_idx] = free_req;
            atomic_set(&free_req->is_used, IS_PAGE_UNUSED);
            smp_wmb();
            spin_unlock(&cnline->evict_lock);
        }
        spin_unlock(&hash_list_lock);
        spin_unlock(&cnthread_lock);
    }
    // printk(KERN_DEFAULT "CNTHREAD: New free page [req: 0x%lx] [page: 0x%lx] [rCnt: %d] [mCnt: %d]\n",
    //        (unsigned long)free_req, (unsigned long)free_req->kpage,
    //        atomic_read(&free_req->kpage->_refcount), 
    //        atomic_read(&free_req->kpage->_mapcount));
    return free_req;
}

struct cnthread_req *cnthread_get_new_page(unsigned int tgid, unsigned long addr, struct mm_struct *mm, int *was_cacheline_exist)
{
    return _cnthread_get_new_page(tgid, addr, mm, 1, was_cacheline_exist);
}
EXPORT_SYMBOL(cnthread_get_new_page);

struct cnthread_req *cnthread_get_page(unsigned int tgid, unsigned long addr, struct mm_struct *mm, int *was_cacheline_exist)
{
    return _cnthread_get_new_page(tgid, addr, mm, 0, was_cacheline_exist);
}
EXPORT_SYMBOL(cnthread_get_page);

// ===== THIS OPTIMIZATION CURRENTLY DISABLED BY THE SWITCH =====
// This function is called to create local copy of memory only when: 
// - this compute node is the first node request memory allocation to the given virtual address
// - this compute node has exclusive access to the memory
// Switch informs whether the node has 'onwership' or not
void cnthread_create_owner_cacheline(unsigned int tgid, unsigned long addr, struct mm_struct *mm)
{
    struct cnthread_cacheline *cnline;
retry_get_free_cache:
    // Check if there is already populated cacheline
    spin_lock(&cnthread_lock);
    spin_lock(&hash_list_lock);
    cnline = find_cacheline_no_lock(tgid, addr);
    if (cnline)
    {   // Skip existing cachline
        spin_unlock(&cnthread_lock);
        spin_unlock(&hash_list_lock);
        return; // Under eviction or parallel access to the same cacheline
    }

    // Find new cacheline
    spin_lock(&free_cacheline_get_lock);
    if (unlikely(list_empty(&cn_free_cacheline_list) || list_empty(&cn_free_page_list)))
    {
        // cnthread_out_of_cache();    // this will kill itself
        printk(KERN_WARNING "Out of DRAM software cache!! (free page: %d [%d], regions: %d[%d])\n",
               atomic_read(&cn_free_page_counter), list_empty(&cn_free_cacheline_list), 
               atomic_read(&cn_free_cacheline_counter), list_empty(&cn_free_page_list));
        spin_unlock(&free_cacheline_get_lock);
        spin_unlock(&hash_list_lock);
        spin_unlock(&cnthread_lock);
        msleep(100);
        goto retry_get_free_cache;
    }
    else
    {
        cnline = _cnthread_allocate_cacheline(tgid, addr, mm);  // Release free_cacheline_get_lock
        if (cnline)
        {
            cnline->ownership = 1;  // This is the only place we set this as 1
        }
        spin_unlock(&hash_list_lock);
        spin_unlock(&cnthread_lock);
    }
}

void cnthread_set_page_received(struct cnthread_req *cnreq)
{
    if (cnreq)
    {
        if (atomic_read(&cnreq->is_used) != IS_PAGE_USED)   // It not already used
        {
            atomic_set(&cnreq->is_used, IS_PAGE_RECEIVED);
        }
    }
}

int cnthread_rollback_page_received(struct cnthread_req *cnreq)
{
    if (cnreq)
    {
        if (atomic_read(&cnreq->is_used) == IS_PAGE_RECEIVED)   // It was unused
        {
            atomic_set(&cnreq->is_used, IS_PAGE_UNUSED);
            return 1;
        }
    }
    return 0;
}

inline int cnthread_is_pre_owned(struct cnthread_req *cnreq, unsigned int tgid, unsigned long address)
{
    if (cnreq && cnreq->cacheline 
        && (cnreq->cacheline->addr == (address & CNTHREAD_CACHLINE_MASK))
        && (cnreq->cacheline->tgid == tgid)
        && (atomic_read(&cnreq->cacheline->used_page) > 0) 
        && !spin_is_locked(&cnreq->cacheline->evict_lock))
    {
        return 1;
    }
    return 0;
}

// This function simply update cnreq to have pte (and related page struct reference counters)
inline int cnthread_add_pte_to_list_with_cnreq(
    pte_t * pte, unsigned long address, struct vm_area_struct *vma,
    struct cnthread_req *cnreq, int new_page)
{
    struct cnthread_cacheline *cnline = cnreq ? cnreq->cacheline : NULL;
    if (unlikely(!cnreq || !cnline))
    {
        printk(KERN_ERR "ERROR: cacheline cleared - cnreq: 0x%lx, cacheline: 0x%lx, addr: 0x%lx, pte: 0x%lx, new_page: %d/%d\n",
               (unsigned long)cnreq, cnreq ? (unsigned long)cnreq->cacheline : 0, address, (unsigned long)pte, new_page, 
               cnreq ? atomic_read(&cnreq->is_used) : -1);
        return -1;
    }

    if (new_page && (atomic_read(&cnreq->is_used) != IS_PAGE_USED))
    {
        atomic_inc(&cnreq->cacheline->used_page);
        pr_cache("New page fetched - tgid: %u, addr: 0x%lx [0x%lx - 0x%lx] used[$:%d, p:%d] free[$:%d, p:%d]\n",
                 cnreq->cacheline->tgid, address, cnreq->cacheline->addr, cnreq->cacheline->addr + CACHELINE_MAX_SIZE,
                 atomic_read(&cnreq->cacheline->used_page), atomic_read(&cnreq->is_used),
                 atomic_read(&cn_free_cacheline_counter), atomic_read(&cn_free_page_counter));
        // Set up mapping counter (cnthread itself hold 1 reference mapping--all the time)
        atomic_set(&cnreq->kpage->_mapcount, 1); // idle: 0 -> mapped: 1
        atomic_set(&cnreq->kpage->_refcount, 3); // idle: 1 -> mapped: 2
    }

    cnreq->pte_ptr = pte;
    cnreq->vma = vma;
    smp_wmb();
    atomic_set(&cnreq->is_used, IS_PAGE_USED);
    return 0;
}
EXPORT_SYMBOL(cnthread_add_pte_to_list_with_cnreq);

//=========== Ack and invalidation listener =========//
static struct cnthread_inv_msg_ctx send_ctx;
static struct socket *recv_socket = NULL, *send_socket = NULL;
static char *ack_buf = NULL;
static char *recv_buf = NULL;
static const size_t _recv_buf_size = 2 * DISAGG_NET_MAX_SIZE_ONCE;

DEFINE_PROFILE_POINT(FH_wait_ack)
DEFINE_PROFILE_POINT(FH_wait_ack_switch)
DEFINE_PROFILE_POINT(FH_wait_ack_inval)
DEFINE_PROFILE_POINT(FH_wait_ack_inval_q)
DEFINE_PROFILE_POINT(FH_wait_ack_handler)
DEFINE_PROFILE_POINT(CN_update_roce_recv)
DEFINE_PROFILE_POINT(CN_update_roce_inv_pre)
DEFINE_PROFILE_POINT(CN_update_roce_inv_ack_evt)
DEFINE_PROFILE_POINT(CN_update_roce_inv_ack_inv)
DEFINE_PROFILE_POINT(CN_update_roce_inv_ack_prmpt)
DEFINE_PROFILE_POINT(CN_update_roce_inv_req_evt)
DEFINE_PROFILE_POINT(CN_update_roce_inv_req_inv)
DEFINE_PROFILE_POINT(CN_update_roce_inv_req_prmpt)
DEFINE_PROFILE_POINT(CN_update_roce_inv_queue)
DEFINE_PROFILE_POINT(CN_update_roce_inv_lock)
DEFINE_PROFILE_POINT(CN_update_roce_inv_send)
DEFINE_PROFILE_POINT(CN_update_roce_inv_req)
DEFINE_PROFILE_POINT(CN_inv_target_data)
DEFINE_PROFILE_POINT(CN_inv_dummy_data)
DEFINE_PROFILE_POINT(CN_inv_target_data_prmpt)
DEFINE_PROFILE_POINT(CN_inv_dummy_data_prmpt)
DEFINE_PROFILE_POINT(CN_inv_other_data_prmpt)
DEFINE_PROFILE_POINT(CN_inv_total)
DEFINE_PROFILE_POINT(CN_inv_total_prmpt)
DEFINE_PROFILE_POINT(CN_inv_total_dum)
DEFINE_PROFILE_POINT(CN_latency_10us)
DEFINE_PROFILE_POINT(CN_latency_50us)
DEFINE_PROFILE_POINT(CN_latency_100us)
DEFINE_PROFILE_POINT(CN_latency_250us)
DEFINE_PROFILE_POINT(CN_latency_500us)
DEFINE_PROFILE_POINT(CN_latency_1000us)

static __always_inline struct cache_waiting_node *find_node_nolock(unsigned int tgid, unsigned long addr)
{
    struct cache_waiting_node *w_node = NULL;
    int found = 0;
    hash_for_each_possible(cn_cache_waiting_list, w_node, hnode, hash_ftn_wait_list(tgid, (u64)addr & PAGE_MASK))
    {
        if ((w_node->tgid == tgid) && (w_node->addr == (addr & PAGE_MASK)))
        {
            found = 1;
            break;
        }
    }
    if (found)
        return w_node;
    return NULL;
}

static __always_inline struct cache_waiting_node *find_node(unsigned int tgid, unsigned long addr)
{
    struct cache_waiting_node *w_node = NULL;
    spin_lock(&cache_waiting_lock);
    w_node = find_node_nolock(tgid, addr);
    spin_unlock(&cache_waiting_lock);
    return w_node;
}

__always_inline struct cache_waiting_node *add_waiting_node(unsigned int tgid, unsigned long addr, struct cnthread_req *cnreq)
{
    struct cache_waiting_node *node;
    struct cnthread_cacheline *cnline = cnreq ? cnreq->cacheline : NULL;
    spin_lock(&cache_waiting_lock);
    node = find_node_nolock(tgid, addr);
    if (unlikely(node))
    {
        spin_unlock(&cache_waiting_lock);
        if (cnreq->cacheline)
        {
            pr_pgfault("WARNING: Existing waiting node — tgid: %u, add: 0x%lx [cachline: 0x%lx]\n",
                        tgid, addr, cnline->addr);
        }
        return (void*)-EAGAIN;    // already existing
    }
    node = kmalloc(sizeof(*node), GFP_KERNEL);
    if (unlikely(!node))
    {
        spin_unlock(&cache_waiting_lock);
        BUG();
        return NULL;
    }
    node->addr = addr & PAGE_MASK;
    node->tgid = tgid;
    atomic_set(&node->target_counter, DISAGG_MAX_COMPUTE_NODE + 1);
    atomic_set(&node->ack_counter, 0);
    atomic_set(&node->unlock_requested, 0);
    barrier();
    hash_add(cn_cache_waiting_list, &node->hnode, hash_ftn_wait_list(tgid, (u64)addr & PAGE_MASK));
    smp_wmb();
    spin_unlock(&cache_waiting_lock);

    // Check evictlock
    // Here, we already have pgfault_lock which was in cnthread_get_page()
    spin_lock(&cnthread_lock);
    spin_lock(&hash_list_lock);
    // Recheck under locks
    cnline = find_cacheline_no_lock(tgid, addr);
    // Check cnline is still assigned to the same addr
    if (cnline && cnreq->cacheline && (cnline == cnreq->cacheline) && !spin_is_locked(&cnline->evict_lock))
    {
        spin_unlock(&cnthread_lock);
        spin_unlock(&hash_list_lock);
        pr_cache("Waiting node added: tgid: %u, add: 0x%lx [cachline: 0x%lx]\n",
                tgid, addr & PAGE_MASK, cnline->addr);
        return node;
    }
    spin_unlock(&cnthread_lock);
    spin_unlock(&hash_list_lock);

    // Under eviction, then rollback and unlock cnreq
    pr_pgfault("WARNING: Cannot add waiting node (evict lock)\n");
    spin_lock(&cache_waiting_lock);
    hash_del(&node->hnode);
    smp_wmb();
    spin_unlock(&cache_waiting_lock);
    kfree(node);
    return NULL;
}
EXPORT_SYMBOL(add_waiting_node);

static int __cnthread_debug_buffer_record[CACHELiNE_ROCE_INVAL_BUF_LENGTH] = {0};
// This function prints out raw values for the given pointer (usually received invalidation request or ACK)
// This function also prints status of the circular invalidation buffer 
// (before and after CNTHREAD_DEBUG_BUFFER_PRINT_RANGE messages)
static void print_raw(char *ptr)
{
    char buf_str[256] = "";
    int i = 0, j = 0;
    pr_info("00: %02x %02x %02x %02x %02x %02x %02x %02x\n",
            ptr[0] & 0xff, ptr[1] & 0xff, ptr[2] & 0xff, ptr[3] & 0xff,
            ptr[4] & 0xff, ptr[5] & 0xff, ptr[6] & 0xff, ptr[7] & 0xff);
    pr_info("08: %02x %02x %02x %02x %02x %02x %02x %02x\n",
            ptr[8] & 0xff, ptr[9] & 0xff, ptr[10] & 0xff, ptr[11] & 0xff,
            ptr[12] & 0xff, ptr[13] & 0xff, ptr[14] & 0xff, ptr[15] & 0xff);

    for (j=0; j<DISAGG_QP_NUM_INVAL_BUF; j++)
    {
        if (inval_buf_ack_head[j] >= CNTHREAD_DEBUG_BUFFER_PRINT_RANGE)
        {
            memset(buf_str, 0, sizeof(char) * 256);
            for (i = 0; i < CNTHREAD_DEBUG_BUFFER_PRINT_RANGE; i++)
            {
                // sprintf(buf_str, "%s %d", buf_str, __cnthread_debug_buffer_record[inval_buf_ack_head[j] - i]);
                sprintf(buf_str, "%s %02x", buf_str,
                        (base_inval_buf[j][(inval_buf_ack_head[j] - i) * CACHELINE_ROCE_HEADER_LENGTH] & 0xff));
            }
            pr_info("Prev: %s\n", buf_str);
        }
        if (inval_buf_ack_head[j] < CACHELiNE_ROCE_INVAL_BUF_LENGTH - CNTHREAD_DEBUG_BUFFER_PRINT_RANGE){
            memset(buf_str, 0, sizeof(char) * 256);
            for (i = 0; i < CNTHREAD_DEBUG_BUFFER_PRINT_RANGE; i++)
            {
                // sprintf(buf_str, "%s %d", buf_str, __cnthread_debug_buffer_record[inval_buf_ack_head[j] + i]);
                sprintf(buf_str, "%s %02x", buf_str,
                        (base_inval_buf[j][(inval_buf_ack_head[j] + i) * CACHELINE_ROCE_HEADER_LENGTH] & 0xff));
            }
            pr_info("Next: %s\n", buf_str);
        }
    }
}

const static int RAW_ACK_TO_VADDR_OFFSET = 2;
const static int RAW_ACK_TO_RKEY_OFFSET = 10;
static u64 parse_vaddr(char *buf)
{
    u64 fva = *((u64 *)(&(buf[RAW_ACK_TO_VADDR_OFFSET])));
    return __be64_to_cpu(fva); // Network endian to host endian
}

static u32 parse_rkey(char *buf)
{
    u32 ret = *((u32 *)(&(buf[RAW_ACK_TO_RKEY_OFFSET])));
    return __be32_to_cpu(ret);
}

static __always_inline void increase_wait_counter(struct cache_waiting_node *node)
{
    if (unlikely(!node))
    {
        BUG();
    }
    atomic_inc(&node->ack_counter);
}

static __always_inline void set_target_counter(struct cache_waiting_node *node, int val)
{
    if (unlikely(!node))
    {
        BUG();
    }
    atomic_set(&node->target_counter, val);
}

static __always_inline int get_target_counter_from_bitmask(u16 shared_list)
{
    int res = 0;
    while (shared_list > 0)
    {
        if (shared_list & 0x1)
            res++;
        shared_list >>= 1;
    }
    return res;
}

static __always_inline int get_id_from_requester(u16 requester)
{
    u16 mask = 0x1;
    int i;
    for (i = 0; i < DISAGG_MAX_COMPUTE_NODE; i++)
    {
        if (requester & mask)
            return i;
        mask <<= 1;
    }
    return -1;
}

static void request_unlock_wait_node(struct cache_waiting_node *node)
{
    if (unlikely(!node))
    {
        BUG();
    }
    // Make sure that unlock is requested
    // (actually the check is meaningless, any number >0 would be OK)
    if (atomic_read(&node->unlock_requested) == 0)  // If not requested yet
        atomic_inc(&node->unlock_requested);
}

void cancel_waiting_for_nack(struct cache_waiting_node *w_node)
{
    request_unlock_wait_node(w_node);
    barrier();
    set_target_counter(w_node, 0); // All needed ack received — it's NACK in this case
}

static unsigned int error_cnt = 0;
static int do_handle_ack(struct cache_waiting_node *w_node)
{
    char *buf = w_node->ack_buf;
    u64 fva = parse_vaddr(buf);
    u32 rkey = parse_rkey(buf);
    u16 pid = (unsigned int)(fva >> 48);
    u64 vaddr = fva & (~MN_VA_PID_BIT_MASK);            // Pure virtual address (48bits)
    u32 state = rkey & CACHELINE_ROCE_RKEY_STATE_MASK;
    PROFILE_POINT_TIME(FH_wait_ack_handler)
    PROFILE_START(FH_wait_ack_handler);

    if (rkey & CACHELINE_ROCE_RKEY_NACK_MASK) // If NACK is set
    {
        u16 r_state = 0, r_sharer = 0, r_size = 0, r_lock = 0, r_cnt = 0;
        error_cnt++;
#ifndef PRINT_CACHE_COHERENCE
        if (error_cnt > DISAGG_NET_POLLING_SHORT_BREAK)
#endif
        {
            send_cache_dir_full_check(pid, vaddr & PAGE_MASK,
                                    &r_state, &r_sharer, &r_size, &r_lock, &r_cnt, CN_SWITCH_REG_SYNC_NONE);
            printk(KERN_WARNING "ERROR: RDMA-NACK [%u] recieved for PID: %u, VA: 0x%llx, state: 0x%x, sharer: 0x%x, size: %u, lock: %u, cnt: %u\n",
                state, pid, vaddr, r_state, r_sharer, r_size, r_lock, r_cnt);
            error_cnt = 0;
        }
        // Request unlock: page fault handler will retry
        if (likely(w_node))
        {
            cancel_waiting_for_nack(w_node);
        }
        else
        { // For debug: can be commented out
            pr_info("NACK: Cannot find waiting node for PID: %u, VA: 0x%llx <-> Cur PID: %u, VA: 0x%lx\n",
                    pid, vaddr, w_node->tgid, w_node->addr);
            BUG();
        }
        return -1;
    }
    else
    {
        // *** Normal ACKs from switch and other nodes *** //
        if (likely(w_node))
        {
            pr_cache("RDMA-ACK [%u] recieved for PID: %u, VA: 0x%llx, state: 0x%x\n",
                     state, pid, vaddr, state);
            if (state == CACHE_STATE_INV_ACK)
            {   // For debug: ack from other node
                // it should NOT be recieved by do_handle_ack() but serve_inval_rdma_ack()
                pr_info_ratelimited("ERROR: RDMA-ACK [%u] recieved for PID: %u, VA: 0x%llx, state: 0x%x\n",
                                    state, pid, vaddr, state);
                increase_wait_counter(w_node);
                return -1;
            }
            else
            {   // Ack directly from the switch
                set_target_counter(w_node, 0); // all ack received
                PROFILE_LEAVE(FH_wait_ack_handler);
                return 0;
            }
        }
        else
        {
#ifndef MIND_USE_TSO    // Packets might be reordered in TSO
            pr_info("ACK: Cannot find waiting node for PID: %u, VA: 0x%llx <-> Cur PID: %u, VA: 0x%lx\n",
                    pid, vaddr, w_node->tgid, w_node->addr);
#endif
        }
    }
    return -1;
}

static __always_inline void check_req_and_unlock_pgfault(struct cnthread_req *cnreq)
{
    if (likely(cnreq))
    {
        // Check whether it was cleared by others or not
        int clear_responsibility = cnthread_rollback_page_received(cnreq);
        smp_wmb();
        if (clear_responsibility)
        {
            cnthread_put_page(cnreq);
        }
        else if (likely(spin_is_locked(&cnreq->pgfault_lock)))
        {
            // We need this so that invalidation handler can serve this ACK (and put the page)
            // which is the one we are waiting here
            spin_unlock(&cnreq->pgfault_lock);
        }
    }
}

inline int wait_until_counter(struct cache_waiting_node *node, spinlock_t *pte_lock, struct rw_semaphore *mmap_sem, struct cnthread_req *cnreq)
{
    int r_val = 0;
    int i = 0, j = 0;
    int ret = 0;
    unsigned long start_time = jiffies;
    int unlocked = 0;

    PROFILE_POINT_TIME(FH_wait_ack)
    PROFILE_POINT_TIME(FH_wait_ack_switch)
    PROFILE_POINT_TIME(FH_wait_ack_inval)
    PROFILE_POINT_TIME(FH_wait_ack_inval_q)

    if (unlikely(!node))
    {
        BUG();
    }
    PROFILE_START(FH_wait_ack);
    PROFILE_START(FH_wait_ack_switch);  // Direct ACK/NACK from switch
    PROFILE_START(FH_wait_ack_inval);   // Invalidation ACK from other nodes
    PROFILE_START(FH_wait_ack_inval_q);

    while (1)
    {
        for (i = 0; i < DISAGG_NET_CTRL_POLLING_SKIP_COUNTER; i++)
        {
            // Check for ACK/NACK from switch
            if (node->ack_buf)
            {
                // If something written from RDMA
                if (node->ack_buf[0])
                {
                    if(!do_handle_ack(node))
                    {
                        PROFILE_LEAVE(FH_wait_ack_switch);
                        goto out_wait;  //ACK
                    }
                }
            }

            r_val = atomic_read(&node->ack_counter);    // This will be set only by ACK/NACK
            if (r_val >= atomic_read(&node->target_counter)) // It only read it
            {
                PROFILE_LEAVE(FH_wait_ack_inval);
                goto out_wait;
            }
#ifdef MIND_USE_TSO
            // If we get message from switch, we are done for cache coherence and let's wait for the actual data
            if (atomic_read(&node->target_counter) <= DISAGG_MAX_COMPUTE_NODE)
            {
                PROFILE_LEAVE(FH_wait_ack_inval_q);
                goto out_wait;
            }
#endif
            for (j = 0; j < DISAGG_QP_NUM_INVAL_BUF; j++)
                try_invalidation_lookahead(PROFILE_CNTHREAD_INV_ACK_SERV_FROM_PRMPT, SERVE_ACK_PER_WAIT);
            r_val = atomic_read(&node->unlock_requested);
            if (r_val == 1)
            {
                // We need to unlock ptl and other locks here
                check_req_and_unlock_pgfault(cnreq);
                if (pte_lock)
                    spin_unlock(pte_lock);
                if (mmap_sem)
                    up_read(mmap_sem);
                unlocked = 1;
                pr_pgfault("CNTHREAD - pte unlocked: tgid: %u, addr: 0x%lx, ack_cnt: %d, tar_cnt: %d\n",
                         node->tgid, node->addr, atomic_read(&node->ack_counter), atomic_read(&node->target_counter));
                atomic_inc(&node->unlock_requested);
            }
        }
        // Check timer
        if (unlikely(jiffies_to_msecs(jiffies - start_time) >= (unsigned long)(DISAGG_NET_ACK_TIMEOUT_IN_MS)))
        {
            u16 state = 0, sharer = 0;
            send_cache_dir_check(node->tgid, node->addr & PAGE_MASK,
                                 &state, &sharer, CN_SWITCH_REG_SYNC_NONE); // pull state before sending something
            printk(KERN_WARNING "ERROR: Cannot receive ACK/NACK - cpu :%d, tgid: %u, addr: 0x%lx, ack_cnt: %d, tar_cnt: %d, timeout (%u ms) / state: 0x%x, sharer: 0x%x\n",
                   smp_processor_id(), node->tgid, node->addr,
                   atomic_read(&node->ack_counter), atomic_read(&node->target_counter),
                   jiffies_to_msecs(jiffies - start_time), state, sharer);
            atomic_inc(&node->unlock_requested);
            print_raw(node->ack_buf);
            if (!base_inval_buf[0])
            {
                printk(KERN_WARNING "ERRPR: cannot get invalidation buffer (head: %u / %u)\n", inval_buf_head, inval_buf_ack_head[0]);
            }
            else
            {
                printk(KERN_WARNING "RDMA-INVAL buf: 0x%lx, head: %u / %u, val: 0x%02x\n",
                       (unsigned long)base_inval_buf[0], inval_buf_head, inval_buf_ack_head[0], 
                       base_inval_buf[0][inval_buf_ack_head[0] * CACHELINE_ROCE_HEADER_LENGTH] & 0xff);
                print_raw(base_inval_buf[0]);
            }
            goto out_wait;
        }
    };
out_wait:
    // Check there was unlock request (NACK)
    barrier();
    r_val = atomic_read(&node->unlock_requested);
    if (r_val > 0 || unlikely(cnreq && !cnreq->cacheline))
    {
        ret = -1;
        if (!unlocked)
        {
            if (pte_lock)
                spin_unlock(pte_lock);
            if (mmap_sem)
                up_read(mmap_sem);
            check_req_and_unlock_pgfault(cnreq);
            // try_invalidation_processing_from_page_fault_handler(cnreq);
        }
    }

    PROFILE_LEAVE(FH_wait_ack);
    // pr_cache("Wait data - before wait_lock: Node: 0x%lx, Ptr: 0x%lx [cpu :%d]\n",
    //          (unsigned long)node, (unsigned long)node->ack_buf, smp_processor_id());
    spin_lock(&cache_waiting_lock);
    hash_del(&node->hnode);
    smp_wmb();
    spin_unlock(&cache_waiting_lock);
    // pr_cache("Wait data - after wait_lock: Node: 0x%lx, Ptr: 0x%lx [cpu :%d]\n",
    //          (unsigned long)node, (unsigned long)node->ack_buf, smp_processor_id());
    return ret;
}
EXPORT_SYMBOL(wait_until_counter);

// This is a routine that can be use inside page fault handler.
// While page fault handler waits for data over network, it can help invalidation handler
// ** CURRENTLY DISABLED DUE TO PERFORMANCE OVERHEAD (caching issue) **
void try_invalidation_processing_from_page_fault_handler(struct cnthread_req *cnreq)
{
#if 0
    struct cnthread_cacheline *cnline = NULL;
    if (cnreq)
    {
        cnline = cnreq->cacheline;
    }
    if (atomic_read(&cnthread_inv_req_counter) > PREEMPTIVE_INV_HIGH_PRESSURE)
    {
        int helper_id = atomic_inc_return(&cnthread_pgfault_helper_idx) % PG_FAULT_HELPER_CPU;
        if (spin_trylock(&cnthread_pgfault_helper_lock[helper_id]))
        {
            check_inval_req_list_and_try(PROFILE_CNTHREAD_INV_ACK_SERV_FROM_PRMPT, cnline);
            spin_unlock(&cnthread_pgfault_helper_lock[helper_id]);
        }
    }
#endif
}

static void __always_inline cnthread_print_cache_status(void)
{
    int cur_size = atomic_read(&cnthread_list_counter);
    pr_info("CNTHREAD - Cache pressure: %d / %d | free [$:%d, pg:%d] | tot [%lu]\n",
            cur_size, (int)CNTHREAD_CACHELINE_OVERSUB,
            atomic_read(&cn_free_cacheline_counter),
            atomic_read(&cn_free_page_counter),
            (unsigned long)atomic_long_read(&cn_total_eviction_counter));
}

static int __always_inline cnthread_is_pressure(void)
{
    if ((atomic_read(&cnthread_list_counter) >= _threshold) || (atomic_read(&cn_free_page_counter) <= _page_free_threshold))
    {
#ifdef PRINT_CACHE_COHERENCE
        cnthread_print_cache_status();
#endif
        return 1;
    }
    else
        return 0;
}

static int __always_inline cnthread_is_high_pressure(void)
{
    if ((atomic_read(&cnthread_list_counter) >= _high_threshold) || (atomic_read(&cn_free_page_counter) <= _page_high_free_threshold))
    {
#ifdef PRINT_CACHE_COHERENCE
        cnthread_print_cache_status();
#endif
        return 1;
    }
    else
        return 0;
}

// Set of profiling points regarding invalidation messaging
DEFINE_PROFILE_POINT(cnthread_evict_one)
DEFINE_PROFILE_POINT(cnthread_evict_one_lock)
DEFINE_PROFILE_POINT(cnthread_evict_one_rdma)
DEFINE_PROFILE_POINT(cnthread_evict_finish_proc)
DEFINE_PROFILE_POINT(cnthread_evict_flush_page)
DEFINE_PROFILE_POINT(cnthread_send_fin_ack_inv)
DEFINE_PROFILE_POINT(cnthread_send_fin_ack_evt)
DEFINE_PROFILE_POINT(cnthread_evict_flush_tlb)
DEFINE_PROFILE_POINT(cnthread_evict_range)
DEFINE_PROFILE_POINT(cnthread_evict_range_flush)
DEFINE_PROFILE_POINT(cnthread_evict_range_remove)
DEFINE_PROFILE_POINT(cnthread_evict_range_page)

// check victim, pte, pte_lock, vma and make pte read-only if it is presented and writable
static //__always_inline
struct cnthread_inval_pte 
__cnthread_check_and_make_read_only(struct cnthread_req *victim, unsigned long addr)
{
    pte_t *pte = NULL;  //, pte_val;
    spinlock_t *pte_lock = NULL;
    pte_t entry;
    struct cnthread_inval_pte res = {0, 0, 0, 0};

    if (victim->vma && (victim->vma->vm_file || !vma_is_anonymous(victim->vma)))
    {
        return res;   // skip to unmap file or non-anonynous mappings and read only pages
    }

    // While we are doing this task, some memory accesses can occur
    // So, let's make it as a read-only page first
    if (likely(victim && victim->cacheline && victim->cacheline->mm))
    {
        pte = find_pte_target_lock(victim->cacheline->mm, addr, &pte_lock);
    }

    if (unlikely(!pte || !pte_lock || !pte_present(*pte)))
    {
        unsigned long dummy = 0;
        if (unlikely(probe_kernel_address((unsigned long *)victim->pte_ptr, dummy)))
        {
            pr_cache("CNTHREAD - no pte: tgid: %u, addr: 0x%lx, pte: 0x%lx [val: 0x%lx], pte_victim: 0x%lx [bad: %d, 0x%lx]\n",
                     victim->cacheline ? victim->cacheline->tgid : 0, 
                     addr, (unsigned long)pte, pte ? (unsigned long)pte->pte : 0,
                     (unsigned long)victim->pte_ptr, 1, dummy);
        }
        // It was already freed by others
        res.skip = 1;
        return res;
    }

    if (unlikely(pte != victim->pte_ptr))
    {
        pr_cache("CNTHREAD - pte != given victim pte: pte: 0x%lx, pte_victim: 0x%lx\n",
                 (unsigned long)pte, (unsigned long)victim->pte_ptr);
    }

    if (likely(victim->vma))
    {
        // Let's rely on pagefault lock which is per page
        spin_lock(pte_lock);
        if (!pte || !pte_present(*pte))
        {
            // If already evicted
            res.skip = 1;
            spin_unlock(pte_lock);
            return res;
        }

        // Flush data from cache
        entry = *pte; // current value
        if (pte_write(*pte))
        {
            flush_cache_page(victim->vma, addr, pte_pfn(*(ctx->pte)));
            entry = pte_wrprotect(entry);
            res.need_push_back = 1;
            smp_wmb();
            set_pte_at_notify(victim->cacheline->mm, addr, pte, entry);
        }
        spin_unlock(pte_lock);
    }else{
        BUG();
    }
    res.pte = pte;
    res.ptl = pte_lock;
    return res;
}

static //__always_inline
int __cnthread_send_other_data_and_zap_pte(struct cnthread_inval_pte *ctx, struct cnthread_req *victim,
                                           unsigned long addr, int remove_data, struct mmu_gather *tlb)
{
    struct vm_area_struct *vma = victim->vma;
    PROFILE_POINT_TIME(cnthread_evict_range_page)
    if (ctx->need_push_back)    // was writable
    {
        PROFILE_START(cnthread_evict_range_page);
        flush_cache_page(vma, addr, pte_pfn(*(ctx->pte)));
        if (unlikely(cn_copy_page_data_to_mn(victim->cacheline->tgid, victim->cacheline->mm, addr,
                                             ctx->pte, CN_OTHER_PAGE, 0, (void *)victim->dma_addr)))
        {
            pr_warn("CNTHREAD - Cannot send data to MN...\n");
            goto rollback;  // It will simply return error -1
        }
        PROFILE_LEAVE(cnthread_evict_range_page);
    }else{
        pr_cache("Read only data: 0x%lx\n", addr);
    }

    if (remove_data)
    {
        spin_lock(ctx->ptl);
        if (pte_present(*ctx->pte))
        {
            // Now we are safe to unmap this - unmap page and PTE
            update_hiwater_rss(victim->cacheline->mm);
            zap_one_pte_without_lock(tlb, vma, addr, addr + PAGE_SIZE,
                                     ctx->pte, NULL);
        }
        spin_unlock(ctx->ptl);
    }
    return 0;

rollback:
    return 0;
}

static int __cnthread_evict_range(struct cnthread_cacheline *cnline, unsigned long addr, unsigned long len, 
                                  unsigned long tar_addr, int remove_data, int is_invalid, int *lock_list)
{
    PROFILE_POINT_TIME(cnthread_evict_range)
    PROFILE_POINT_TIME(cnthread_evict_range_flush)
    PROFILE_POINT_TIME(cnthread_evict_range_remove)
    int i, len_in_page = (len / PAGE_SIZE);
    struct cnthread_req *victim;
    unsigned long tmp_addr = 0;
    struct mmu_gather tlb;
    int flush_cnt = 0;
    struct cnthread_inval_pte *on_going_pte_list = NULL;
    PROFILE_START(cnthread_evict_range);
    on_going_pte_list = kzalloc(sizeof(struct cnthread_inval_pte) * len_in_page, GFP_KERNEL);
    if (unlikely(!on_going_pte_list))
    {
        BUG();
    }
    tmp_addr = addr;
    PROFILE_START(cnthread_evict_range_flush);
    for (i = 0; i < len_in_page; i++)
    {
        if (tmp_addr == tar_addr)
        {
            tmp_addr += PAGE_SIZE;
            continue;
        }
        victim = find_page_from_cacheline(cnline, tmp_addr);
        if (victim)
        {
            if (atomic_read(&victim->is_used) == IS_PAGE_USED) // For any on-going page faults, 
                                                               // we already triggered cancellation of the corresponding waiting node
            {
                if (unlikely((atomic_read(&cnline->used_page) <= 0) || !lock_list[i]))
                {
                    // DEBUG
                    printk(KERN_WARNING
                           "ERROR: Suspicious (oth) used_page [%d] is_used [%d]"
                           "is_locked [%d]- tgid: %u, addr: 0x%lx, inval: %d, Ddel:%d\n",
                           atomic_read(&cnline->used_page), atomic_read(&victim->is_used), (int)spin_is_locked(&victim->pgfault_lock),
                           cnline->tgid, addr, is_invalid, remove_data);
                    if (atomic_read(&cnline->used_page) <= 0)
                        atomic_set(&cnline->used_page, 1);  // At least one for itself
                }
                on_going_pte_list[i] = __cnthread_check_and_make_read_only(victim, tmp_addr);
                if (on_going_pte_list[i].need_push_back)
                    flush_cnt ++;
            }
        }
        tmp_addr += PAGE_SIZE;
    }
    if (flush_cnt)
    {
        flush_tlb_mm(cnline->mm);
        PROFILE_LEAVE(cnthread_evict_range_flush);
    }
    // If we need to remove data (if another compute blade wants to have exclusive permission to the data)
    if (remove_data || flush_cnt)
    {
        PROFILE_START(cnthread_evict_range_remove);
        lru_add_drain();
        tlb_gather_mmu(&tlb, cnline->mm, addr, addr + len);
        tmp_addr = addr;
        for (i = 0; i < len_in_page; i++)
        {
            if (on_going_pte_list[i].pte && !on_going_pte_list[i].skip && (tmp_addr != tar_addr))
            {
                victim = find_page_from_cacheline(cnline, tmp_addr);
                __cnthread_send_other_data_and_zap_pte(&on_going_pte_list[i], victim, tmp_addr, remove_data, &tlb);
            }
            tmp_addr += PAGE_SIZE;
        }
        tlb_finish_mmu(&tlb, addr, addr + len);
        // Finalize
        if (remove_data)
        {
            tmp_addr = addr;
            for (i = 0; i < len_in_page; i++)
            {
                victim = find_page_from_cacheline(cnline, tmp_addr);
                if (victim)
                {
                    if ((on_going_pte_list[i].skip || on_going_pte_list[i].pte) && (tmp_addr != tar_addr))
                    {
                        if (likely(atomic_read(&victim->is_used) == IS_PAGE_USED))
                        {
                            cnthread_put_page(victim);
                            atomic_dec(&cnline->used_page);
                            atomic_long_inc(&cn_total_eviction_counter);
                        }
                    }
                }
                tmp_addr += PAGE_SIZE;
            }
        }
        PROFILE_LEAVE(cnthread_evict_range_remove);
    }
    PROFILE_LEAVE(cnthread_evict_range);
    kfree(on_going_pte_list);
    return 0;
}

// prototype here
static void cnthread_send_dummy_data(unsigned int tgid, unsigned long addr,
                                     struct cnthread_inv_msg_ctx *ctx);

/* 
 *  We assume that the caller of this function already hold mm->mmap_sem
 */
static int ___cnthread_evict_one(struct cnthread_req *victim, unsigned long addr, int cur_size, int high_threshold,
                                 struct cnthread_inv_msg_ctx *inv_ctx, char* inval_buf, u32 req_qp)
{
    
    pte_t *pte = NULL;
    spinlock_t *pte_lock = NULL;
    struct mmu_gather tlb;
    pte_t entry;
    int need_push_back = 0;
    int skip_eviction = 0;
    struct cnthread_cacheline *cnline;
    struct vm_area_struct *vma = NULL;

    PROFILE_POINT_TIME(cnthread_evict_one)
    PROFILE_POINT_TIME(cnthread_evict_one_lock)
    PROFILE_POINT_TIME(cnthread_evict_one_rdma)
    PROFILE_POINT_TIME(cnthread_evict_finish_proc)
    PROFILE_POINT_TIME(cnthread_evict_flush_page)
    PROFILE_POINT_TIME(cnthread_evict_flush_tlb)

    PROFILE_START(cnthread_evict_one);

    if (victim->vma && (victim->vma->vm_file || !vma_is_anonymous(victim->vma)))
    {
        return 0;   // TODO: Skip file or non-anonynous mappings
    }

    // While we are doing this task, some memory accesses can occur
    // So, let it be a read only first
    cnline = victim->cacheline;
    if (likely(cnline && cnline->mm))
    {
        pte = find_pte_target_lock(cnline->mm, addr, &pte_lock);
    }

    if (unlikely(!pte || !pte_lock || !pte_present(*pte)))
    {
        unsigned long dummy = 0;
        if (unlikely(probe_kernel_address((unsigned long *)victim->pte_ptr, dummy)))
        {
            pr_cache("CNTHREAD - no pte: tgid: %u, addr: 0x%lx, pte: 0x%lx [val: 0x%lx], pte_victim: 0x%lx [bad: %d, 0x%lx]\n",
                     victim->cacheline->tgid, addr,
                     (unsigned long)pte,
                     pte ? (unsigned long)pte->pte : 0,
                     (unsigned long)victim->pte_ptr,
                     1, dummy);
        }
        // It was already freed by others
        goto clear_victim_from_evict_list;
    }

    if (unlikely(pte != victim->pte_ptr))
    {
        pr_cache("CNTHREAD - pte != given victim pte: pte: 0x%lx, pte_victim: 0x%lx\n",
                 (unsigned long)pte, (unsigned long)victim->pte_ptr);
    }

    vma = victim->vma;
    if (likely(vma && (atomic_read(&victim->is_used) == IS_PAGE_USED)))
    {
        // Let's rely on pagefault lock which is per page
        PROFILE_START(cnthread_evict_one_lock);
        // We are safe to modify victim thanks to pgfault_lock,
        // but to prevent other accesses to the pte
        spin_lock(pte_lock);
        PROFILE_LEAVE(cnthread_evict_one_lock);

        if (!pte || !pte_present(*pte))
        {
            // if already evicted
            spin_unlock(pte_lock);
            goto clear_victim_from_evict_list;
        }

        entry = *pte; // Current value
        if (pte_write(*pte))
        {
            // Flush data from cache (actually dummy for X86)
            flush_cache_page(vma, addr, pte_pfn(*pte));
            // Make it read only
            entry = pte_wrprotect(entry);
            need_push_back = 1;

            PROFILE_START(cnthread_evict_flush_tlb);
            smp_wmb();
            set_pte_at_notify(cnline->mm, addr, pte, entry);
            // Please check cpu list for better understading of the latency: mm_cpumask(mm)
            flush_tlb_mm_range(cnline->mm, addr, addr + PAGE_SIZE, VM_NONE);
            PROFILE_LEAVE(cnthread_evict_flush_tlb);
        }
        spin_unlock(pte_lock);

        // Now we have read-only pte and data in memory
        // We assume that communication/re-tx was already handled by the RDMA library.
        PROFILE_START(cnthread_evict_one_rdma);
        if (need_push_back || inv_ctx->is_target_data)
        {
            int cpu_id = (2 * DISAGG_NUM_CPU_CORE_IN_COMPUTING_BLADE) + get_cpu();
#ifdef CNTHREAD_COPY_FOR_INVALIDATE
            memcpy(get_dummy_page_buf_addr(cpu_id), victim->kmap, PAGE_SIZE);
            if (unlikely(cn_copy_page_data_to_mn(cnline->tgid, cnline->mm, addr,
                                                 NULL, inv_ctx->is_target_data ? CN_TARGET_PAGE : CN_OTHER_PAGE,
                                                 req_qp, (void*)get_dummy_page_dma_addr(cpu_id))))
#else
            (void)cpu_id;   // make compiler happy
            if (unlikely(cn_copy_page_data_to_mn(cnline->tgid, cnline->mm, addr,
                                                 pte, inv_ctx->is_target_data ? CN_TARGET_PAGE : CN_OTHER_PAGE, 
                                                 req_qp, (void *)victim->dma_addr)))
#endif
            {
                pr_warn("CNTHREAD - Cannot send data to MN...\n");
                skip_eviction = -1;
                put_cpu();
                goto rollback;  // will simply return error -1
            }
            put_cpu();
            PROFILE_LEAVE(cnthread_evict_one_rdma);
        }else{
            pr_cache("Read only data: 0x%lx\n", addr);
        }

        if (inv_ctx->remove_data)
        {
            PROFILE_START(cnthread_evict_finish_proc);
            spin_lock(pte_lock);
            tlb_gather_mmu(&tlb, cnline->mm, addr, addr + PAGE_SIZE);
            update_hiwater_rss(cnline->mm);
            // Try direct unmap
            zap_one_pte_without_lock(&tlb, vma, addr, addr + PAGE_SIZE,
                                     pte, NULL); // flush now for this page
            spin_unlock(pte_lock);
            tlb_finish_mmu(&tlb, addr, addr + PAGE_SIZE);
            PROFILE_LEAVE(cnthread_evict_finish_proc);
        }
    }else{
        // Unlikely case
        printk(KERN_ERR "ERROR: suspicious used_page: %d\n", atomic_read(&victim->is_used));
    }

clear_victim_from_evict_list:
    if (victim && inv_ctx->remove_data && likely(atomic_read(&victim->is_used) == IS_PAGE_USED))
    {
        cnthread_put_page(victim);
        atomic_dec(&cnline->used_page);
        atomic_long_inc(&cn_total_eviction_counter);
    }

#ifdef PRINT_CACHE_COHERENCE
    // if (inv_ctx->remove_data)
    {
        int used_page = 0;
        if (likely(victim))
        {
            used_page = atomic_read(&cnline->used_page);
            pr_cache("Evicted cache - tgid: %d, addr: 0x%lx, used_page: %d, Ddel: %d\n",
                    cnline ? (int)cnline->tgid : -1, addr,
                    victim ? used_page : -1, inv_ctx->remove_data);
        }
    }
#endif
    PROFILE_LEAVE(cnthread_evict_one);
    return skip_eviction;

rollback:
    // Please unlock locks if there is any (no for now)
    return skip_eviction;
}

static void cnthread_send_dummy_data(unsigned int tgid, unsigned long addr,
                                     struct cnthread_inv_msg_ctx *ctx)
{
    u8 opcode, dummy_flag = 0x40;
    u32 dummy_ack_req_and_psn = 0x80000000 | ctx->rdma_ctx.psn;
    u32 ret, dma_len = PAGE_SIZE;
    u16 ret_16;
    struct sockaddr_in addr_in;
    PROFILE_POINT_TIME(CN_inv_dummy_data)
    PROFILE_START(CN_inv_dummy_data);
    spin_lock(&cnthread_udp_lock);
    memcpy(&addr_in, &ctx->addr_in, sizeof(addr_in));
    // Generate RoCE Req here
    // - switch need: src ip, dest ip, and qp
    // - switch will swap src and dest, and set up opcode as RoCE read request
    opcode = ROCE_WRITE_REQ_OPCODE;
    memcpy(&(ctx->dummy_buf[CACHELINE_ROCE_OFFSET_TO_OPCODE]), &opcode, sizeof(opcode));
    memcpy(&(ctx->dummy_buf[CACHELINE_ROCE_OFFSET_TO_FLAGS]), &dummy_flag, sizeof(dummy_flag));
    // set up psn
    dummy_ack_req_and_psn = __cpu_to_be32(dummy_ack_req_and_psn);
    memcpy(&(ctx->dummy_buf[CACHELINE_ROCE_OFFSET_TO_ACKREQ]), &dummy_ack_req_and_psn, sizeof(dummy_ack_req_and_psn));
    // set up qp
    ret = ctx->original_qp;
    ret = __cpu_to_be32(ret);
    memcpy(&(ctx->dummy_buf[CACHELINE_ROCE_OFFSET_TO_QP]), &ret, sizeof(ret));
    // set up pkey
    ret_16 = 0xffff; //65535
    ret_16 = __cpu_to_be16(ret_16);
    memcpy(&(ctx->dummy_buf[CACHELINE_ROCE_OFFSET_TO_PKEY]), &ret_16, sizeof(ret_16));
    // set up rkey to mark this message as dummy data requets
    ret = *((u32 *)(&(ctx->dummy_buf[CACHELINE_ROCE_OFFSET_TO_RKEY])));
    ret = __be32_to_cpu(ret);
    ret &= ~((u32)MN_RKEY_PERMISSION_MASK); // erase given permission
    ret |= (VM_READ << MN_RKEY_PERMISSION_SHIFT);
    ret = __cpu_to_be32(ret);
    memcpy(&(ctx->dummy_buf[CACHELINE_ROCE_OFFSET_TO_RKEY]), &ret, sizeof(ret));
    // DMA length in the RoCE header
    dma_len = __cpu_to_be32(dma_len);
    memcpy(&(ctx->dummy_buf[CACHELINE_ROCE_OFFSET_TO_DMALEN]), &dma_len, sizeof(dma_len));
    // set up destination addr as requster's data
    addr_in.sin_addr.s_addr = ctx->addr_in.sin_addr.s_addr;
    addr_in.sin_port = htons(DEFAULT_ROCE_PORT); // send to the roce
    udp_send(ctx->sk, ctx->dummy_buf, CACHELINE_ROCE_HEADER_LENGTH, MSG_DONTWAIT, &addr_in); // send back to controller
    spin_unlock(&cnthread_udp_lock);
    PROFILE_LEAVE(CN_inv_dummy_data);
}

static int cnthread_send_finish_ack_roce(u16 tgid, unsigned long addr, int is_invalid)
{
    struct fault_data_struct payload;
    struct fault_reply_struct reply; //dummy buffer for ack
    int ret;
    u32 msg_type = DISSAGG_ROCE_FIN_ACK;
    int cpu_id = get_cpu();
    // WE MAY NOT NEED LOCK HERE (no data buffer to send)
    spin_lock(&cnthread_inval_send_ack_lock[cpu_id]);
    payload.pid = tgid; // dummy, will not be used
    payload.tgid = tgid;
    payload.address = addr;
    payload.data_size = CACHELINE_ROCE_HEADER_LENGTH;
    // dummy pointer, no meanful data
    payload.data = (void*)get_dummy_page_dma_addr(DISAGG_NUM_CPU_CORE_IN_COMPUTING_BLADE + cpu_id);
    barrier();
    if (!is_invalid)
        msg_type = DISSAGG_ROCE_EVICT_ACK;
    ret = send_msg_to_memory_rdma(msg_type, &payload, CACHELINE_ROCE_HEADER_LENGTH,
                                  &reply, sizeof(reply));
    spin_unlock(&cnthread_inval_send_ack_lock[cpu_id]);
    put_cpu();
    return ret;
}

static void cnthread_send_finish_ack(unsigned int tgid, unsigned long addr,
                                     struct cnthread_inv_msg_ctx *ctx, int is_invalid)
{
    u8 opcode;
    u32 ret;
    u64 fva;
    int res, i;
    struct sockaddr_in addr_in;
    PROFILE_POINT_TIME(cnthread_send_fin_ack_inv)
    PROFILE_POINT_TIME(cnthread_send_fin_ack_evt)
    if (unlikely(!ctx || !ack_buf))
    {
        BUG();
    }
    PROFILE_START(cnthread_send_fin_ack_inv);
    PROFILE_START(cnthread_send_fin_ack_evt);
#ifndef ROCE_FIN_ACK
    spin_lock(&cnthread_udp_send_lock);
    // clean up
    memset(ack_buf, 0, _recv_buf_size);
    // Opcode @ CACHELINE_ROCE_OFFSET_TO_OPCODE
    opcode = ROCE_WRITE_REQ_OPCODE;
    memcpy(&(ack_buf[CACHELINE_ROCE_OFFSET_TO_OPCODE]), &opcode, sizeof(opcode));
    // virtual address
    fva = generate_full_addr(tgid, addr);
    fva = __cpu_to_be64(fva); // network endian to host endian (e.g., big to little in usual X86)
    memcpy(&(ack_buf[CACHELINE_ROCE_OFFSET_TO_VADDR]), &fva, sizeof(fva));
    // Switch need to differentiate eviction and invalidation:
    //       - S^D and M^D requires invalidation (with ACK and/or DATA) not just eviction
    // Invalidation finished ack: send with permission 0x3 = (VM_READ | VM_WRITE)
    ret = *((u32 *)(&(ack_buf[CACHELINE_ROCE_OFFSET_TO_RKEY])));
    ret = __be32_to_cpu(ret);
    ret &= ~((u32)MN_RKEY_PERMISSION_MASK); // erase given permission
    if (is_invalid)
        ret |= ((VM_READ | VM_WRITE) << MN_RKEY_PERMISSION_SHIFT);
    else
        ret |= (MN_RKEY_VM_EVICTION << MN_RKEY_PERMISSION_SHIFT);
    ret = __cpu_to_be32(ret);
    memcpy(&(ack_buf[CACHELINE_ROCE_OFFSET_TO_RKEY]), &ret, sizeof(ret));
    // dummy qp
    ret = DISAGG_QP_INV_ACK_OFFSET_START;   // we do not send data via this QP
    ret = __cpu_to_be32(ret);
    memcpy(&(ack_buf[CACHELINE_ROCE_OFFSET_TO_QP]), &ret, sizeof(ret));
    barrier();

    // Set address
    addr_in.sin_family = AF_INET;
    addr_in.sin_addr.s_addr = get_controller_ip();
    addr_in.sin_port = htons(DEFAULT_ROCE_PORT); // send to the roce

    res = udp_send(ctx->sk, ack_buf, ROCE_REQ_SIZE, MSG_DONTWAIT, &addr_in); // send back to controller
    if (unlikely(res < ROCE_REQ_SIZE))
    {
        printk(KERN_ERR "ERROR: sent finish ACK [%d]\n", res);
        for (i = 0; i < ROCE_REQ_SIZE; i += 4)
        {
            pr_cache("0x%x ", *((unsigned int *)&ack_buf[i]));
        }
    }
    spin_unlock(&cnthread_udp_send_lock);
#else
    (void)opcode;
    (void)ret;
    (void)fva;
    (void)res;
    (void)i;
    (void)addr_in;
    cnthread_send_finish_ack_roce(tgid, addr, is_invalid);
#endif
    if (is_invalid == TRY_INVALIDATION)
        PROFILE_LEAVE(cnthread_send_fin_ack_inv);
    else
        PROFILE_LEAVE(cnthread_send_fin_ack_evt);
}

inline u64 generate_full_addr(u16 tgid, unsigned long addr)
{
    u64 full_addr = (u64)tgid << MN_VA_PID_SHIFT; // first 16 bits
    full_addr += addr & (~MN_VA_PID_BIT_MASK);    // last 48 bits
    return full_addr;
}
EXPORT_SYMBOL(generate_full_addr);

int is_owner_address(unsigned int tgid, unsigned long addr)
{
    struct cnthread_cacheline *cnline = find_cacheline_no_lock(tgid, addr & CNTHREAD_CACHLINE_MASK);
    if (cnline)
        return cnline->ownership;
    else
        return 0;
}

static char *dummy_page_buf[DISAGG_NUM_CPU_CORE_IN_COMPUTING_BLADE * 3] = {NULL};
static char *dummy_inv_page_buf = NULL;
static unsigned long dummy_page_dma_addr[DISAGG_NUM_CPU_CORE_IN_COMPUTING_BLADE * 3] = {0};
static unsigned long dummy_inv_page_dma_addr = 0;
#ifndef MIND_USE_TSO
static int cn_send_inval_ack_roce(u16 tgid, unsigned long addr, void* inval_buf)
{
    struct fault_data_struct payload;
    struct fault_reply_struct reply; //buffer for ack
    int ret, cpu_id = get_cpu();
    spin_lock(&cnthread_inval_send_ack_lock[cpu_id]);
    payload.pid = tgid;
    payload.tgid = tgid;
    payload.address = addr;
    payload.data_size = CACHELINE_ROCE_HEADER_LENGTH;
    payload.data = (void*)get_dummy_inv_page_dma_addr();
    memcpy(dummy_inv_page_buf, inval_buf, CACHELINE_ROCE_HEADER_LENGTH);
    barrier();
    ret = send_msg_to_memory_rdma(DISSAGG_ROCE_INVAL_ACK, &payload, CACHELINE_ROCE_HEADER_LENGTH,
                                  &reply, sizeof(reply));
    put_cpu();
    spin_unlock(&cnthread_inval_send_ack_lock[cpu_id]);
    return ret;
}

static void _cnthread_send_inval_ack(u16 tgid, unsigned long vaddr, char *inval_buf)
{
    int err;
    PROFILE_POINT_TIME(CN_update_roce_inv_send)
    PROFILE_START(CN_update_roce_inv_send);
    err = cn_send_inval_ack_roce(tgid, vaddr, inval_buf);
    PROFILE_LEAVE(CN_update_roce_inv_send);
    pr_cache("Inv ACK sent [err: %d] - tgid: %u, addr: 0x%lx\n", err, tgid, vaddr);
}
#endif

static int check_and_hold_evict_lock(unsigned int tgid, unsigned long addr,
                                     struct cnthread_inv_msg_ctx *inv_ctx, 
                                     struct cnthread_cacheline *cnline)
{
    unsigned long cache_addr = addr & CNTHREAD_CACHLINE_MASK;
    int ret = RET_NO_CNLINE;
    spin_lock(&cnthread_lock);
    spin_lock(&hash_list_lock);
    inv_ctx->cnline_ptr = find_cacheline_no_lock(tgid, cache_addr);
    if (inv_ctx->cnline_ptr)
    {
        // As the caller of this function can be the page fault handler,
        // check whether the invalidation is about itself
        if (cnline && (cnline == inv_ctx->cnline_ptr))
        {
            ret = RET_CNLINE_NOT_LOCKED;
        }
        else 
        {
            if(spin_trylock(&inv_ctx->cnline_ptr->evict_lock))  //check concurrent invalidation
            {
                ret = RET_CNLINE_LOCKED;
            }else{
                ret = RET_CNLINE_NOT_LOCKED;   // there is cnline but couldn't hold evict_lock
            }
        }
    }
    spin_unlock(&hash_list_lock);
    spin_unlock(&cnthread_lock);
    return ret;
}

static int cnthread_find_and_evict(unsigned int tgid, unsigned long addr, unsigned long dir_size,
                            int is_invalid, int data_required,
                            int remove_data, char* inv_ack_buf,
                            struct cnthread_inv_msg_ctx *inv_ctx, int from_preampt)
{
    unsigned long start_addr = (addr / dir_size) * dir_size;
    unsigned long cache_addr = addr & CNTHREAD_CACHLINE_MASK;
    unsigned long end_addr = start_addr + dir_size;
    unsigned long tmp_addr, next_inval_addr;
    struct cnthread_req *victim = NULL;
    struct cnthread_cacheline *cnline = NULL;
    int removed = 0;
    int cnt = 0, wcnt = 0, ret = 0, rpage_idx=0, range_inv = 0;
    struct cache_waiting_node *w_node = NULL;
    unsigned long start, start_ack, end, proc_time, start_lock, end_lock;
    u16 state = 0, sharer = 0, r_size = 0, r_lock = 0, r_cnt = 0;
    int _cnthread_is_lock[CNTHREAD_CACHELINE_SIZE_IN_PAGES] = {0};
    PROFILE_POINT_TIME(CN_inv_target_data)
    PROFILE_POINT_TIME(CN_inv_total)
    PROFILE_POINT_TIME(CN_inv_total_dum)
    PROFILE_POINT_TIME(CN_inv_total_prmpt)
    PROFILE_POINT_TIME(CN_inv_target_data_prmpt)
    PROFILE_POINT_TIME(CN_inv_dummy_data_prmpt)
    PROFILE_POINT_TIME(CN_inv_other_data_prmpt)
    // make compiler happy
    (void)wcnt;
    addr &= PAGE_MASK; // may not be needed
    // send_cache_dir_check(tgid, addr & CNTHREAD_CACHLINE_MASK,
    //                      &state, &sharer, CN_SWITCH_REG_SYNC_NONE); // pull state from the switch (**high overhead**)
    pr_cache("Eviction triggered-tgid: %u, addr: 0x%lx [0x%lx - 0x%lx], inval: %d, Dreq: %d, Ddel:%d / state: 0x%x, sharer: 0x%x\n",
           tgid, addr, start_addr, end_addr, is_invalid, data_required, remove_data, state, sharer);
    PROFILE_START(CN_inv_total);
    PROFILE_START(CN_inv_total_dum);
    PROFILE_START(CN_inv_total_prmpt);
    // 1) Find target node in hash table
    start = sched_clock();
recheck:
    spin_lock(&cnthread_lock);
    spin_lock(&hash_list_lock);
    cnt = 0;
    if (inv_ctx)
    {
        if (inv_ctx->cnline_ptr && inv_ctx->is_locked)
        {
            cnline = inv_ctx->cnline_ptr;
            cnline->ownership = 0;
            goto hold_ongoing_lock;
        }
    }
    cnline = find_cacheline_no_lock(tgid, cache_addr);
    // 2) Check status of the target cacheline (=cache region of cache directory)
    if (cnline)
    {
        pr_cache("Cacheline found-tgid: %u, addr: 0x%lx - 0x%lx [0x%lx - 0x%lx], used: %d, free [p:%d, c:%d]\n",
                 tgid, start_addr, end_addr, cache_addr, cache_addr + CACHELINE_MAX_SIZE,
                 atomic_read(&cnline->used_page), atomic_read(&cn_free_page_counter), 
                 atomic_read(&cn_free_cacheline_counter));
        // now, this cacheline becomes non-exclusive
        // (we are not using ownership optimization, though)
        cnline->ownership = 0;
        if (is_invalid)     // if it is invaliation not eviction (which means, it should be synchronous)
        {
            if(!spin_trylock(&cnline->evict_lock))  // Check concurrent invalidation
            {
                // Concurrent invalidation happens when invalidation requested for the region begin evicted
                spin_unlock(&hash_list_lock);
                spin_unlock(&cnthread_lock);
                // BUG();  // it can occurs when the program terminates by signal (EXIT syscall during invalidation)
                if ((is_invalid != TRY_INVALIDATION) && !data_required)
                {   // simply return and try next
                    return 0;
                }
                udelay(500);
                goto recheck; // We need lock to send required target data
            }
        }
        else
        {
            if (spin_trylock(&cnline->evict_lock))
            {
                if (atomic_read(&cnline->used_page) <= 0)
                    is_invalid = EVICT_BUT_RETRYING;    // Cleaning up routine
                else
                    spin_unlock(&cnline->evict_lock);   // Back to pre-eviction routine
            }
        }
hold_ongoing_lock:
        if (!spin_trylock(&cnline->on_going_lock))
        {
            if (is_invalid && spin_is_locked(&cnline->evict_lock))
                spin_unlock(&cnline->evict_lock);
            spin_unlock(&hash_list_lock);
            spin_unlock(&cnthread_lock);
            if (!is_invalid && !data_required)
            {   // simply return and try next
                return 0;
            }
            pr_info_ratelimited("Cacheline::Already being evicted-tgid: %u, addr: 0x%lx [0x%lx - 0x%lx], retry: %d\n",
                                tgid, addr, start_addr, end_addr, ret);
            udelay(500);
            goto recheck; //we need this lock to send required target data
        }

        if (!ret)    // retry > 0: already removed the node, so skip
        {
            pr_cache("Cacheline::Remove from list-tgid: %u, addr: 0x%lx [0x%lx - 0x%lx], retry: %d\n",
                     tgid, addr, start_addr, end_addr, ret);
            list_del(&(cnline->node));    // remove from the LRU list
            pr_cache("cnline->node: 0x%lx\n", (unsigned long)&cnline->node);
            atomic_dec(&cnthread_list_counter);
            pr_cache("cnthread_list_counter: %d\n", atomic_read(&cnthread_list_counter));
        }
    }else{
        // We do not hold evict lock here
        if (is_invalid != TRY_INVALIDATION) // no ack for eviction if there was no cacheline
        {
            ; // No cacheline (already cleaned/flushed), just skip
        }else{
            // We have cacheline but do not have the target get (page granularity)
            if (is_invalid && inv_ack_buf)  // no invalidation to wait
            {
#ifndef MIND_USE_TSO
                _cnthread_send_inval_ack(tgid, addr, inv_ack_buf);
                barrier();
#endif
            }
            // if data required, then send it
            if (is_invalid && data_required)
            {
                // Since data was not found, send special message, 
                // which is lookin like a RoCE request from the original requester (to the memory)
                if (unlikely(!inv_ctx))
                    BUG();
                cnthread_send_dummy_data(tgid, addr, inv_ctx);
                pr_cache("Dummy data sent(no cacheline)-tgid: %u, addr: 0x%lx, inval: %d, Dreq: %d, Ddel:%d\n",
                         tgid, addr, is_invalid, data_required, remove_data);
                udelay(10); //to avoid reordered msg
                            // (although reordering will not break the procotol 
                            //  since the up-to-date data is stored in the memory)
            }
            start_ack = sched_clock();
            cnthread_send_finish_ack(tgid, addr & PAGE_MASK, &send_ctx, is_invalid);
            end = sched_clock();
            proc_time = (end > start) ? (end - start) / 1000 : 0;
    #ifndef PRINT_CACHE_COHERENCE
            if (proc_time >= DISAGG_SLOW_ACK_REPORT_IN_USEC)    // report when suspicious
    #endif
            {
                send_cache_dir_full_check(tgid, start_addr, //CNTHREAD_CACHLINE_MASK,
                                          &state, &sharer, &r_size, &r_lock, &r_cnt, CN_SWITCH_REG_SYNC_NONE);
                pr_info("FinA sent-tgid: %u, addr: 0x%lx, inval: %d, Dreq: %d, Ddel:%d, time: %lu us (ack: %ld us)\n",
                    tgid, addr, is_invalid, data_required, remove_data, proc_time,
                    (end > start_ack) ? (end - start_ack) / 1000 : -1);
                pr_info("FinA Reg: state: 0x%x, sharer: 0x%x, size: %u, lock: %u, cnt: %u\n", state, sharer, r_size, r_lock, r_cnt);
            }
        }
        spin_unlock(&hash_list_lock);
        spin_unlock(&cnthread_lock);
        PROFILE_LEAVE(CN_inv_total_dum);
        return 0;
    }
    spin_unlock(&hash_list_lock);
    spin_unlock(&cnthread_lock);

    // 3) Invalidate the target page / data
    // Now we have a cnline (=cache region) to invalidate
    barrier();
    if (is_invalid)
    {
        // send cancel message to all the on-going data fetch
        for (tmp_addr = start_addr; tmp_addr < end_addr; tmp_addr += PAGE_SIZE)
        {
            w_node = find_node(tgid, tmp_addr);
            if (w_node)
            {
                request_unlock_wait_node(w_node);
                cnt++;
            }
        }
    }
    smp_wmb();
    pr_cache("Canceled waiting node [found: %d] - tgid: %u, addr: 0x%lx - 0x%lx, inval: %d, Dreq: %d, Ddel:%d, wCnt: %d, rwCnt: %ld\n",
                cnt, tgid, start_addr, end_addr, is_invalid, data_required, remove_data, wcnt, atomic_long_read(&cnline->access_sem.count));
    start_lock = jiffies;
    victim = find_page_from_cacheline(cnline, addr);
    if (victim)
    {
        w_node = NULL;
        // skip test metadata for eviction
        if (!is_invalid && victim->vma && TEST_is_meta_vma(victim->vma->vm_start, victim->vma->vm_end))
            goto out_evict;

        if (!spin_trylock(&victim->pgfault_lock))
        {
            if (!is_invalid)
            {
                //cancel eviction
                goto out_evict;
            }
            // under fetched (page fault)
            else if (atomic_read(&victim->is_used) != IS_PAGE_UNUSED)
            {
                // Try to cancel waiting node again
                // If it was already fetched, wait for the page fault handler
#ifdef CNTHREAD_ACTIVE_CANCEL_PGFAULT
                w_node = find_node(tgid, addr);
                if (w_node)
                {
                    request_unlock_wait_node(w_node);
                }else 
#endif
                if((is_invalid == EVICT_FORCED) && (cnt == 0))
                {
                    spin_lock_init(&victim->pgfault_lock);  // set to unlocked
                }
                spin_lock(&victim->pgfault_lock);
            }
            else
            {
               goto send_target_dummy_data;
            }
        }
        // now we hold pgfault_lock
        end_lock = jiffies;
        if (unlikely((end_lock > start_lock) && (jiffies_to_usecs(end_lock - start_lock) >= DISAGG_SLOW_LOCK_REPORT_IN_USEC)))    // report when suspicious
        {
            // send_cache_dir_full_check(tgid, start_addr, //CNTHREAD_CACHLINE_MASK,
            //                         &state, &sharer, &r_size, &r_lock, &r_cnt, CN_SWITCH_REG_SYNC_NONE);
            printk(KERN_DEFAULT "ERROR: suspcious locking time (tar) - tgid: %u, addr: 0x%lx, w_node: 0x%lx, inval: %d, Dreq: %d, Ddel:%d, used_page[%d] time[%u ms]\n",
                    tgid, addr, (unsigned long)w_node, is_invalid, data_required, remove_data, atomic_read(&cnline->used_page), jiffies_to_msecs(end_lock - start_lock));
        }
        // since we hold pgfault_lock: IS_PAGE_UNUSED or IS_PAGE_USED (not IS_PAGE_RECEIVED)
        if (victim->vma && (atomic_read(&victim->is_used) != IS_PAGE_UNUSED))
        {
            u32 req_qp = 0;
            int dummy_ctx = 0;
            if ((is_invalid == TRY_INVALIDATION) && inv_ctx)
                req_qp = (get_id_from_requester(inv_ctx->rdma_ctx.requester) * DISAGG_QP_PER_COMPUTE) + inv_ctx->original_qp;
            // now we have lock, so there must be no intermediate state such as IS_PAGE_RECEIVED
            if (unlikely(((atomic_read(&cnline->used_page) <= 0) && (atomic_read(&victim->is_used) != IS_PAGE_UNUSED)) 
                         || (atomic_read(&victim->is_used) == IS_PAGE_RECEIVED)))
            {
                printk(KERN_WARNING "ERROR: Suspicious (tar) used_page [%d] is_used [%d] - tgid: %u, addr: 0x%lx, inval: %d, Dreq: %d, Ddel:%d\n",
                    atomic_read(&cnline->used_page), atomic_read(&victim->is_used), tgid, addr, is_invalid, data_required, remove_data);
                if (atomic_read(&cnline->used_page) <= 0)
                    atomic_set(&cnline->used_page, 1);  // at least one for itself
            }
            PROFILE_START(CN_inv_target_data);
            PROFILE_START(CN_inv_target_data_prmpt);
            // Send data only when data push was required
            if (!inv_ctx)
            {
                inv_ctx = kzalloc(sizeof(*inv_ctx), GFP_KERNEL);
                dummy_ctx = 1;
            }
            inv_ctx->is_target_data = data_required;
            inv_ctx->remove_data = remove_data;
            inv_ctx->is_invalid = is_invalid;
            ret = ___cnthread_evict_one(victim, addr, _threshold, _high_threshold,
                                        inv_ctx, inv_ack_buf, req_qp);
            if (dummy_ctx)  //FIXME: unlikely?—eviction and termination
            {
                kfree(inv_ctx);
                inv_ctx = NULL;
            }
            
            if (is_invalid == TRY_INVALIDATION) // Only for invalidations
            {
                if (from_preampt)
                    PROFILE_LEAVE(CN_inv_target_data_prmpt);
                else
                    PROFILE_LEAVE(CN_inv_target_data);
            }
            if (unlikely(ret < 0))
                BUG();
            if (ret > 0)    // evition skipped
                ;   // continue
            else if (remove_data) // page removed
                removed++;
        }else{
            // No page / data
            spin_unlock(&victim->pgfault_lock);
            goto send_target_dummy_data;
        }
        // If it was already cleared -> already unlocked
        // If it was not cleared -> we need to unlock
        if ((victim->cacheline == cnline) && spin_is_locked(&victim->pgfault_lock))
            spin_unlock(&victim->pgfault_lock);
    }
    else
    {
send_target_dummy_data:
        if (is_invalid && data_required)    // ACK w/ data is only required for invalidation
        {
            if (unlikely(!inv_ctx))
                BUG();
            PROFILE_START(CN_inv_dummy_data_prmpt);
            cnthread_send_dummy_data(tgid, addr, inv_ctx);
            if (from_preampt)
                PROFILE_LEAVE(CN_inv_dummy_data_prmpt);
            pr_cache("Dummy data sent-tgid(w/ cacheline): %u, addr: 0x%lx, inval: %d, Dreq: %d, Ddel:%d\n",
                        tgid, addr, is_invalid, data_required, remove_data);
            udelay(10); // To avoid reordered data - ACK (it must not be in this order: ACK - data)
        }
    }
    barrier();
    // Now target page is cleared
    if (is_invalid && inv_ack_buf)
    {
        if (is_invalid == TRY_INVALIDATION)
        {
#ifndef MIND_USE_TSO
            _cnthread_send_inval_ack(tgid, addr, inv_ack_buf);
            barrier();
#endif
        }
    }
    pr_cache("Target/first page invalidated - tgid: %u, addr: 0x%lx, inval: %d, Dreq: %d, Ddel:%d\n",
             tgid, addr, is_invalid, data_required, remove_data);

    // 4) Invalidate remaining pages in the same cache directory
    rpage_idx = -1; range_inv = 0;
    next_inval_addr = start_addr;
    for (tmp_addr = start_addr; tmp_addr < end_addr; tmp_addr += PAGE_SIZE)
    {
        rpage_idx ++;
#if 0   // Preemptive invalidation request receiving routine : DISABLED NOW (not needed)
        if ((rpage_idx + 1) % PREEMPTIVE_ACK_CHK_PER_INV == 0)
        {
            // Check and try to serve invalidation ACKs
            // try_invalidation_lookahead(PROFILE_CNTHREAD_INV_ACK_SERV_FROM_PRMPT, SERVE_ACK_PER_RANGE_INV);
        }
#endif

        if (tmp_addr == addr)   // Skip the target (already flushed above)
            continue;
        // Find victim and try to evict it
        victim = find_page_from_cacheline(cnline, tmp_addr);
        if (victim)
        {
            start_lock = end_lock = 0;
            w_node = NULL;
            if (!spin_trylock(&victim->pgfault_lock))
            {
                // Under data fetch (=by the page fault handler)
                if (!is_invalid)
                {
                    range_inv = -1; // Failed
                    break;
                }
                else
                {
                    // Already fetched, wait for the page fault handler
#ifdef CNTHREAD_ACTIVE_CANCEL_PGFAULT
                    w_node = find_node(tgid, tmp_addr);
                    if (w_node)
                    {
                        request_unlock_wait_node(w_node);
                    }
#endif
                    smp_wmb();
                    if (atomic_read(&victim->is_used) != IS_PAGE_UNUSED)
                    {
                        if ((is_invalid == EVICT_FORCED) && (cnt == 0))
                        {
                            spin_lock_init(&victim->pgfault_lock);
                        }
                        start_lock = jiffies;
                        spin_lock(&victim->pgfault_lock);
                        end_lock = jiffies;
                    }
                    else
                    {
                        // Skip this page: no data to flush, and the page fault handler will fail since we canceled waiting node
                        continue;
                    }
                }
            }
            if (unlikely((end_lock > start_lock) && (jiffies_to_usecs(end_lock - start_lock) > DISAGG_SLOW_LOCK_REPORT_IN_USEC)))    // report when suspicious
            {
                printk(KERN_DEFAULT "ERROR: suspcious locking time (oth) - tgid: %u, addr: 0x%lx [0x%lx - 0x%lx], w_node: 0x%lx, inval: %d, Dreq: %d, Ddel:%d, time: %u us\n",
                       tgid, addr, start_addr, end_addr, (unsigned long)w_node, 
                       is_invalid, data_required, remove_data, jiffies_to_usecs(end_lock - start_lock));
            }
            // Now we hold pgfault_lock
            if (likely(victim->cacheline == cnline) && (atomic_read(&victim->is_used) != IS_PAGE_UNUSED))
                _cnthread_is_lock[rpage_idx] = 1;   // We will flush those pages below
            else
                spin_unlock(&victim->pgfault_lock); // Unlock to let page fault handler clear this page
            // now we hold pgfault_lock: IS_PAGE_UNUSED or IS_PAGE_USED (not IS_PAGE_RECEIVED)
        }
    }
    // For any remaining pages
    pr_cache("Try to evict remaining pages[%d] - tgid: %u, addr: 0x%lx - 0x%lx, inval: %d, Dreq: %d, Ddel:%d\n",
             range_inv, tgid, next_inval_addr, tmp_addr, is_invalid, data_required, remove_data);
    if (!range_inv) // It if was successful
    {
        PROFILE_START(CN_inv_other_data_prmpt);
        __cnthread_evict_range(cnline, next_inval_addr, tmp_addr - next_inval_addr, addr, remove_data, is_invalid, _cnthread_is_lock);
        if (from_preampt)
            PROFILE_LEAVE(CN_inv_other_data_prmpt);
        next_inval_addr = tmp_addr; // FIXME: it doesn't seem to be needed

    }
    rpage_idx = -1;
    for (tmp_addr = start_addr; tmp_addr < end_addr; tmp_addr += PAGE_SIZE)
    {
        rpage_idx ++;
        if (tmp_addr == addr)   // Skip the target (already flushed above)
            continue;
        if (_cnthread_is_lock[rpage_idx])
        {
            victim = find_page_from_cacheline(cnline, tmp_addr);
            // FIXME: if it was locked, it should be cleared so victim here should be NULL
            if (victim && (victim->cacheline == cnline) && spin_is_locked(&victim->pgfault_lock))
            {
                spin_unlock(&victim->pgfault_lock);
            }
        }
    }
#ifdef PRINT_CACHE_COHERENCE
    if (is_invalid && data_required)
    {
        pr_cache("After Data sent-tgid(w/ cacheline): %u, addr: 0x%lx, inval: %d, Dreq: %d, Ddel: %d / state: 0x%x, sharer: 0x%x\n",
            tgid, addr, is_invalid, data_required, remove_data,
            state, sharer);
    }
#endif
    // Send final data ack - invalidated or evicted whole cacheline
    if (is_invalid || atomic_read(&cnline->used_page) <= 0)
    {
        if (is_invalid)
        {
            
            start_ack = jiffies;
            // @is_invalid = 0: eviction, TRY_INVALIDATION: actual invalidation, EVICT_BUT_RETRYING: eviction in invalidate mode
            if (is_invalid == TRY_INVALIDATION) // invalidation
            {
                cnthread_send_finish_ack(tgid, addr & PAGE_MASK, &send_ctx, 1);
            }else{
                // send ack for all the evicted pages, since we do not know exact cache region size now
                for (tmp_addr = start_addr; tmp_addr < end_addr; tmp_addr += PAGE_SIZE)
                    cnthread_send_finish_ack(tgid, tmp_addr, &send_ctx, 0);
            }
            end = jiffies;
            proc_time = (end > start) ? jiffies_to_usecs(end - start) : 0;
#ifndef PRINT_CACHE_COHERENCE
            if (unlikely(proc_time >= DISAGG_SLOW_LOCK_REPORT_IN_USEC))    // report when suspicious
#endif
            {
                // send_cache_dir_check(tgid, start_addr,    // CNTHREAD_CACHLINE_MASK,
                //                      &state, &sharer, CN_SWITCH_REG_SYNC_NONE); // pull state from the switch (**high overhead**)
                send_cache_dir_full_check(tgid, start_addr,
                                        &state, &sharer, &r_size, &r_lock, &r_cnt, CN_SWITCH_REG_SYNC_NONE);
                printk(KERN_WARNING "Fin2 Ack sent-tgid(w/ cacheline): %u, addr: 0x%lx (0x%lx), inval: %d, Dreq: %d, Ddel: %d, Time: %lu us (ack: %u us)\n",
                        tgid, addr, start_addr, is_invalid, data_required, remove_data,
                        proc_time, (end >= start_ack) ? jiffies_to_usecs(end - start_ack) : 0);
                printk(KERN_WARNING "Fin2 Reg: state: 0x%x, sharer: 0x%x, size: %u, lock: %u, cnt: %u\n", state, sharer, r_size, r_lock, r_cnt);
            }
        }else{
            // First eviction pass but no data -- rollback cnline structure, exit and try again
            goto out_evict;
        }
    }

    if (((is_invalid == EVICT_BUT_RETRYING) || (is_invalid == EVICT_FORCED))   // only when we removed all the pages in this 2MB cacheline
        && remove_data && (atomic_read(&cnline->used_page) <= 0))
    {
        spin_lock(&hash_list_lock);
        hash_del(&cnline->hnode);
        smp_wmb();
        add_cacheline_to_free_list(cnline); // clean and add to the free list
        spin_unlock(&hash_list_lock);
        // cnthread_print_cache_status();
    }
    else
    {
out_evict:
        // Put the cnline back to the list and we will try again next time
        // (only possible for eviction)
        spin_lock(&cnthread_lock);
        list_add(&cnline->node, &cn_handler_lru_list);
        atomic_inc(&cnthread_list_counter);
        spin_unlock(&cnthread_lock);
    }
    if (is_invalid && spin_is_locked(&cnline->evict_lock))
        spin_unlock(&cnline->evict_lock);
    spin_unlock(&cnline->on_going_lock);

    if (is_invalid)
    {
        if (from_preampt)
            PROFILE_LEAVE(CN_inv_total_prmpt);
        else
            PROFILE_LEAVE(CN_inv_total);
    }
    pr_cache("End evcit-tgid(w/ cacheline): %u, addr: 0x%lx, inval: %d, Dreq: %d, Ddel: %d, Removed: %d, Remaining: %d\n",
             tgid, addr, is_invalid, data_required, remove_data, removed, atomic_read(&cnline->used_page));
    return CNTHREAD_TRY_EVICTION_AGAIN;
}

int cnthread_check_free_space(void)
{
    if (likely(!list_empty(&cn_free_cacheline_list) && atomic_read(&cn_free_page_counter) > 0))
    {
        return 0;
    }
    else
    {
#ifdef PRINT_CACHE_COHERENCE
        cnthread_print_cache_status();
#endif
        return -1;
    }
}

int cnthread_evict_one(int threshold, int high_threshold)
{
    int cur_size, cur_cnt;
    struct cnthread_cacheline *cnline = NULL;
    int ret = 0;
    unsigned long addr = 0;
    unsigned int tgid = 0;

    // Get one from the list
    if (cnthread_is_pressure())
    {
        spin_lock(&cnthread_lock);
        // Now we need to evict one: from the last (oldest)
        // Follow FIFO model in LegoOS (instead of LRU or LFU)
        if (cn_handler_lru_list.prev && (cn_handler_lru_list.prev != &cn_handler_lru_list))
        {

            cnline = container_of(cn_handler_lru_list.prev, struct cnthread_cacheline, node);
            tgid = cnline->tgid;
            addr = cnline->addr;
        }
        spin_unlock(&cnthread_lock);
    }

    // There is victim?
    if (!cnline)
    {
        return -1;
    }

    // Main eviction function
    // THIS SHOULD BE ALL PAGES IN A CACHELINE (=cache region with maximum size)
    ret = cnthread_find_and_evict(tgid, addr, CNTHREAD_CACHELINE_SIZE_IN_PAGES * PAGE_SIZE,
                                  0, 0, CNTHREAD_BATCH_IN_CACHELINE, NULL, NULL, 0);
    if (ret == CNTHREAD_TRY_EVICTION_AGAIN)
    {
        // Here we print number before this eviction
        cur_cnt = (int)atomic_read(&cnthread_evict_counter);
        cur_size = atomic_read(&cnthread_list_counter);
        if (unlikely(cur_cnt % 1000 == 0))
        {

            if ((cur_size > high_threshold) || (cur_cnt % 10000 == 0))
            {
                atomic_set(&cnthread_evict_counter, 0);
                cnthread_print_cache_status();
            }
        }
        atomic_inc(&cnthread_evict_counter);
    }
    return ret;
}

static atomic_t DEBUG_trigger_eviction;
static unsigned int DEBUG_eviction_tgid = 0;
static unsigned long DEBUG_eviction_addr = 0;

void DEBUG_trigger_evict(unsigned int tgid, unsigned int addr)
{
    int tval = 0;
    DEBUG_eviction_tgid = tgid;
    DEBUG_eviction_addr = addr;
    barrier();
    atomic_set(&DEBUG_trigger_eviction, 1);
    while(1)
    {
        msleep(100);
        tval = atomic_read(&DEBUG_trigger_eviction);
        if(!tval)
            break;
    }
}
EXPORT_SYMBOL(DEBUG_trigger_evict);

unsigned long get_dummy_page_dma_addr(int cpu_id)
{
    return dummy_page_dma_addr[cpu_id];
}

void *get_dummy_page_buf_addr(int cpu_id)
{
    return dummy_page_buf[cpu_id];
}

unsigned long get_dummy_inv_page_dma_addr(void)
{
    return dummy_inv_page_dma_addr;
}

static void cnthread_initial_kmap(void)
{
    struct cnthread_req *cnreq = NULL;
    int i = 0;
    spin_lock(&free_page_get_lock);
    list_for_each_entry(cnreq, &cn_free_page_list, node)
    {
        cnreq->kmap = kmap_atomic(cnreq->kpage);
        cnreq->dma_addr = mapping_dma_region_page(cnreq->kmap);
    }
    spin_unlock(&free_page_get_lock);
    for (i = 0; i < DISAGG_NUM_CPU_CORE_IN_COMPUTING_BLADE * 3; i++)
    {
        dummy_page_buf[i] = kmalloc(PAGE_SIZE, GFP_KERNEL);
        memset(dummy_page_buf[i], 0, PAGE_SIZE);
        dummy_page_dma_addr[i] = mapping_dma_region_page(dummy_page_buf[i]);
    }
    dummy_inv_page_buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
    memset(dummy_inv_page_buf, 0, PAGE_SIZE);
    dummy_inv_page_dma_addr = mapping_dma_region_page(dummy_inv_page_buf);
    pr_info("* Kmapping and DMA addresses are initialied\n");
}

// Currently, it just delete from the list
// since we assume the actual page was unmapped by using vm_mummap
// (so should NOT be used *inside* invalidation routine)
int _cnthread_delete_from_list(u16 tgid, unsigned long address, int already_locked)
{
    struct cnthread_req *tmp;
    struct cnthread_cacheline *cnline;
    address &= PAGE_MASK;
    // Check there is already populated cacheline
recheck:
    if (!already_locked)
        spin_lock(&hash_list_lock);
    tmp = find_page_no_lock(tgid, address);
    if (!tmp || !tmp->cacheline || (atomic_read(&tmp->is_used) != IS_PAGE_USED))
    {
        // Under eviction, will be deleted automatically
        if (!already_locked)
            spin_unlock(&hash_list_lock);
        return 0;
    }

    cnline = tmp->cacheline;
    if(!spin_trylock(&cnline->on_going_lock))
    {
        if (!already_locked)
            spin_unlock(&hash_list_lock);
        printk(KERN_DEFAULT "Is locked....retry-tgid: %u, address: 0x%lx\n", (unsigned int)tgid, address);
        msleep(1000);
        goto recheck;
    }

    // Now we are holding on_going_lock
    if (!already_locked)
        spin_unlock(&hash_list_lock);
    // Clean the metadata
    cnthread_put_page(tmp);
    if (atomic_read(&cnline->used_page) == 0 || atomic_dec_and_test(&cnline->used_page))
    {
        // Remove this cache region (=cacheline structure)
        pr_cache("Clean cacheline-tgid: %u, address: 0x%lx\n", (unsigned int)tgid, address);
        if (!already_locked)
        {
            spin_lock(&cnthread_lock);
            spin_lock(&hash_list_lock);
        }
        list_del(&cnline->node);
        atomic_dec(&cnthread_list_counter);
        hash_del(&cnline->hnode);
        if (!already_locked)
        {
            spin_unlock(&hash_list_lock);
            spin_unlock(&cnthread_lock);
         }
        add_cacheline_to_free_list(cnline);
    }
    spin_unlock(&cnline->on_going_lock);
    return 0;
}

int cnthread_delete_from_list(u16 tgid, unsigned long address)
{
    return _cnthread_delete_from_list(tgid, address, 0);
}

int cnthread_delete_from_list_no_lock(u16 tgid, unsigned long address)
{
    return _cnthread_delete_from_list(tgid, address, 1);
}

DEFINE_PROFILE_POINT(cnthread_delete_one_cnreq) 
static int _cnthread_delete_all_request(u16 tgid, int flush_data)
{
    PROFILE_POINT_TIME(cnthread_delete_one_cnreq)
    struct cnthread_cacheline *cline;
    struct list_head *next = NULL;
    int deleted = 0;
    unsigned long addr;
    printk(KERN_DEFAULT "Del all req.: tgid: %u\n", (unsigned int)tgid);

restart_search:
    spin_lock(&cnthread_lock);
    spin_lock(&hash_list_lock);
    if (!list_empty(&cn_handler_lru_list))
    {
        cline = container_of(cn_handler_lru_list.next, struct cnthread_cacheline, node);
        while(1)
        {
            next = cline->node.next;
            if (cline && cline->tgid == tgid)
            {
                if (flush_data)
                {
                    addr = cline->addr;
                    spin_unlock(&hash_list_lock);
                    spin_unlock(&cnthread_lock);
                    cnthread_find_and_evict(tgid, addr, CACHELINE_MAX_SIZE, EVICT_FORCED, 0, (CACHELINE_MAX_SIZE/PAGE_SIZE), NULL, NULL, 0);
                    goto restart_search;
                }
                else
                {
                    unsigned long tmp_addr;
                    for (tmp_addr = cline->addr; tmp_addr < cline->addr + CACHELINE_MAX_SIZE; tmp_addr += PAGE_SIZE)
                        cnthread_delete_from_list_no_lock(tgid, tmp_addr); // cnthread_lock and hash_list_lock was already hold
                }
                deleted ++;
            }

            if (!next || next == &cn_handler_lru_list)
                break;

            cline = container_of(next, struct cnthread_cacheline, node);
        }
    }
    spin_unlock(&hash_list_lock);
    spin_unlock(&cnthread_lock);
    if (deleted > 0)
    {
        cnthread_delete_all_request(tgid);  // Check again (to remove pending ones)
    }
    return 0;
}

int cnthread_delete_all_request(u16 tgid)
{
    return _cnthread_delete_all_request(tgid, 0);
}

int cnthread_flush_all_request(u16 tgid)
{
    return _cnthread_delete_all_request(tgid, 1);
}

int cnthread_clean_up_non_existing_entry(u16 tgid, struct mm_struct *mm)
{
    struct cnthread_cacheline *cnline;
    struct list_head *next = NULL;
    struct vm_area_struct *vma;

    spin_lock(&cnthread_lock);
    spin_lock(&hash_list_lock);
    if (!list_empty(&cn_handler_lru_list))
    {
        cnline = container_of(cn_handler_lru_list.next, struct cnthread_cacheline, node);
        while (1)
        {
            next = cnline->node.next;
            if (cnline && cnline->tgid == tgid)
            {
                unsigned long tmp_addr;
                for (tmp_addr = cnline->addr; tmp_addr < cnline->addr + CACHELINE_MAX_SIZE; tmp_addr += PAGE_SIZE)
                {
                    // Check VMA
                    vma = find_vma(mm, tmp_addr); // current or next vma
                    if (!vma || !(vma->vm_start > tmp_addr))    // no vma
                    {
                        cnthread_delete_from_list_no_lock(tgid, tmp_addr); // cnthread_lock and hash_list_lock was already hold
                    }
                }
                // Update mm (it was EXECed!)
                if (cnline->mm != mm)
                {
                    cnline->mm = mm;
                }
            }
            if (!next || next == &cn_handler_lru_list)
                break;

            cnline = container_of(next, struct cnthread_cacheline, node);
        }
    }
    spin_unlock(&hash_list_lock);
    spin_unlock(&cnthread_lock);
    return 0;
}

int DEBUG_try_evict(u32 tgid, u64 address)
{
    struct fault_data_struct payload;
    int res = -1;
    u32 tot_size = sizeof(struct fault_reply_struct);
    struct fault_reply_struct reply;
    void *buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
    if (unlikely(!buf))
    {
        BUG();
    }
    payload.tgid = tgid;
    payload.address = address;
    payload.data = buf;
    payload.data_size = PAGE_SIZE;
    printk(KERN_DEFAULT "Evict to 0x%llx...\n", address);
    // Check the size of the received data: it should have at least default struct size
    res = send_msg_to_memory_rdma(DISSAGG_DATA_PUSH, &payload, PAGE_SIZE,
                                  &reply, tot_size);    // FIXME: tot_size is not used inside
    kfree(buf);
    return res;
}

static inline void create_invalidation_rdma_ack(char *inval_buf, u64 fva, u32 rkey, u32 qp)
{
    u16 pkey = ROCE_DEFAULT_PKEY;
    // Opcode @ CACHELINE_ROCE_OFFSET_TO_OPCODE
    u16 preamble = CACHELINE_ROCE_ACK_PREAMBLE;
    memcpy(&inval_buf[CACHELINE_ROCE_OFFSET_TO_OPCODE], &preamble, sizeof(preamble));
    // Pkey
    pkey = __cpu_to_be16(pkey);
    memcpy(&(inval_buf[CACHELINE_ROCE_OFFSET_TO_PKEY]), &pkey, sizeof(pkey));
    // QP
    qp = CACHELINE_BYPASS_MULTICAST_MASK;   // last 16 bits, to make it always pass multicasting mask
    qp = __cpu_to_be32(qp);
    memcpy(&(inval_buf[CACHELINE_ROCE_OFFSET_TO_QP]), &qp, sizeof(qp));
    // virtual address
    fva = __cpu_to_be64(fva); // network endian to host endian (e.g., big to little in usual X86)
    memcpy(&(inval_buf[CACHELINE_ROCE_OFFSET_TO_VADDR]), &fva, sizeof(fva));
    // rkey
    rkey &= ~((u32)CACHELINE_ROCE_RKEY_INVALIDATION_MASK);          // clear invalidation mask
    rkey = __cpu_to_be32((rkey & ~((u32)0xf)) | CACHE_STATE_INV_ACK); // set state as CACHE_STATE_INV_ACK
    memcpy(&(inval_buf[CACHELINE_ROCE_OFFSET_TO_RKEY]), &rkey, sizeof(rkey));
    // DEBUG
    // print_raw(inval_buf);
}

static __always_inline void inc_inval_rdma_ack(int head_idx)
{
    // for DISAGG_QP_NUM_INVAL_BUF
    inval_buf_ack_head[head_idx] = (inval_buf_ack_head[head_idx] + 1) % CACHELiNE_ROCE_INVAL_BUF_LENGTH;
}

static inline void *check_inval_rdma_ack(int *msg_is_empty, int head_idx)
{
    unsigned int cur_ack_pos;
    if (unlikely(!base_inval_buf[head_idx]))
        return NULL;
    // Check current head
    cur_ack_pos = inval_buf_ack_head[head_idx] * CACHELINE_ROCE_HEADER_LENGTH;
    if (base_inval_buf[head_idx][cur_ack_pos])
        *msg_is_empty = 0;
    else
        *msg_is_empty = 1;
    // Is there a new data?
    return ((base_inval_buf[head_idx][cur_ack_pos] & 0xff) == (CACHELINE_ROCE_ACK_PREAMBLE_8U)) ? &(base_inval_buf[head_idx][cur_ack_pos]) : NULL;
}

static inline int check_inval_rdma_type(unsigned char* rdma_buf)
{
    u32 ret = *((u32 *)(&(rdma_buf[CACHELINE_ROCE_OFFSET_TO_RKEY])));
    ret = __be32_to_cpu(ret);
    if (ret & CACHELINE_ROCE_RKEY_INVALIDATION_MASK)
    {
        u32 qp_val = *((u32 *)(&(rdma_buf[CACHELINE_ROCE_OFFSET_TO_QP])));
        qp_val = __be32_to_cpu(qp_val);
        //
        if (qp_val & CACHELINE_ROCE_QP_INV_REQUESTER)
            return MSG_OVER_RDMA_SELF_INV;
        else
            return MSG_OVER_RDMA_INV_REQ;
    }else{
        return MSG_OVER_RDMA_ACK;
    }
}

static void inval_rdma_retrieve_base(struct cnthread_rdma_msg_ctx *ctx, unsigned char* rdma_buf)
{
    if (unlikely(!ctx))
        return;

    ctx->ip_val = *((u32*)(&(rdma_buf[CACHELINE_ROCE_VOFFSET_TO_IP])));
    // Find vaddr
    ctx->fva = *((u64 *)(&(rdma_buf[CACHELINE_ROCE_OFFSET_TO_VADDR])));
    ctx->fva = __be64_to_cpu(ctx->fva); // Network endian to host endian (e.g., big to little in usual X86)
    ctx->ret = *((u32 *)(&(rdma_buf[CACHELINE_ROCE_OFFSET_TO_RKEY])));
    ctx->ret = __be32_to_cpu(ctx->ret);
    ctx->pid = (unsigned int)(ctx->fva >> 48);
    ctx->vaddr = ctx->fva & (~MN_VA_PID_BIT_MASK); // Pure virtual address (48bits)
    ctx->state = ctx->ret & CACHELINE_ROCE_RKEY_STATE_MASK;
    pr_cache("[cRDMA] Buf[0x%lx] || FVA: 0x%llx => PID: %u, VA: 0x%llx || RKEY: 0x%x, IP: 0x%lx\n",
                (unsigned long)rdma_buf, ctx->fva, ctx->pid, ctx->vaddr, ctx->ret, (unsigned long)ctx->ip_val);
}

static void inval_rdma_retrieve_ext(struct cnthread_rdma_msg_ctx *ctx, unsigned char* rdma_buf)
{
    if (unlikely(!ctx))
        return;
    ctx->requester = *((u16 *)(&(rdma_buf[CACHELINE_ROCE_OFFSET_TO_PKEY])));
    ctx->requester = __be16_to_cpu(ctx->requester);
    ctx->qp_val = *((u32 *)(&(rdma_buf[CACHELINE_ROCE_OFFSET_TO_QP])));
    ctx->qp_val = __be32_to_cpu(ctx->qp_val);
    ctx->sharer = ctx->qp_val & CACHELINE_ROCE_QP_SHARER_MASK;
    ctx->psn = *((u32 *)(&(rdma_buf[CACHELINE_ROCE_OFFSET_TO_ACKREQ])));
    ctx->psn = (__be32_to_cpu(ctx->psn) & CACHELINE_ROCE_PSN_MASK);  // the last 24 bits
}

static __always_inline void serve_inval_rdma_ack(unsigned char* rdma_buf, int msg_type)
{
    struct cnthread_rdma_msg_ctx rdma_ctx;
    struct cache_waiting_node *w_node = NULL;
    PROFILE_POINT_TIME(CN_update_roce_recv)
    PROFILE_START(CN_update_roce_recv);
    // Mark that we picked this message
    rdma_buf[0] = 0x0;
    inval_rdma_retrieve_base(&rdma_ctx, rdma_buf);
    w_node = find_node(rdma_ctx.pid, (unsigned long)rdma_ctx.vaddr);
    inval_rdma_retrieve_ext(&rdma_ctx, rdma_buf);   // to get sharer

    // Main message processing routine
    if (msg_type == MSG_OVER_RDMA_SELF_INV)
    {
        int val;
        val = get_target_counter_from_bitmask((u16)rdma_ctx.sharer); // number of sharers in the shared list
        if (w_node)
        {
            pr_cache("[cRDMA] Pre-Ack for Node: 0x%lx, PID: %u, VA: 0x%llx, State: %u, Sharer: 0x%x\n",
                    (unsigned long)w_node, rdma_ctx.pid, rdma_ctx.vaddr, 
                    (unsigned int)rdma_ctx.state, (unsigned int)rdma_ctx.sharer);
            set_target_counter(w_node, val - 1);
        }
        else
        {
#ifndef MIND_USE_TSO
            pr_info("ERROR: Cannot find waiting node for PID: %u, VA: 0x%llx\n",
                    rdma_ctx.pid, rdma_ctx.vaddr);
#endif
        }
    }
    else if(msg_type == MSG_OVER_RDMA_ACK)
    {
        if (w_node)
        {
            pr_cache("[cRDMA] Ack for Node: 0x%lx, PID: %u, VA: 0x%llx, State: %u, Sharer: 0x%x\n",
                    (unsigned long)w_node, rdma_ctx.pid, rdma_ctx.vaddr, 
                    (unsigned int)rdma_ctx.state,  (unsigned int)rdma_ctx.sharer);
            if (rdma_ctx.state == CACHE_STATE_INV_ACK)
            { // ACK from other node
                increase_wait_counter(w_node);
            }
            else
            { // ACK directly from switch
                set_target_counter(w_node, 0); // all ack received
            }
            PROFILE_LEAVE(CN_update_roce_recv);
        }
        else
        {
#ifndef MIND_USE_TSO
            pr_info("[cRDMA] Cannot find waiting node for PID: %u, VA: 0x%llx, State: %u\n",
                    rdma_ctx.pid, rdma_ctx.vaddr, (unsigned int)rdma_ctx.state);
#endif
        }
    }else{
        BUG();  // ERROR: Unknown message type
    }
}

static __always_inline int check_inval_req_list_and_try(int from_inv, struct cnthread_cacheline *cnline)
{
    int res = 0;
    struct cnthread_inv_msg_ctx *inv_ctx = NULL;
    struct cnthread_rdma_msg_ctx *rdma_ctx = NULL;
    PROFILE_POINT_TIME(CN_update_roce_inv_req_evt)
    PROFILE_POINT_TIME(CN_update_roce_inv_req_inv)
    PROFILE_POINT_TIME(CN_update_roce_inv_req_prmpt)
    PROFILE_POINT_TIME(CN_update_roce_inv_queue)
    PROFILE_POINT_TIME(CN_latency_10us)
    PROFILE_POINT_TIME(CN_latency_50us)
    PROFILE_POINT_TIME(CN_latency_100us)
    PROFILE_POINT_TIME(CN_latency_250us)
    PROFILE_POINT_TIME(CN_latency_500us)
    PROFILE_POINT_TIME(CN_latency_1000us)

    // Check without lock (no need to be accurate, we will retry)
    if (list_empty(&cn_inval_req_list))
    {
        return -1;
    }

    spin_lock(&cnthread_inval_req_list_lock);
    if (list_empty(&cn_inval_req_list))
    {
        spin_unlock(&cnthread_inval_req_list_lock);
        return -1;
    }
    inv_ctx = (struct cnthread_inv_msg_ctx *)container_of(cn_inval_req_list.prev, struct cnthread_inv_msg_ctx, node);
    list_del(&inv_ctx->node);
    spin_unlock(&cnthread_inval_req_list_lock);

    // Check lock
    if (inv_ctx)
    {
        rdma_ctx = &inv_ctx->rdma_ctx;
        res = check_and_hold_evict_lock(rdma_ctx->pid, rdma_ctx->vaddr, inv_ctx, cnline);
        inv_ctx->is_locked = (res == RET_CNLINE_LOCKED) ? 1 : 0;
        if (res != RET_CNLINE_NOT_LOCKED)   // locked or cnline not found
        {
            int data_required;
            u32 inval_mod;
            int shared_inval, self_request;
            unsigned int dir_size;
            volatile unsigned long debug_jiff = 0;
            int queue_length = 0;
            // Counters for invalidation queue
            atomic_dec(&cnthread_inv_req_counter);  // do not need to be accurate and synchronized (in timing)
            PROFILE_ADD_MEASUREMENT(CN_update_roce_inv_queue, (unsigned long)(sched_clock() - inv_ctx->last_update));
            inv_ctx->last_update = sched_clock();
            PROFILE_START(CN_update_roce_inv_req_evt);
            PROFILE_START(CN_update_roce_inv_req_inv);
            PROFILE_START(CN_update_roce_inv_req_prmpt);

            rdma_ctx = &(inv_ctx->rdma_ctx);
            // Original QP used by the requester: right 8 bits, we assume that left 16 bits = 0
            inv_ctx->original_qp = (rdma_ctx->ret & CACHELINE_ROCE_RKEY_QP_MASK) >> CACHELINE_ROCE_RKEY_QP_SHIFT;
            create_invalidation_rdma_ack(inv_ctx->inval_buf, rdma_ctx->fva, rdma_ctx->ret, rdma_ctx->qp_val);
            *((u32*)(&(inv_ctx->inval_buf[CACHELINE_ROCE_VOFFSET_TO_IP]))) = rdma_ctx->ip_val;
            // Invalidation context
            data_required = ((rdma_ctx->ret & CACHELINE_INVALIDATION_DATA_REQ) > 0) ? 1 : 0;
            inval_mod = rdma_ctx->ret & CACHELINE_ROCE_RKEY_INVALIDATION_MASK; // if 0, then not invalidation
            shared_inval = ((inval_mod & CACHELINE_INVALIDATION_SHARED) == CACHELINE_INVALIDATION_SHARED) ? 1 : 0;
            self_request = (rdma_ctx->qp_val & CACHELINE_ROCE_QP_INV_REQUESTER) ? 1 : 0;
            dir_size = (unsigned int)((rdma_ctx->ret & CACHELINE_ROCE_RKEY_SIZE_MASK) >> CACHELINE_ROCE_RKEY_SIZE_SHIFT);
            pr_cache("[cRDMA] Inval (State: %u, Sharer: 0x%04x, Size: %u, ReqId: %d, Type: %c, IamReq: %d, QP: %u) for PID: %u, VA: 0x%llx, IP:0x%x\n",
                    (unsigned int)rdma_ctx->state, (unsigned int)rdma_ctx->sharer, dir_size, get_id_from_requester(rdma_ctx->requester),
                    shared_inval ? 'S' : 'M', self_request, inv_ctx->original_qp, rdma_ctx->pid, rdma_ctx->vaddr, rdma_ctx->ip_val);
            
            queue_length = atomic_read(&cnthread_inv_req_counter);
            PROFILE_START(CN_latency_10us);
            PROFILE_START(CN_latency_50us);
            PROFILE_START(CN_latency_100us);
            PROFILE_START(CN_latency_250us);
            PROFILE_START(CN_latency_500us);
            PROFILE_START(CN_latency_1000us);
            debug_jiff = sched_clock();
            barrier();
            cnthread_find_and_evict(rdma_ctx->pid, rdma_ctx->vaddr, size_index_to_size(dir_size), TRY_INVALIDATION,
                                    data_required, !shared_inval, inv_ctx->inval_buf, inv_ctx, from_inv == PROFILE_CNTHREAD_INV_ACK_SERV_FROM_PRMPT);
            barrier();
            debug_jiff = (unsigned long)(sched_clock() - debug_jiff) / 1000;
            if (debug_jiff <= 10)
                PROFILE_LEAVE(CN_latency_10us);
            else if (debug_jiff <= 50)
                PROFILE_LEAVE(CN_latency_50us);
            else if (debug_jiff <= 100)
                PROFILE_LEAVE(CN_latency_100us);
            else if (debug_jiff <= 250)
                PROFILE_LEAVE(CN_latency_250us);
            else if (debug_jiff <= 500)
                PROFILE_LEAVE(CN_latency_500us);
            else
                PROFILE_LEAVE(CN_latency_1000us);
            kfree(inv_ctx);
            if (from_inv == PROFILE_CNTHREAD_INV_ACK_SERV_FROM_EVICT)
                PROFILE_LEAVE(CN_update_roce_inv_req_evt);
            else if(from_inv == PROFILE_CNTHREAD_INV_ACK_SERV_FROM_INVAL)
                PROFILE_LEAVE(CN_update_roce_inv_req_inv);
            else if(from_inv == PROFILE_CNTHREAD_INV_ACK_SERV_FROM_PRMPT)
                PROFILE_LEAVE(CN_update_roce_inv_req_prmpt);
            return 0;
        }else{
            // Put it back to the list
            spin_lock(&cnthread_inval_req_list_lock);
            list_add(&inv_ctx->node, cn_inval_req_list.prev->prev);
            spin_unlock(&cnthread_inval_req_list_lock);
            return 1;
        }
    }
    return -1;
}

static __always_inline void serve_inval_rdma_req(unsigned char* rdma_buf)
{
    PROFILE_POINT_TIME(CN_update_roce_inv_req)
    struct cnthread_inv_msg_ctx *inv_ctx = kzalloc(sizeof(struct cnthread_inv_msg_ctx), GFP_KERNEL);
    struct cnthread_rdma_msg_ctx *rdma_ctx;
    if (!inv_ctx)
        BUG();

    rdma_ctx = &inv_ctx->rdma_ctx;
    // Mark that we picked this message
    rdma_buf[0] = 0x0;
    inval_rdma_retrieve_base(rdma_ctx, rdma_buf);
    inval_rdma_retrieve_ext(rdma_ctx, rdma_buf);   // to get sharer, qp_val

    // Main routine
    // Prepare inv_ctx and put invalidation request to the list
    inv_ctx->sk = recv_socket;
    inv_ctx->rdma_buf = rdma_buf;  // should be fine until we put 0xff into rdma_buf[0] again.
    PROFILE_START(CN_update_roce_inv_req);
    memcpy(inv_ctx->dummy_buf, rdma_buf, CACHELINE_ROCE_HEADER_LENGTH); // this buffer will be used for sending dummy data
    inv_ctx->addr_in.sin_family = AF_INET;
    inv_ctx->addr_in.sin_addr.s_addr = rdma_ctx->ip_val;   // same network endian
    inv_ctx->is_locked = 0;
    // inv_ctx->last_update = jiffies;      // for tracking invalidation requests
    inv_ctx->last_update = sched_clock();
    spin_lock(&cnthread_inval_req_list_lock);
    list_add(&inv_ctx->node, &cn_inval_req_list);
    spin_unlock(&cnthread_inval_req_list_lock);
    PROFILE_LEAVE(CN_update_roce_inv_req);
}

static __always_inline int get_next_buf_head_idx(void)
{
    return ((unsigned int)atomic_inc_return(&cnthread_buf_head_inv_splitter)) % DISAGG_QP_NUM_INVAL_BUF;
}

static __always_inline int one_step_inval_rdma_ack(int from_inv, int head_idx)
{
    PROFILE_POINT_TIME(CN_update_roce_inv_ack_inv)
    PROFILE_POINT_TIME(CN_update_roce_inv_ack_evt)
    PROFILE_POINT_TIME(CN_update_roce_inv_ack_prmpt)
    int msg_is_empty = 1, msg_type = 0;
    char *rdma_buf = NULL;
    rdma_buf = check_inval_rdma_ack(&msg_is_empty, head_idx);
    if (!rdma_buf)
    {
        if (msg_is_empty)
        {
            return -1;  // no more ack
        }
        else
        {   // there is other than preamble
            BUG();
        }
    }
    PROFILE_START(CN_update_roce_inv_ack_inv);
    PROFILE_START(CN_update_roce_inv_ack_evt);
    PROFILE_START(CN_update_roce_inv_ack_prmpt);
    msg_type = check_inval_rdma_type(rdma_buf);
    pr_cache("Message type: %d [ack head: %u]", msg_type, inval_buf_ack_head[head_idx]);
    if (msg_type != MSG_OVER_RDMA_INV_REQ)
    {
        // Serve ack here
        serve_inval_rdma_ack(rdma_buf, msg_type);
        if (from_inv == PROFILE_CNTHREAD_INV_ACK_SERV_FROM_INVAL)
            PROFILE_LEAVE(CN_update_roce_inv_ack_inv);
        else if (from_inv == PROFILE_CNTHREAD_INV_ACK_SERV_FROM_PRMPT)
            PROFILE_LEAVE(CN_update_roce_inv_ack_prmpt);
        else
            PROFILE_LEAVE(CN_update_roce_inv_ack_evt);
    }else{
        // Parse and put the requests into the list
        serve_inval_rdma_req(rdma_buf);
        atomic_inc(&cnthread_inv_req_counter);  // do not need to be accurate and synchronized (in timing)
    }
    __cnthread_debug_buffer_record[inval_buf_ack_head[head_idx]] = msg_type;  // record the last message type
    inc_inval_rdma_ack(head_idx);
    return 0;
}

void try_invalidation_lookahead(int from_inv, int lookahead)
{
    int rcv_i = 0;
    int head_idx = 0;
    spin_lock(&cnthread_inval_ack_lock);
    head_idx = get_next_buf_head_idx();
    spin_unlock(&cnthread_inval_ack_lock);
    if (spin_trylock(&cnthread_inval_head_buf_lock[head_idx]))
    {
        // Foresee ACKs
        for (rcv_i = 0; rcv_i < lookahead; rcv_i++)
        {
            if (one_step_inval_rdma_ack(from_inv, head_idx))
            {
                break;
            }
        }
        mb();
        spin_unlock(&cnthread_inval_head_buf_lock[head_idx]);
    }
}
EXPORT_SYMBOL(try_invalidation_lookahead);

// == EVICTION HANDLER ROUTINE == //
int cnthread_handler(void *data)
{
    struct cnthread_handler_data *h_data = (struct cnthread_handler_data *)data;
    int *after_init_stage = h_data->init_stage;
    int i, loop_i = 0;

    allow_signal(SIGKILL | SIGSTOP);
    atomic_set(&DEBUG_trigger_eviction, 0);
    // Wait until roce kernel module is initialized (and connected to the switch)
    while (!(*after_init_stage))
    {
        usleep_range(10, 10);
    }
    cnthread_initial_kmap();
    pin_current_thread(EVICT_HANDLER_CPU);
    pr_info("CN_thread_handler has been started\n");

    while (1)
    {
        if (kthread_should_stop())
        {
            goto release;
        }

        if (signal_pending(current))
        {
            __set_current_state(TASK_RUNNING);
            goto release;
        }

        // Try evition
        if (cnthread_evict_one(_threshold, _high_threshold) < 0) // If low cache pressure
        {
            // Help invalidation handler
            for (i = 0; i < DISAGG_QP_NUM_INVAL_BUF; i++)
                try_invalidation_lookahead(PROFILE_CNTHREAD_INV_ACK_SERV_FROM_EVICT, SERVE_ACK_PER_INV_FROM_EVICT);
            // Invalidation part
            if (check_inval_req_list_and_try(PROFILE_CNTHREAD_INV_ACK_SERV_FROM_EVICT, NULL) < 0)
            {
                // Wait if no eviction / pressure
                if (unlikely(loop_i >= DISAGG_NET_CTRL_POLLING_SKIP_COUNTER))
                {
                    loop_i = 0;
                    usleep_range(10, 10);
                }
                loop_i++;
            }
        }

        if (atomic_read(&DEBUG_trigger_eviction))   // If it was activated by test module
        {
            cnthread_find_and_evict(DEBUG_eviction_tgid, DEBUG_eviction_addr, PAGE_SIZE, 0, 0, 0, NULL, NULL, 0);
            atomic_set(&DEBUG_trigger_eviction, 0);
        }
    }

release: // Please release memory here
    if (data)
    {
        kfree(data);
        data = NULL; // meaningless
    }

    // We are shutting down the system...
    // TODO: we also need to unmap DMA address for dummy_page_buf and dummy_inv_page_buf
    for (i = 0; i < 3 * DISAGG_NUM_CPU_CORE_IN_COMPUTING_BLADE; i++)
    {
        if (dummy_page_buf[i])
        {
            kfree(dummy_page_buf[i]);
            dummy_page_buf[i] = NULL;
        }
    }
    if (dummy_inv_page_buf)
    {
        kfree(dummy_inv_page_buf);
        dummy_inv_page_buf = NULL;
    }
    return 0;
}

// == INVALIDATION HANDLER ROUTINE == //
static unsigned long inval_timer_start = 0;
static __always_inline int receive_next_request(void)
{
    int i = 0;
    get_cpu();
    for (i = 0; i < DISAGG_QP_NUM_INVAL_BUF; i++)
        try_invalidation_lookahead(PROFILE_CNTHREAD_INV_ACK_SERV_FROM_INVAL, SERVE_ACK_PER_INV);

    // Try invalidation
    if (check_inval_req_list_and_try(PROFILE_CNTHREAD_INV_ACK_SERV_FROM_INVAL, NULL) >= 0)    // If sucessfully served
    {
        put_cpu();
        return 0;
    }
    // No invalidation, return with -1
    put_cpu();
    return -1;
}

// Main body of the invalidation handler
int cache_ack_handler(void *data)
{
    struct cnthread_handler_data *h_data = (struct cnthread_handler_data *)data;
    int *after_init_stage = h_data->init_stage;
    int i = 0, loop_i = 0;
    unsigned long inval_timer_end;

    allow_signal(SIGKILL | SIGSTOP);

    // Initialize socket
    // - Wait until roce module is initialized (and connected to the switch)
    while (!recv_buf)
    {
        recv_buf = kmalloc(_recv_buf_size, GFP_KERNEL);
        msleep(1);
    }
    while (!ack_buf)
    {
        ack_buf = kmalloc(_recv_buf_size, GFP_KERNEL);
        msleep(1);
    }
    memset(ack_buf, 0, _recv_buf_size);
    pr_info("CN_ack_handler has been started\n");

    while (!(*after_init_stage))
    {
        usleep_range(10, 10);
    }

    if (!is_cache_ack_pinned)
        pin_current_thread(INVAL_HANDLER_CPU);
    is_cache_ack_pinned = 1;

    udp_initialize(&recv_socket, DEFAULT_UDP_PORT);
    udp_initialize(&send_socket, DEFAULT_UDP_SEND_PORT);
    send_ctx.sk = send_socket;

    // Set up invalidation rdma receiver
    for (i = 0; i < DISAGG_QP_NUM_INVAL_BUF; i++)
    {
        base_inval_buf[i] = get_inval_buf_ptr(i);
        pr_info("CN_thread_handler [inval buf[%d]: 0x%lx]\n",
                i, (unsigned long)base_inval_buf[i]);
    }

    // Send initial test message
    inval_timer_start = jiffies;    // intial timer
    while (1)
    {
        if (kthread_should_stop())
        {
            goto release;
        }

        if (signal_pending(current))
        {
            __set_current_state(TASK_RUNNING);
            goto release;
        }

        if(receive_next_request())
        {
            if (unlikely(loop_i >= DISAGG_NET_CTRL_POLLING_SKIP_COUNTER))
            {
                loop_i = 0;
                usleep_range(10, 10);
            }
            loop_i++;
        }

        inval_timer_end = jiffies;
        if (unlikely((inval_timer_end > inval_timer_start) 
                     && (jiffies_to_msecs(inval_timer_end - inval_timer_start) > (unsigned long)CNTHREAD_HEARTBEAT_IN_MS)))
        {
            inval_timer_start = inval_timer_end;
            printk(KERN_DEFAULT "CNTHREAD: HeartBeat from Invalidation handler: CPU[%d] (head: %u / ack_head: %u, %u) Free(c: %d, p: %d)\n",
                   (int)smp_processor_id(), inval_buf_head, inval_buf_ack_head[0], inval_buf_ack_head[1],
                   atomic_read(&cn_free_cacheline_counter), atomic_read(&cn_free_page_counter));
        }
    }

release: // Please release memory here
    // We are shutting down the system...
    if (recv_buf)
    {
        kfree(recv_buf);
        recv_buf = NULL;
    }
    if (ack_buf)
    {
        kfree(ack_buf);
        ack_buf = NULL;
    }
    return 0;
}
