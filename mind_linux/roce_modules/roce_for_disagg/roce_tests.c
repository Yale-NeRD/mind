/*
 * Test routines for RoCE module
 * - Not complete, minimal useable functions (on-going work)
 */

// #include "config.h"

#include "roce_disagg.h"
#include "../../include/disagg/cnthread_disagg.h"

#include <linux/mman.h>
#include <asm/traps.h> // X86_PF_WRITE

#define TEST_ROLE_INDEX 1
#define CHECK_REGISTER
// #define TEST_FOR_NEW_DIR
// #define TEST_BYPASS_CACHE_MATCH

// static unsigned long MN_VA_PARTITION_BIT_MASK = 0x10000000000;
extern int disagg_fork_for_test(struct task_struct *tsk);
extern unsigned long
do_disagg_mmap(struct task_struct *tsk,
                unsigned long addr, unsigned long len, unsigned long prot,
                unsigned long flags, vm_flags_t vm_flags, unsigned long pgoff,
                struct file *file);
extern int disagg_exit_for_test(struct task_struct *tsk);

static int TEST_read_msg = 0;
static int TEST_write_msg = 1;
static int TEST_reset_msg = 2;
static int try_pgfault(u32 tgid, u64 address, void *retbuf, int access, struct fault_reply_struct *reply)
{
    struct fault_msg_struct payload;
    int res = -1;
    u32 tot_size = sizeof(struct fault_reply_struct);

    reply->data = (void *)cn_reg_mr_ftn(retbuf, PAGE_SIZE);
    payload.tgid = tgid;
    payload.address = address;
    payload.error_code = 0;
    if (access == TEST_write_msg)
        payload.error_code = X86_PF_WRITE; //writable
    payload.flags = (access == TEST_reset_msg); // reset msg

    // check the size of the received data: it should have at least default struct size
    res = send_msg_via_rdma(DISSAGG_PFAULT, &payload, sizeof(payload),
                            (void*)reply, tot_size);  //tot_size is not used inside
    return res;
}

static int try_pgfault_cache(u32 tgid, u64 address, void *retbuf, int is_read)
{
    int res = -1;
    struct cache_waiting_node *wait_node;
    struct cnthread_req *cnreq;
    struct fault_reply_struct reply;
    int exist_page;
    spinlock_t *core_lock;
    int cpu_id;
    // if (is_read)
    //     cnreq= cnthread_get_new_page(tgid, address, NULL);
    // else
    cnreq = cnthread_get_page(tgid, address, NULL, &exist_page); // can be old page
    if (!cnreq)
        goto go_out_without_cpu;
    cpu_id = get_cpu();
    core_lock = get_per_core_lock(cpu_id);
    spin_lock(core_lock);
#ifndef TEST_BYPASS_CACHE_MATCH
    wait_node = add_waiting_node(tgid, address & PAGE_MASK, cnreq);
#endif
    // printk(KERN_DEFAULT "Read from 0x%llx [w:%d]...\n", address, !is_read);
    if (!wait_node)
        goto go_out;
    res = try_pgfault(tgid, address, retbuf, is_read, &reply);
    wait_node->ack_buf = reply.ack_buf;
#ifndef TEST_BYPASS_CACHE_MATCH
    wait_until_counter(wait_node, NULL, NULL, cnreq);
#endif
    // printk(KERN_DEFAULT "Data received for 0x%llx [w:%d]\n", address, !is_read);
    // printk(KERN_DEFAULT "Ack received\n");
    cnthread_add_pte_to_list_with_cnreq(NULL, address, NULL, cnreq, !exist_page);
    // if (cnreq)
    //     up_read(&cnreq->cacheline->access_sem);
go_out:
    spin_unlock(core_lock);
    put_cpu();
    spin_unlock(&cnreq->pgfault_lock);
go_out_without_cpu:
    return res;
}

static int try_evict(u32 tgid, u64 address, void *buf)
{
    (void)buf;
    DEBUG_trigger_evict(tgid, address);
    msleep(3000);
    return 0;
}

// test function prototype exposed from kerenl to test register
#ifdef TEST_BYPASS_CACHE_MATCH
static const unsigned long tgid_for_test = 0x1000;
#else
static const unsigned long tgid_for_test = 0x4321;  //other than 0x1000
#endif
int send_cache_dir_check(u16 tgid, u64 vaddr, u16 *state, u16 *sharer, int sync_direction);
int send_cache_dir_full_check(u16 tgid, u64 vaddr, u16 *state, u16 *sharer,
							  u16 *dir_size, u16 *dir_lock, u16 *inv_cnt, int sync_direction);

static void try_cache_dir_req(u16 tgid, u64 vaddr, void *buf, int access_mod)
{
    u16 state, sharer, dir_size, dir_lock, inv_cnt;
#ifdef CHECK_REGISTER
    // send_cache_dir_check(tgid, vaddr, &state, &sharer, CN_SWITCH_REG_SYNC_PULL);    // pull state before sending something
    send_cache_dir_full_check(tgid, vaddr, &state, &sharer, &dir_size, &dir_lock, &inv_cnt, CN_SWITCH_REG_SYNC_PULL);
    printk(KERN_DEFAULT "[Before] Current state for 0x%llx: state [0x%x] sharer [0x%x] size[0x%x], lock/cnt[%u/%u]\n",
           vaddr, state, sharer, dir_size, (unsigned int)dir_lock, (unsigned int)inv_cnt);

#endif
    try_pgfault_cache(tgid, vaddr, buf, access_mod);
#ifdef CHECK_REGISTER
    // send_cache_dir_check(tgid, vaddr, &state, &sharer, CN_SWITCH_REG_SYNC_PUSH);    // ack received, so push
    send_cache_dir_full_check(tgid, vaddr, &state, &sharer, &dir_size, &dir_lock, &inv_cnt, CN_SWITCH_REG_SYNC_PUSH);
    printk(KERN_DEFAULT "[After] Current state for 0x%llx: state [0x%x] sharer [0x%x] size[0x%x], lock/cnt[%u/%u]\n",
           vaddr, state, sharer, dir_size, (unsigned int)dir_lock, (unsigned int)inv_cnt);
#endif
}

static void try_cache_dir_evict(u16 tgid, u64 vaddr, void *buf)
{
    u16 state, sharer, dir_size, dir_lock, inv_cnt;
#ifdef CHECK_REGISTER
    // send_cache_dir_check(tgid, vaddr, &state, &sharer, CN_SWITCH_REG_SYNC_PULL);
    send_cache_dir_full_check(tgid, vaddr, &state, &sharer, &dir_size, &dir_lock, &inv_cnt, CN_SWITCH_REG_SYNC_PULL);
    printk(KERN_DEFAULT "[Before] Current state for 0x%llx: state [0x%x] sharer [0x%x] size[0x%x], lock/cnt[%u/%u]\n",
           vaddr, state, sharer, dir_size, (unsigned int)dir_lock, (unsigned int)inv_cnt);
#endif
    try_evict(tgid, vaddr, buf);
#ifdef CHECK_REGISTER
    // send_cache_dir_check(tgid, vaddr, &state, &sharer, CN_SWITCH_REG_SYNC_PUSH);
    send_cache_dir_full_check(tgid, vaddr, &state, &sharer, &dir_size, &dir_lock, &inv_cnt, CN_SWITCH_REG_SYNC_PUSH);
    printk(KERN_DEFAULT "[After] Current state for 0x%llx: state [0x%x] sharer [0x%x] size[0x%x], lock/cnt[%u/%u]\n",
           vaddr, state, sharer, dir_size, (unsigned int)dir_lock, (unsigned int)inv_cnt);
#endif
}

#define repeat (10)
const unsigned long test_len = repeat * PAGE_SIZE * CNTHREAD_CACHELINE_SIZE_IN_PAGES;

static void test_cache_directory_as_primary_node(void)
{
    unsigned long addr = 0;
    int ret = -1;
    void *buf = kmalloc(1024 * 1024, GFP_KERNEL);
    // int read = 0, write = 1;
    // int i = 0, j = 0;
    struct task_struct *tsk;
    tsk = kmalloc(sizeof(struct task_struct), GFP_KERNEL);
    tsk->tgid = tgid_for_test;
    tsk->pid = tgid_for_test;
    memcpy(tsk->comm, " \0", 2);

    if (!buf)
        return;

    msleep(3000);
    ret = disagg_fork_for_test(tsk);
    if (ret < 0)
    {
        printk(KERN_WARNING "Cannot fork address");
        return;
    }
    msleep(500);

    addr = do_disagg_mmap(tsk, 0, test_len,
                          0, MAP_PRIVATE | MAP_ANONYMOUS, VM_READ, 0, NULL);
    if (addr <= 0)
    {
        printk(KERN_WARNING "Cannot mmap address");
        return;
    }
    printk(KERN_DEFAULT "mapped address: 0x%lx\n", addr);
    msleep(500);

    try_cache_dir_req(tgid_for_test, addr, buf, TEST_write_msg);  // I -> S
#ifdef TEST_FOR_NEW_DIR
    for (i = 0; i < repeat; i++) // do not access to the same one again (is just likes to trigger page fault to the same page twice)
    {
        try_cache_dir_req(tgid_for_test, addr + (i * CNTHREAD_CACHELINE_SIZE_IN_PAGES * PAGE_SIZE), buf, read); // I -> S
    }
#else
    // for (i = 1; i < repeat; i++)
    // {
    //     // for (j = 0; j < CNTHREAD_CACHELINE_SIZE_IN_PAGES; j++)
    //     {
    //         try_cache_dir_req(tgid_for_test, addr + (i * PAGE_SIZE), buf, read); // S -> S
    //     }
    //     msleep(1);
    // }
#endif
    // while (1)
    // {
    //     // Test1
    //     try_cache_dir_req(tgid_for_test, addr + PAGE_SIZE, buf, write);     // S -> M
    //     usleep_range(100, 100);
    // }
    // Test 1)
    // try_cache_dir_req(tgid_for_test, addr, buf, TEST_write_msg);     // S -> M
    // try_cache_dir_req(tgid_for_test, addr, buf, TEST_read_msg);      // M -> M
    // try_cache_dir_req(tgid_for_test, addr, buf, TEST_read_msg);      // M -> M
    // try_cache_dir_req(tgid_for_test, addr, buf, TEST_reset_msg);     // M -> M
    // try_cache_dir_evict(tgid_for_test, addr, buf);          // M -> I
    //
    // Test 2)
    // try_cache_dir_req(tgid_for_test, addr, buf, TEST_read_msg); // I -> S
    // try_cache_dir_req(tgid_for_test, addr, buf, TEST_write_msg); // S -> M
    //
    // try_cache_dir_req(tgid_for_test, addr, buf, write); // M -> M^D
    // try_cache_dir_req(tgid_for_test, addr, buf, write); // M^D -> M^D (generating NACK)
    //
    // try_cache_dir_evict(tgid_for_test, addr, buf);      // M^D -> M -> I (no one in sharer list)
    // try_cache_dir_req(tgid_for_test, addr, buf, write); // I -> M
    // try_cache_dir_req(tgid_for_test, addr, buf, TEST_read_msg);  // I -> S
    //
    // Test1
    // try_cache_dir_evict(tgid_for_test, addr, buf);      // M -> I
    // try_cache_dir_req(tgid_for_test, addr, buf, read);  // I -> S
    // try_cache_dir_evict(tgid_for_test, addr, buf);      // S -> I, cnthread has been cleared?
    //
    // try_cache_dir_evict(tgid_for_test, addr, buf);      // S -> I
    // disagg_exit_for_test(tsk);

    kfree(buf);
}

void test_cache_directory_as_secondary_node(void)
{
    unsigned long addr = 0x200000;
    // unsigned long addr = 0x1000;
    // unsigned long addr = MN_VA_MIN_ADDR;    // the first cache region
    // int ret = -1;
    void *buf = kmalloc(1024 * 1024, GFP_KERNEL);
    // int read = 0, write = 1;
    // int i = 0;
    struct task_struct *tsk;
    tsk = kmalloc(sizeof(struct task_struct), GFP_KERNEL);
    tsk->tgid = tgid_for_test;
    tsk->pid = tgid_for_test;
    memcpy(tsk->comm, " \0", 2);

    if (!buf)
        return;

    printk(KERN_DEFAULT "Target address: 0x%lx\n", addr);   // assumed address
    msleep(500);

    // while (1)
    // {
    //     try_cache_dir_req(tgid_for_test, addr, buf, read); // M -> S (dummy data)
    //     try_cache_dir_req(tgid_for_test, addr, buf, write); // M -> S (dummy data)
    //     usleep_range(100, 100);
    // }
    // try_cache_dir_req(tgid_for_test, addr, buf, TEST_write_msg); // S -> M: no actual data, so no pte error at node 1
    // try_cache_dir_evict(tgid_for_test, addr, buf);      // M -> I
    try_cache_dir_req(tgid_for_test, addr + 0x1000, buf, TEST_write_msg); // S -> M: no actual data, dummy data from node 1
    //
    try_cache_dir_req(tgid_for_test,
                      addr + test_len - (2 * CACHELINE_MAX_SIZE),
                      buf, TEST_read_msg); // I -> S for last directory (to the merged entry)

    // try_cache_dir_req(tgid_for_test, addr, buf, read);  // I -> S
    // try_cache_dir_req(tgid_for_test, addr, buf, write); // S -> M
    // try_cache_dir_req(tgid_for_test, addr, buf, write); // M -> M^D
    // try_cache_dir_req(tgid_for_test, addr, buf, write); // M^D -> M^D (generating NACK)
    // try_cache_dir_evict(tgid_for_test, addr, buf);      // M^D -> M -> I (no one in sharer list)
    // try_cache_dir_req(tgid_for_test, addr, buf, write); // I -> M
    // try_cache_dir_req(tgid_for_test, addr, buf, read);  // M -> S^D
    // try_cache_dir_evict(tgid_for_test, addr, buf);      // S^D -> S
    // try_cache_dir_evict(tgid_for_test, addr, buf);      // S -> I

    disagg_exit_for_test(tsk);
    kfree(buf);
}

static void test_cache_directory(struct rdma_context *ctx, struct dest_info *dest)
{
#ifdef __CN_ROCE_TEST__
    if (TEST_ROLE_INDEX == 1)
        test_cache_directory_as_primary_node();
    else if (TEST_ROLE_INDEX == 2)
        test_cache_directory_as_secondary_node();
#endif
}

void test_rdma(struct rdma_context *ctx, struct dest_info *dest)
{
    // test_rdma_multiplexing(ctx, dest);
    test_cache_directory(ctx, dest);
}
