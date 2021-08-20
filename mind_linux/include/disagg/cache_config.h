#ifndef __CACHE_CONFIG_HEADER__
#define __CACHE_CONFIG_HEADER__

// cache region / directory size related
// 0 for 4 KB, 9 for 2 MB
enum{
    REGION_SIZE_4KB = 0,
    REGION_SIZE_16KB = 2,
    REGION_SIZE_1MB = 8,
    REGION_SIZE_2MB = 9,
    REGION_SIZE_TOTAL = 10,
};
#define REGION_SIZE_BASE 12  // for 4KB
#define DYN_MIN_DIR_SIZE 4096

#define MIND_USE_TSO

#define SERVE_ACK_PER_INV 16
#define SERVE_ACK_PER_INV_FROM_EVICT 16
#define SERVE_ACK_PER_RANGE_INV 1
#define SERVE_ACK_PER_NACK 8
#define SERVE_ACK_PER_WAIT 4
#define PREEMPTIVE_ACK_CHK_PER_INV 1024     // 1024 for disable
#define PREEMPTIVE_INV_HIGH_PRESSURE 4      // 0 for always

uint64_t size_index_to_size(uint16_t s_idx);

#endif

