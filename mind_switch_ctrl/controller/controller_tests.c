#include "controller.h"
#include "memory_management.h"
#include "request_handler.h"
#include "cacheline_def.h"
#include "config.h"
#include <stdio.h>

/*
 * Outdated - maybe not working
 */

/* Test for functions addressing internal data strucutres */
#define TEST_PRINT_FAILURE                                           \
    fprintf(stderr, "Failed: %s at line: %d\n", __func__, __LINE__); \
    return -1;

#define TEST_PRINT_SUCCESS                                     \
    fprintf(stderr, "Passed: %s at line: %d\n", __func__, __LINE__);

#define TEST_ASSERT_VALUES_EQ(V1, V2) \
    if (V1 != V2)                     \
    {                                 \
        TEST_PRINT_FAILURE            \
    }else{                            \
        TEST_PRINT_SUCCESS            \
    }

#define TEST_ASSERT_VALUES_NEQ(V1, V2) \
    if (V1 == V2)                      \
    {                                  \
        TEST_PRINT_FAILURE             \
    }else{                             \
        TEST_PRINT_SUCCESS             \
    }

#ifdef __CTRL_RUN_BASE_TEST__
static int test_insert_new_task()
{
    struct task_struct *old_tsk;
    u16 sender_id = 0;
    u16 tgid = 0x1b;
    u16 prev_tgid = 0x1a;
    struct task_struct *dummy_tsk, *dummy_tsk2;

    // 1) initial for from systemd
    // get task but should not have
    TEST_ASSERT_VALUES_EQ(mn_get_task(sender_id, tgid), NULL);

    // get prev_task but no
    old_tsk = mn_get_task(sender_id, prev_tgid);
    TEST_ASSERT_VALUES_EQ(old_tsk, NULL);

    // insert new task
    dummy_tsk = malloc(sizeof(struct task_struct));
    dummy_tsk->mm = NULL;
    TEST_ASSERT_VALUES_NEQ(dummy_tsk, NULL);
    TEST_ASSERT_VALUES_EQ(mn_insert_new_task_mm(sender_id, tgid, dummy_tsk), 0);

    // 2) copy from previous
    tgid = 0x1c;
    prev_tgid = 0x1b;

    // get task but should not have
    TEST_ASSERT_VALUES_EQ(mn_get_task(sender_id, tgid), NULL);

    // get prev_task
    old_tsk = mn_get_task(sender_id, prev_tgid);
    TEST_ASSERT_VALUES_NEQ(old_tsk, NULL);

    // insert new task
    dummy_tsk2 = malloc(sizeof(struct task_struct));
    dummy_tsk2->mm = NULL;
    TEST_ASSERT_VALUES_NEQ(dummy_tsk2, NULL);
    TEST_ASSERT_VALUES_EQ(mn_insert_new_task_mm(sender_id, tgid, dummy_tsk2), 0);

    // 3) override existing
    TEST_ASSERT_VALUES_NEQ(mn_get_task(sender_id, tgid), NULL);
    // just increase reference
    increase_utgid_ref(sender_id, tgid);

    // 4) delete all and check we cannot find them
    mn_delete_task(sender_id, tgid);
    // we duplicated this entry, so should be found
    TEST_ASSERT_VALUES_NEQ(mn_get_task(sender_id, tgid), NULL);
    // delete again, should be deleted
    mn_delete_task(sender_id, tgid);
    TEST_ASSERT_VALUES_EQ(mn_get_task(sender_id, tgid), NULL);

    // 5) clean up
    // Already deleted all the inserted tasks

    return 0;
}

static int test_add_memory_node(void)
{
    int node_id = 21;   // 16 + 1 + 4 (= memory node id for IP)
    int lid = 0;
    u8 mac[ETH_ALEN] = {0};
    unsigned int qpn[NUM_PARALLEL_CONNECTION] = {0};
    int psn = 0;
    char gid[sizeof(DISAGG_RDMA_GID_FORMAT)] = {0};
    u32 lkey = 0;
    u32 rkey = 0;
    u64 addr = 0x004000000; // Random offset
    u64 size = 0x001000000; // 16 MB
    struct dest_info *added_mn =
        ctrl_set_node_info(node_id, lid, mac, qpn, psn, gid, lkey, rkey, addr, size, NULL);
    TEST_ASSERT_VALUES_EQ(added_mn->base_addr, addr);
    TEST_ASSERT_VALUES_EQ(added_mn->lid, lid);
    TEST_ASSERT_VALUES_EQ(added_mn->node_id, node_id);

    // Make dummy node for further test
    debug_increase_mem_node_count();

    return 0;
}

void run_controller_base_test(void)
{
    printf("\nStart controller test ...\n");
    if (test_insert_new_task())
        goto fail;

    if (test_add_memory_node())
        goto fail;

    printf("Controller test finished\n\n");
    return;

fail:
    fprintf(stderr, "!!! Controller test failed !!!\n\n");
    return;
}
#else
void run_controller_base_test(void){;}
#endif

#ifdef __CTRL_RUN_BFRT_TEST__
static int test_bfrt_register_write(void)
{
    const unsigned int repeat = 100000;
    const unsigned int index_range = 20000;
    unsigned int i;
    uint16_t state = 0x6;
    uint16_t sharer = 0x4;
    uint16_t dir_size = INITIAL_REGION_INDEX;
    uint16_t dir_lock = CACHELINE_UNLOCKED;
    uint32_t inv_cnt = 0;
    uint32_t idx;
    printf("Start BFRT test\n");
    for (i = 0; i < repeat; i++)
    {
        idx = i % index_range;
        bfrt_add_cacheline_reg(idx, state, sharer, dir_size, dir_lock, inv_cnt);
        if (i < index_range)
            bfrt_add_cacheline(DYN_MIN_DIR_SIZE * idx, (64 - REGION_SIZE_BASE), idx);
    }
    printf("Done [%u]\n", i);
    TEST_ASSERT_VALUES_EQ(0, 0);
    return 0;
}
void run_controller_bfrt_test(void)
{
    test_bfrt_register_write();
}
#else
void run_controller_bfrt_test(void){;}
#endif

void run_controller_test(void)
{
    run_controller_base_test();
    run_controller_bfrt_test();
}
