#ifndef __DISAGG_PROGRAM_CONFIG_H__
#define __DISAGG_PROGRAM_CONFIG_H__

// Configuration file for test
#define TEST_PROGRAM_NAME "test_protocol"
#define TEST_PROGRAM_DIGIT 13
#define REMOTE_THREAD_SLEEP_INIT_IN_SECOND 1
#define TEST_INIT_ALLOC_SIZE (4 * 1024 * 1024 * 1024UL)  // 4 GB
#define TEST_MACRO_ALLOC_SIZE (8 * 1024 * 1024 * 1024UL)  // 8 GB
#define TEST_SUB_REGION_ALLOC_SIZE (1 * 1024 * 1024 * 1024UL)  // 1 GB
#define TEST_META_ALLOC_SIZE (32 * 1024 * 1024UL)  // 32 MB
#define TEST_ALLOC_FLAG 0xfe
#define TEST_ALLOC_FILE_FLAG 0xfd
#define TEST_PROGRAM_TGID 0
#define RESET_DIR_WHEN_FAIL 0
#define TEST_DEBUG_SIZE_LIMIT (256 * 1024 * 1024)

#ifndef __TEST__
// Test VMA inditifier: body is inside mmap_disagg.c
int TEST_is_target_vma(unsigned long vm_start, unsigned long vm_end);
int TEST_is_sub_target_vma(unsigned long vm_start, unsigned long vm_end);
int TEST_is_meta_vma(unsigned long vm_start, unsigned long vm_end);
int TEST_is_test_vma(unsigned long vm_start, unsigned long vm_end);
// Thread counter for test program
void init_test_program_thread_cnt(void);
atomic_t *get_test_program_thread_cnt(void);
#endif
#endif
