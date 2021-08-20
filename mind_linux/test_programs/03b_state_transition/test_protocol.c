// Test program to allocate new memory

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>
#include <pthread.h>
#include "../../include/disagg/config.h"
#include "errno.h"

#define MAX_THREAD_NUM 32            // 4 for LegoOS, but we can do it up to 16
#define LOCALITY 1  // all to one page

#ifdef DEBUG
    #define CACHE_LINE 1024 * 1024 * 2L
#else
    #define CACHE_LINE 1024 * 4L
#endif
#define CACHE_REGION_SIZE 1024 * 1024 * 2L  // 2MB

enum {
  READ_ACCESS = 0,
  WRITE_ACCESS = 1,
};

int main(int argc, char **argv) {
  int i, j;
  struct timespec start, end;
  double time_taken;
  char *data_buf = NULL, *meta_buf = NULL, *share_buf = NULL;
  unsigned long flags = TEST_ALLOC_FLAG;
  int local_thread_num = MAX_THREAD_NUM;
  int is_tester = 0;
  int locality = LOCALITY;
  int num_node = 1;
  int node_id = 0;
  int num_compute = 1, num_memory = 1, mem_chunk = 0, share_chunk = 0;
  int mem_chunk_per_node = 1, thread_per_chunk = MAX_THREAD_NUM, chunks_per_thread = 1;
  int share_ratio = 50;
  int read_ratio = 50;
  int total_share_region_size = 0;
  int access_mode = 0;
  int num_cache_region = 1024;
  unsigned char dummy_buf[CACHE_LINE] = {1};

  if (argc > 1 && atoi(argv[1])) {
    // if it is local
    flags = MAP_PRIVATE | MAP_ANONYMOUS;
  }

  if (argc > 2) {
    node_id = atoi(argv[2]);
  }
  printf("Disagg node id: %d\n", node_id);

  if (argc > 3) {
    access_mode = atoi(argv[3]);
    if (access_mode != READ_ACCESS && access_mode != WRITE_ACCESS)
    {
      printf("Unexpected mode: %d\n", access_mode);
      return -1;
    }
  }

  if (argc > 4) {
    num_cache_region = atoi(argv[4]); // number of cache region
  }

  meta_buf = (char *)mmap(NULL, TEST_MACRO_ALLOC_SIZE, PROT_READ | PROT_WRITE, flags, -1, 0);
  if (!meta_buf || meta_buf == (void *) -1) {
    int errnum = errno;
    printf("Error: cannot allocate buffer [0x%lx] %s\n", (unsigned long) meta_buf, strerror(errnum));
    return -1;
  }
  num_cache_region = (num_cache_region > (TEST_INIT_ALLOC_SIZE / CACHE_REGION_SIZE)) ? (TEST_INIT_ALLOC_SIZE / CACHE_REGION_SIZE) : num_cache_region;

  printf("\n==Start test [%d regions]==\n", num_cache_region);
  for (i = 0; i < num_cache_region; i++)
  {
    if (access_mode == READ_ACCESS)
    {
      memcpy(dummy_buf, &meta_buf[i * CACHE_REGION_SIZE], CACHE_LINE);
    }else{
      memcpy(&meta_buf[i * CACHE_REGION_SIZE], dummy_buf, CACHE_LINE); 
    }
  }
  printf("Set up ended!\n");
  printf("Wait forever... please kill me after checking latency (CTRL+C)\n");
  while(1)
  {
    sleep(1);
  }
  munmap(meta_buf, TEST_MACRO_ALLOC_SIZE);
  return 0;
}

