#ifndef __04_MACRO_TEST_PROGRAM_H__
#define __04_MACRO_TEST_PROGRAM_H__

// Number of logs that can be run in one step 
#define LOG_NUM_TOTAL (unsigned long)500000
#define TIMEWINDOW_US 100000
//
#define MMAP_ADDR_MASK 0xffffffffffff
#define MAX_NUM_THREAD 16
#define MAX_NUM_NODES 16
#define SLEEP_THRES_NANOS 10
#define LOG_MAP_ALIGN (15 * 4096)
#define PRINT_INTERVAL 5

// Test configuration flags
// #define meta_data_test

using namespace std;

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

// cdf buckets
#define CDF_BUCKET_NUM	512

struct trace_t {
	char *logs;
	unsigned long offset;//mmap offset to log file
	unsigned long len;
	bool done;
	bool all_done;
	bool write_res;
	char *meta_buf;
	char *data_buf;
	int node_idx;
	int num_nodes;
	int master_thread;
	int tid;
	unsigned long time;
	unsigned long last_dt;
	FILE *cdf_fp;
	unsigned long cdf_cnt_r[CDF_BUCKET_NUM] = {0};
	unsigned long cdf_cnt_w[CDF_BUCKET_NUM] = {0};
};
struct trace_t args[MAX_NUM_THREAD];

struct load_arg_t {
	int fd;
	struct trace_t *arg;
	unsigned long ts_limit;
	bool all_done;
};
struct load_arg_t load_args[MAX_NUM_THREAD];

struct metadata_t {
	unsigned int node_mask[MAX_NUM_NODES];
	unsigned int fini_node_step[MAX_NUM_NODES];
};

#endif /* __04_MACRO_TEST_PROGRAM_H__ */