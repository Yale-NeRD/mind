#include "../../../../include/trace_def.hpp"

#define MAX_NUM_THREADS 192
#define BUF_SIZE_PER_THREAD (8388608)
#define MMAP_ADDR_MASK 0xffffffffffff
#define MMAP_MAX_LEN 0xffffffff
#define PAD 4096
#define NUM_MM_SYSCALLS 4
