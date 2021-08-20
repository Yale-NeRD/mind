// Test program to allocate new memory
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>
#include <pthread.h>
#include <stdarg.h>

/* config
 *
 */
#define VERIFY
#ifdef VERIFY
#define MAX_NUM_READS_TO_VERIFY 10000000
#define MAX_NUM_WRITES_TO_VERIFY 10000000
#endif

//#define TEST_ALLOC_FLAG MAP_PRIVATE|MAP_ANONYMOUS	// default: 0xef
//#define TEST_INIT_ALLOC_SIZE 1024L * 1024 // default: 16 GB

//#define LOG_NUM_ONCE 1000L
#define LOG_NUM_ONCE 1000 * 1000LU
#define LOG_NUM_TOTAL 1000000L/*000*/
#define TIMEWINDOW_US 10000L/*0000*/
#define LOG_MAP_ALIGN (15 * 4096)

#define MMAP_ADDR_MASK 0xffffffffffff
#define MAX_NUM_THREAD 16
#define MAX_NUM_NODES 16
//#define SLEEP_THRES_NANOS 10
//#define TEST_TO_APP_SLOWDOWN 1
//mmap log loading


struct log_header_5B {
  char op;
  unsigned int usec;
}__attribute__((__packed__));

struct RWlog {
  char op;
  union {
    struct {
      char pad[6];
      unsigned long usec;
    }__attribute__((__packed__));
    unsigned long addr;
  }__attribute__((__packed__));
}__attribute__((__packed__));

struct Mlog {
  struct log_header_5B hdr;
  union {
    unsigned long start;
    struct {
      char pad[6];
      unsigned len;
    }__attribute__((__packed__));
  }__attribute__((__packed__));
}__attribute__((__packed__));

struct Blog {
  char op;
  union {
    struct {
      char pad[6];
      unsigned long usec;
    }__attribute__((__packed__));
    unsigned long addr;
  }__attribute__((__packed__));
}__attribute__((__packed__));

struct Ulog {
  struct log_header_5B hdr;
  union {
    unsigned long start;
    struct {
      char pad[6];
      unsigned len;
    }__attribute__((__packed__));
  }__attribute__((__packed__));
}__attribute__((__packed__));

char* concat(int count, ...)
{
  va_list ap;
  int i;

  // Find required length to store merged string
  int len = 1; // room for NULL
  va_start(ap, count);
  for(i=0 ; i<count ; i++)
    len += strlen(va_arg(ap, char*));
  va_end(ap);

  // Allocate memory to concat strings
  char *merged = calloc(sizeof(char),len);
  int null_pos = 0;

  // Actually concatenate strings
  va_start(ap, count);
  for(i=0 ; i<count ; i++)
  {
    char *s = va_arg(ap, char*);
    strcpy(merged+null_pos, s);
    null_pos += strlen(s);
  }
  va_end(ap);

  return merged;
}


int pin_to_core(int core_id);

/**
int main(int argc, char *argv[]) {
  if (argc < 4) {
    printf("Usage: gen test_mem_size cache_line_size");
    return 1;
  }
  size_t test_mem_size = atol(argv[1]);
  size_t cache_line_size = atol(argv[2]);
  int num_threads = atoi(argv[3]);

  struct RWlog *logs = new struct RWlog[MAX_NUM_READS_TO_VERIFY];

  default_random_engine g;
  uniform_int_distribution<unsigned long> d_addr(0, test_mem_size / cache_line_size - 1);
  uniform_int_distribution<int> d_op(0, 1);
  for (int tid = 0; tid < num_threads; ++tid) {
    memset(logs, 0, sizeof(struct RWlog) * MAX_NUM_READS_TO_VERIFY);

    for (int i = 0; i < MAX_NUM_READS_TO_VERIFY; ++i) {
      struct RWlog *log = &logs[i];
      log->addr = (d_addr(g) * cache_line_size + tid) & MMAP_ADDR_MASK;
      log->op = d_op(g) ? 'R' : 'W';
      log->usec = i;
    }

    int fd = open(string(string("random_") + to_string(tid) + string("_0")).c_str(), O_WRONLY|O_CREAT|O_TRUNC, 777);
    long size = write(fd, logs, sizeof(struct RWlog) * MAX_NUM_READS_TO_VERIFY);
    if (size != sizeof(struct RWlog) * MAX_NUM_READS_TO_VERIFY)
      printf("fail to gen random log(%ld)\n", size);
    close(fd);

    //gen gold res
    uint8_t *addr_space = new uint8_t[test_mem_size];
    memset(addr_space, 0, test_mem_size);
    uint8_t *res = new uint8_t[MAX_NUM_READS_TO_VERIFY];
    size_t num_reads = 0, num_writes = 0;
    for (int i = 0; i < MAX_NUM_READS_TO_VERIFY; ++i) {
      struct RWlog *log = &logs[i];
      if (log->op == 'R') {
        res[num_reads++] = addr_space[log->addr & MMAP_ADDR_MASK];
      } else {
        addr_space[log->addr & MMAP_ADDR_MASK] = (uint8_t)((num_writes + 1) * (tid + 1));
        ++num_writes;
      }
    }
    fd = open(string(string("random_") + to_string(tid) + string("_0.res")).c_str(), O_WRONLY|O_CREAT|O_TRUNC, 777);
    size = write(fd, res, sizeof(uint8_t) * num_reads);
    if (size != sizeof(uint8_t) * num_reads)
      printf("fail to gen res(%ld)\n", size);
    close(fd);
  }


  return 0;
}
**/
