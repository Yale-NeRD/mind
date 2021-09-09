// Test program to allocate new memory
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>
#include <pthread.h>
#include <sched.h>
#include <assert.h>
#include "../../include/disagg/config.h"
#include "test_utils.h"
#include "time_utils.h"
#include "log_util.h"
#include "errno.h"

// #define VERBOSE

static pthread_barrier_t s_barrier, e_barrier, rs_barrier;
// Shared ratio(0, 25%, 50%, 75%, 100%)
// Read/write ratio (0, 25%, 50%, 75%, 100%)

#define SLEEP_THRES_NANOS 10
#define DATASET_SIZE (1000 * 1000LU)    // 4 KB * 1 million
#define MAX_THREAD_NUM 32            // 4 for LegoOS, but we can do it up to 16
#define PAGE_SIZE 4096LU
#define LOCALITY 1  // all to one page
#define MAX_MEM_NODE 4
#define DEBUG_ALLOC_SIZE 1024 * 1024 * 1024 * 2L
//#define DEBUG
#define CDF_BUCKET_NUM 512

#define GENERATE_LOG

#define CACHE_LINE 1024 * 4L
#define CACHE_MAX_DIR (4 * 1024LU)
#define CACHE_MAX_DIR_MASK (~(CACHE_MAX_DIR - 1))

/**
 * @brief Latency into CDF buckets
 * @param lat_in_us latency in microseconds
 * @return Bucket number
 */
static int latency_to_bkt(unsigned long lat_in_us) {
  if (lat_in_us < 100)
    return (int) lat_in_us;
  else if (lat_in_us < 1000)
    return 100 + ((lat_in_us - 100) / 10);
  else if (lat_in_us < 10000)
    return 190 + ((lat_in_us - 1000) / 100);
  else if (lat_in_us < 100000)
    return 280 + ((lat_in_us - 10000) / 1000);
  else if (lat_in_us < 1000000)
    return 370 + ((lat_in_us - 100000) / 10000);
  return CDF_BUCKET_NUM - 1;    // over 1 sec
}


/**
 * @brief Trace object
 */
struct trace_t {
  char *logs;
  unsigned long num_cache_line;
  char *meta_buf;
  char *data_buf;
  char *shared_buf;
  int node_idx;
  int num_nodes;
  int is_main;
  int test_mode;
  bool master_thread;
  // FILE *fd;
  int thread_id;
  int total_threads;
  unsigned long access_size;
  int num_compute_nodes;
  int node_id;
  int read_ratio;
  int shared_ratio;
  int spatial_locality;
  unsigned long num_shared_pages;
  unsigned long data_buf_size;
  unsigned long len;
  unsigned long local_num_pages;
  unsigned long cdf_cnt_p_r[CDF_BUCKET_NUM];
  unsigned long cdf_cnt_p_w[CDF_BUCKET_NUM];
  unsigned long cdf_cnt_s_r[CDF_BUCKET_NUM];
  unsigned long cdf_cnt_s_w[CDF_BUCKET_NUM];
};

struct load_arg_t {
  // int fd;
  struct trace_t *arg;
  unsigned long ts_limit;
  bool all_done;
};

struct load_arg_t load_args[MAX_NUM_THREAD];
struct trace_t args[MAX_THREAD_NUM];
pthread_t threads[MAX_THREAD_NUM];
static int phase = 0;
int num_nodes;

#define MAX_NUM_THREAD 16
#define MAX_NUM_NODES 16

struct metadata_t {
  unsigned int node_mask[MAX_NUM_NODES];
  unsigned int fini_node_pass[MAX_NUM_NODES];
};

inline void interval_between_access(long delta_time_usec) {
  if (delta_time_usec <= 0)
    return;
  else {
    struct timespec ts;
    ts.tv_nsec = (delta_time_usec << 1) / 3;
    if (ts.tv_nsec > SLEEP_THRES_NANOS) {
      ts.tv_sec = 0;
      nanosleep(&ts, NULL);
    }
  }
}

static inline void record_time(struct trace_t *trace, unsigned long dt_op, int is_read, int is_shared) {
  if (trace) {
    if (is_read) {
      if (is_shared)
        trace->cdf_cnt_s_r[latency_to_bkt(dt_op)]++;
      else
        trace->cdf_cnt_p_r[latency_to_bkt(dt_op)]++;

    } else {
      if (is_shared)
        trace->cdf_cnt_s_w[latency_to_bkt(dt_op)]++;
      else
        trace->cdf_cnt_p_w[latency_to_bkt(dt_op)]++;
    }
  }
}

static bool check_ready_nodes(struct metadata_t *meta_ptr, int should_zero) {
  int done_cnt = 0;
  for (int j = 0; j < MAX_NUM_NODES; ++j) {
    if ((!should_zero && (meta_ptr->node_mask[j] == 0))
        || (should_zero && (meta_ptr->node_mask[j] != 0))) {
    } else {
      done_cnt++;
    }
  }
  return (done_cnt >= num_nodes ? 1 : 0);
}

int init(struct trace_t *trace) {
  if (trace && trace->meta_buf) {
    struct metadata_t *meta_ptr = (struct metadata_t *) trace->meta_buf;
    // write itself
    if (trace->master_thread) {
      if (!phase)    // 0 -> 1
        meta_ptr->node_mask[trace->node_idx] = 1;
      else        // 1 -> 0
        meta_ptr->node_mask[trace->node_idx] = 0;
      // check nodes
      int i = 0;
      while (!check_ready_nodes(meta_ptr, phase)) {
        if (i % 100 == 0) {
          char node_bits[256] = "";
          for (int j = 0; j < MAX_NUM_NODES; ++j)
            sprintf(node_bits, "%s%01d", node_bits, meta_ptr->node_mask[j]);
#ifdef VERBOSE
          printf("Waiting nodes [%03d]: %d [p:%d] || [0x%x] of [%s]\n",
                 (i / 100) % 100, trace->num_nodes, phase, meta_ptr->node_mask[trace->node_idx], node_bits);
#endif
        }
        if (i % 200 == 0) {
#ifdef meta_data_test
          meta_ptr->node_mask |= (1 << (trace->node_idx)); // TEST PURPOSE ONLY
#endif
          if (!phase)                                      // 0 -> 1
            meta_ptr->node_mask[trace->node_idx] = 1; //|= node_mask;
          else                                          // 1 -> 0
            meta_ptr->node_mask[trace->node_idx] = 0; //&= (~node_mask);
        }
        usleep(10000);
        i++;
      }
      printf("All nodes are initialized: %d [0x%x]\n", trace->num_nodes, meta_ptr->node_mask[trace->node_idx]);
      fflush(stdout);
      phase = !phase;
      pthread_barrier_wait(&s_barrier);
      return 0;
    } else {
      pthread_barrier_wait(&s_barrier);
      return 0;
    }
  }
  return -1;
}

void mem_access_load_func(void *data) {
  struct trace_t *arg = (struct trace_t *) data;
  struct timeval ts, ts_op;
  unsigned long dt_op = 0;
  pin_to_core(arg->thread_id);

#ifndef DEBUG
  if (init(arg)) {
    fprintf(stderr, "Initialization error!\n");
    return;
  }
#endif
  unsigned long start_addr = (unsigned long) arg->data_buf;
  unsigned long
      thread_start_addr = start_addr + (arg->node_id * arg->total_threads + arg->thread_id) * arg->access_size;
  unsigned long thread_end_addr = thread_start_addr + arg->access_size;
  unsigned long addr;
  unsigned int seedp = arg->total_threads * arg->node_id + arg->thread_id;
  int shared_ratio = arg->shared_ratio;
  int read_ratio = arg->read_ratio;
  unsigned long i = 0;
  int j = 0;
  char zeros[PAGE_SIZE];
  char *shared_buf, *data_buf;
  struct RWlog *logs;
  unsigned long num_shared_pages = arg->num_shared_pages;
  unsigned long data_buf_size = arg->data_buf_size;
  unsigned long data_region_size = arg->total_threads * arg->access_size;// size of single region
  unsigned long local_num_pages = arg->local_num_pages;
  shared_buf = arg->shared_buf;
  data_buf = arg->data_buf;

  memset(zeros, 0, PAGE_SIZE);
#ifdef VERBOSE
  printf("Thread[%d]: Data start from: 0x%lx thread addr: 0x%lx - 0x%lx Shared buffer addr: 0x%lx - 0x%lx\n",
         arg->thread_id,
         (unsigned long) start_addr,
         (unsigned long) thread_start_addr,
         (unsigned long) thread_end_addr,
         (unsigned long) arg->data_buf + data_buf_size,
         (unsigned long) (arg->data_buf + data_buf_size + arg->num_shared_pages * PAGE_SIZE));
#endif

#ifdef DEBUG
  pthread_barrier_wait(&s_barrier);
#endif
  pthread_barrier_wait(&rs_barrier);

  // printf("Pass all barriers here! Thread[%d] LOG_ALL: %d\n", arg->thread_id, (int) LOG_ALL);

  for (i = 0; i < DATASET_SIZE; i++) {
    // Writing to the shared region
    if (TrueOrFalse(shared_ratio, &seedp)) {
      addr = GetRandom(0, num_shared_pages, &seedp) * PAGE_SIZE;
      addr &= CACHE_MAX_DIR_MASK;
      if (TrueOrFalse(read_ratio, &seedp)) {
        {
          memcpy(zeros, &data_buf[addr], PAGE_SIZE);
        }
      } else {
        {
          memcpy(&data_buf[addr], zeros, PAGE_SIZE);
        }
      }
    } else
      // to the private region
    {
      addr = GetRandom(0, local_num_pages, &seedp) * PAGE_SIZE; // Page aligned
      addr &= CACHE_MAX_DIR_MASK;
      if (TrueOrFalse(read_ratio, &seedp)) {
        {
          memcpy(zeros,
                 &data_buf[addr + data_region_size
                     + ((arg->node_id * arg->total_threads + arg->thread_id) * arg->access_size)],
                 PAGE_SIZE);
          }
      } else {
        {
          memcpy(&data_buf[addr + data_region_size
              + ((arg->node_id * arg->total_threads + arg->thread_id) * arg->access_size)], zeros, PAGE_SIZE);
        }
      }
    }
  }

  pthread_barrier_wait(&e_barrier);
#ifdef VERBOSE
  printf("Thread[%d]: ended [j:%d]\n", arg->thread_id, j);
#endif
}

void enforce_ratio_boundary(int *ratio) {
  if (*ratio < 0)
    *ratio = 0;
  else if (*ratio > 100)
    *ratio = 100;
}

enum {
  arg_mmap_local_flag = 1,
  arg_local_thread_num = 2,
  arg_num_shared_pages = 3,
  arg_num_compute = 4,
  arg_node_id = 5,
  arg_num_memory = 6,
  arg_share_ratio = 7,
  arg_read_ratio = 8,
  arg_spatial_locality = 9,
  arg_local_num_pages = 10,
  arg_generate_logs = 11,
  arg_log1 = 12,
};

int main(int argc, char **argv) {
  int i, j;
  struct timespec start, end;
  double time_taken;
  char *data_buf = NULL, *meta_buf = NULL, *share_buf = NULL;


  // Default values
  unsigned long flags = TEST_ALLOC_FLAG;
  int local_thread_num = MAX_THREAD_NUM;
  int local_num_pages = 204800; // Value from GAM benchmark
  int num_shared_pages = 1;
  int node_id = 0;
  int num_compute = 1, num_memory = 1;
  int share_ratio = 50;
  int read_ratio = 50;
  int spatial_locality = 50;
  int LOG_ALL = false;
  // FILE **fptr = NULL;
  if (argc < 8) {
    printf(
        "Usage: ./bin/debug_test mmap_flag total_threads, number of shared pages, num_compute, node_id, num_memory, share_ratio, read_ratio, spatial locality, local_num_pages, generate_log_flag, logfile_prefix / logfile_names\n");
    return 1;
  }
  // Values from input args
  if (argc > arg_mmap_local_flag && atoi(argv[arg_mmap_local_flag])) {
    // if it is local
    flags = MAP_PRIVATE | MAP_ANONYMOUS;
  }

  if (argc > arg_local_thread_num) {
    // if it is local
    local_thread_num = atoi(argv[arg_local_thread_num]);
  }
#ifdef VERBOSE
  printf("\nTotal threads: %d\n", local_thread_num);
#endif
  if (argc > arg_num_shared_pages) {
    num_shared_pages = atoi(argv[arg_num_shared_pages]);
  }
#ifdef VERBOSE
  printf("Number of shared pages in pages: %d\n", num_shared_pages);
#endif
  if (argc > arg_num_compute) {
    num_compute = atoi(argv[arg_num_compute]);
    if (num_compute <= 0) {
      num_compute = 1;
    }
  }

  if (argc > arg_node_id) {
    node_id = atoi(argv[arg_node_id]);
  }
  printf("Disagg node id: %d\n", node_id);
  printf("Total disaggregated nodes: %d\n", num_compute);

  if (argc > arg_num_memory) {
    num_memory = atoi(argv[arg_num_memory]);
  }
#ifdef VERBOSE
  printf("Num memory node: %d\n", num_memory);
#endif

  if (argc > arg_share_ratio) {
    share_ratio = atoi(argv[arg_share_ratio]);
  }
  enforce_ratio_boundary(&share_ratio);
#ifdef VERBOSE
  printf("Share ratio is: %d\n", share_ratio);
#endif
  if (argc > arg_read_ratio) {
    read_ratio = atoi(argv[arg_read_ratio]);
  }
  enforce_ratio_boundary(&read_ratio);
#ifdef VERBOSE
  printf("Read ratio is: %d\n", read_ratio);
#endif

  // no impact, only for compatibility
  if (argc > arg_spatial_locality) {
    spatial_locality = atoi(argv[arg_spatial_locality]);
  }
  enforce_ratio_boundary(&spatial_locality);

  if (argc > arg_local_num_pages) {
    local_num_pages = atoi(argv[arg_local_num_pages]);
  }
#ifdef VERBOSE
  printf("Local number of pages is: %d\n", local_num_pages);
#endif
  if (num_memory <= 0) {
    num_memory = 1;
  }
  num_nodes = num_compute;

  unsigned long total_share_size = num_shared_pages * PAGE_SIZE; // FIXME maybe adjust this memory

  unsigned long thread_local_size = local_num_pages * PAGE_SIZE;


// Allocate test memory mappings
#ifdef DEBUG
  meta_buf = (char *)mmap(NULL, DEBUG_ALLOC_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON | MAP_NORESERVE, -1, 0); //TEST PURPOSE
#else
  meta_buf = (char *) mmap(NULL, TEST_MACRO_ALLOC_SIZE, PROT_READ | PROT_WRITE, TEST_ALLOC_FLAG, -1, 0);
#endif
  if (!meta_buf || meta_buf == (void *) -1) {
    int errnum = errno;
    printf("Error: cannot allocate buffer [0x%lx] %s\n", (unsigned long) meta_buf, strerror(errnum));
    return -1;
  }
  data_buf = meta_buf + TEST_META_ALLOC_SIZE;
  unsigned long long data_buf_size = thread_local_size * num_compute * local_thread_num;
#ifdef DEBUG
  share_buf = meta_buf + DEBUG_ALLOC_SIZE - total_share_size;
#else
  share_buf = data_buf + data_buf_size;
#endif
  // allocated memory => | meta | data buffer region for all the thread | shared buffer region | unused |
#ifdef VERBOSE
  printf("Allocated: Meta [0x%llx - 0x%llx], Data [0x%llx - 0x%llx], Shared [0x%llx - 0x%llx]\n",
         (unsigned long long) meta_buf, (unsigned long long) meta_buf + TEST_META_ALLOC_SIZE,
         (unsigned long long) data_buf,
         (unsigned long long) (data_buf + data_buf_size),
         (unsigned long long) (share_buf),
         (unsigned long long) (share_buf + total_share_size));
#endif
  pthread_barrier_init(&s_barrier, NULL, local_thread_num + 1);
  pthread_barrier_init(&e_barrier, NULL, local_thread_num + 1);
  // let threads know all the remote nodes are initialized
  pthread_barrier_init(&rs_barrier, NULL, local_thread_num + 1);
  int k;
  printf("\n==Start test==\n");
  for (i = 0; i < local_thread_num; i++) {
    args[i].meta_buf = meta_buf;
    args[i].data_buf = data_buf;
    args[i].num_nodes = num_compute; // number of nodes that actually run this program
    args[i].node_idx = node_id;
    args[i].master_thread = (i == 0);
    args[i].logs = (char *) malloc(LOG_NUM_TOTAL * sizeof(struct RWlog)); // This should be allocated locally
    // if (fptr == NULL) {
    //   args[i].fd = NULL;
    // } else {
    //   args[i].fd = fptr[i];
    // }
    //
    args[i].access_size = (unsigned long) thread_local_size;
#ifdef VERBOSE
    printf("Setting access size to %lu for thread: %u\n", thread_local_size, i);
#endif
    args[i].thread_id = i;
    args[i].total_threads = local_thread_num;
    args[i].shared_buf = share_buf;
    args[i].num_compute_nodes = num_compute;
    args[i].node_id = node_id;
    args[i].shared_ratio = share_ratio;
    args[i].read_ratio = read_ratio;
    args[i].num_shared_pages = num_shared_pages;
    args[i].data_buf_size = data_buf_size;
    args[i].local_num_pages = local_num_pages;
    for (k = 0; k < CDF_BUCKET_NUM; k++) {
      args[i].cdf_cnt_p_r[k] = 0;
      args[i].cdf_cnt_s_r[k] = 0;
      args[i].cdf_cnt_p_w[k] = 0;
      args[i].cdf_cnt_s_w[k] = 0;
    }
  }

  int ret;

  for (i = 0; i < local_thread_num; i++) {
    if (pthread_create(&threads[i], NULL, (void *) mem_access_load_func, &args[i])) {
      printf("Error: cannot creating thread [%d]\n", i);
      return -2;
    }
  }

  // start time
  pthread_barrier_wait(&s_barrier);
#ifdef VERBOSE
  printf("Passed the initialization barrier\n");
#endif
  pthread_barrier_wait(&rs_barrier);
  clock_gettime(CLOCK_MONOTONIC, &start);
#ifdef VERBOSE
  printf("Passed the thread start barrier\n");
#endif
  // end time
  pthread_barrier_wait(&e_barrier);
  clock_gettime(CLOCK_MONOTONIC, &end);
  time_taken = (end.tv_sec - start.tv_sec) * 1e9;
  time_taken = (time_taken + (end.tv_nsec - start.tv_nsec)) * 1e-9;
  printf("Done in (%.9lf sec, %.4lf accesses/sec, total incl. remote)\n\n\n",
         time_taken, (double) (DATASET_SIZE * local_thread_num) / time_taken);
  fflush(stdout);
  {
    FILE *ofp = NULL;
    char out_file[255] = "";
    sprintf(out_file, "logs_03a_sharing_ratio/res_%d_sr%03d_rw%03d.log",
            node_id, share_ratio, read_ratio);
    ofp = fopen(out_file, "w+");
    if (ofp) {
      fprintf(ofp, "\nTotal threads: %d\n", local_thread_num);
      fprintf(ofp, "Dataset size (in pages): %lu\n", (unsigned long) DATASET_SIZE);
      fprintf(ofp, "Number of shared pages in pages: %d\n", num_shared_pages);
      fprintf(ofp, "Local number of pages is: %d\n", local_num_pages);
      fprintf(ofp, "Share ratio is: %d\n", share_ratio);
      fprintf(ofp, "Read ratio is: %d\n", read_ratio);
      fprintf(ofp, "[NOT USED] Spatial locality is: %d\n", spatial_locality);
      fprintf(ofp, "\nDone in (%.9lf sec, %.4lf accesses/sec, total incl. remote)\n",
              time_taken, (double) (DATASET_SIZE * local_thread_num) / time_taken);
      fclose(ofp);
    }
  }

  void *b;
  for (i = 0; i < local_thread_num; i++) {
    if (pthread_join(threads[i], &b) != 0) {
      printf("Error: cannot join thread [%d]\n", i);
      return -2;
    }
  }
#ifndef DEBUG
  printf("Wait 60 sec for the other threads\n");
  fflush(stdout);
  sleep(60);
#endif
#ifdef DEBUG
  munmap(meta_buf, DEBUG_ALLOC_SIZE);
#else
  munmap(meta_buf, TEST_MACRO_ALLOC_SIZE);
#endif
  return 0;
}

//TODO: Add spatial locality
//TODO: free after malloc
