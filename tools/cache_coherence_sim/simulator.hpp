#ifndef __SIMULATOR_H__
#define __SIMULATOR_H__

#include <unordered_map>
#include <chrono>
#include <thread>

/* config
 *
 */
// #define VERIFY
#define DYNAMIC_RESIZE
#define RECORD_CDF

#ifdef VERIFY
#define MAX_NUM_READS_TO_VERIFY 100000000
#define MAX_NUM_WRITES_TO_VERIFY 100000000
// #define LOG_NUM_TOTAL 15000000L
#define LOG_NUM_ONCE 1000L
#define MAX_PASS_COUNTER 100
// #define VERBOSE
// #define RWVERBOSE
#endif

#define TEST_ALLOC_FLAG MAP_PRIVATE|MAP_ANONYMOUS	// default: 0xef
#define TEST_INIT_ALLOC_SIZE 8L * 1024 * 1024 * 1024 // default: 16 GB
#define MAX_DIR_ENTRIES 30000L  // 8 K entries x 6 layers

#ifndef LOG_NUM_TOTAL
#define LOG_NUM_TOTAL 500000L    // make it enough, so that do not limit logs in that timewindow
#endif
#ifndef LOG_NUM_ONCE
#define LOG_NUM_ONCE LOG_NUM_TOTAL
#endif
#define TIMEWINDOW_US 100000L/*0000*/
#define LOG_MAP_ALIGN (15 * 4096)
#define MAX_WAIT_INV 3

#define MMAP_ADDR_MASK 0xffffffffffff
#define MAX_NUM_THREAD 80
#define MAX_NUM_NODES 8
#ifndef MAX_PASS_COUNTER
#define MAX_PASS_COUNTER 75000 //77376
#endif
#define MAX_DIR_SIZE 2097152    // 2MB
#define MIN_DIR_SIZE 4096       // 4KB
//
#define NUM_INVALIDATOR 8
//#define SLEEP_THRES_NANOS 10
//#define TEST_TO_APP_SLOWDOWN 1
//accounting
#define NUM_K 1000
#define NUM_M 1000000

using namespace std;

const struct timespec timeout = {0, 10000};
const struct timespec timesleep = {0, 100};

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

enum {
    ACK = 0,
    NACK = 1
};
enum {
    INVALID = 0,
    SHARED = 1,
    MODIFIED = 2,
    IS = 3,
    IM = 4,
    SM = 5,
    II = 6,
    SI = 7,
    MI = 8
};
inline int is_transient_state(int state) {
    return state >= IS ? 1 : 0;
}
inline int state_can_read(int state) {
    return state == SHARED || state == MODIFIED || state == SM/* || state == SI || state == MI*/;
}
inline int state_can_write(int state) {
    return state == MODIFIED/* || state == MI*/;
}

struct cache_line_t {
    pthread_mutex_t mtx;
    int8_t state;
    uint8_t *val;
    cache_line_t() {
        pthread_mutex_init(&mtx, NULL);
        state = INVALID;
    };
};

enum {
    R2C = 0,
    R2D = 1,
    W2C = 2,
    W2C_INV = 3,
    W2D = 4,
    ASYNC_INV = 5,     // invalidate the current page
    ASYNC_INV_WB = 6,     // invalidate and send back
    ANY_RA = 7,         // remote accesses
    ASYNC_INV_WB_PREV = 8,     // invalidate and send back
    NUM_EVENT = 9,
};

struct entry_t {
    pthread_mutex_t mtx;
    int state;
    uint16_t sharers;
    uint8_t *val;
    int ack_cnt;
    bool accessed;
    unsigned long event_cnt[NUM_EVENT]; // per entry counter
    unsigned long addr;
    size_t dir_size;
    entry_t() {
        pthread_mutex_init(&mtx, NULL);
        state = INVALID;// owner id, -1 indicates no owner
        sharers = 0;// bitmap of sharers
        memset(event_cnt, 0, sizeof(event_cnt));
        ack_cnt = 0;
    }
};

struct directory_t {
    // struct entry_t *entries;//pre allocated entries
    std::list<struct entry_t *> entry_list;
    std::unordered_map<unsigned long, struct entry_t *> entry_hash;
    unsigned long event_cnt[NUM_EVENT];//transition type 2 cnt map
    pthread_mutex_t dir_lock;//to simulate sequensial req handling in switch
    unsigned long max_dir_entries;
    directory_t(size_t _num_dir_blocks, size_t initial_dir_size, const size_t _max_dir_entries) {
        struct entry_t *entry = NULL;
        for (unsigned long i=0; i < _num_dir_blocks; i++)
        {
            entry = new entry_t;
            entry_list.push_back(entry);
            entry->dir_size = initial_dir_size;
        }
        memset(event_cnt, 0, sizeof(event_cnt));
        pthread_mutex_init(&dir_lock, NULL);
        max_dir_entries = _max_dir_entries;
    }
    ~directory_t() {
        while (!entry_list.empty())
        {
            struct entry_t *entry = entry_list.back();
            entry_list.pop_back();
            delete entry;
        }
    }
};
extern struct directory_t *dir;

enum {
    I2S = 0,
    I2M = 1,
    S2M = 2,
    NUM_TRANSITION = 3
};

enum {
    IDLE = 0,
    DONE_SYNC_WORK = 1,
};

enum {
    REQ_INV = 0,
    INV = 1
};

struct inv_data_t {
    unsigned long addr;
    unsigned len;
    int state;
    uint8_t *synced_data;
    int type;
    int state_after_transition;
    int sharers_after_transition;
    pthread_mutex_t inv_data_lock;
    inv_data_t(size_t _cache_line_size) {
        state = IDLE;
        pthread_mutex_init(&inv_data_lock, NULL);
        //synced_data = new uint8_t[_cache_line_size];
    }
    ~inv_data_t() {
        pthread_mutex_destroy(&inv_data_lock);
    }
};

struct cache_t;
struct trace_t;
int evict(struct cache_t *cache);
int invalidate(struct cache_t *cache);
struct cache_t {
    struct directory_t *dir;
    struct cache_line_t *lines;
    list<struct cache_line_t*> fifo;
    unordered_map<unsigned long, typename list<struct cache_line_t*>::iterator> fifo_hash;
    pthread_mutex_t fifo_mtx;
    int node_id;
    size_t max_cache_lines;
    unsigned long transition_cnt[NUM_TRANSITION];
    pthread_t evictor;
    pthread_t invalidator[NUM_INVALIDATOR];
    deque<struct inv_data_t *>inv_data_queue;
    pthread_mutex_t q_lock;
    pthread_mutex_t network_lock;//to simulate sequential usage of network
    unsigned long tot_invs, tot_inv_pages, tot_wb_pages;
    unsigned long *inv_histogram;
    unsigned long *wb_histogram;
    int terminate;

    cache_t(struct directory_t *_dir, int _node_id, int _num_nodes, size_t _cache_line_size,
            size_t _num_cache_lines, size_t _max_cache_lines) {
        dir = _dir;
        node_id = _node_id;
        lines = new struct cache_line_t[_num_cache_lines];
        max_cache_lines = _max_cache_lines;
        memset(transition_cnt, 0, sizeof(transition_cnt));
        terminate = 0;
        pthread_mutex_init(&fifo_mtx, NULL);
        pthread_create(&evictor, NULL, (void *(*)(void *))evict, (void *)this);
        for (int i=0; i<NUM_INVALIDATOR; i++)
            pthread_create(&invalidator[i], NULL, (void *(*)(void *))invalidate, (void *)this);
        pthread_mutex_init(&q_lock, NULL);
        pthread_mutex_init(&network_lock, NULL);
        //inv_data = new inv_data_t[_num_nodes];
        //for (int i = 0; i < _num_nodes; ++i) {
        //    inv_data[i].state = IDLE;
        //    pthread_mutex_init(&inv_data[i].inv_data_lock, NULL);
        //    inv_data[i].synced_data = new uint8_t[_cache_line_size];
        //}
        inv_histogram = new unsigned long[MAX_DIR_SIZE / _cache_line_size + 1];
        memset(inv_histogram, 0, sizeof(unsigned long) * MAX_DIR_SIZE / _cache_line_size + 1);
        wb_histogram = new unsigned long[MAX_DIR_SIZE / _cache_line_size + 1];
        memset(wb_histogram, 0, sizeof(unsigned long) * MAX_DIR_SIZE / _cache_line_size + 1);
    }
};
extern struct cache_t *caches[MAX_NUM_NODES];

enum
{
	LR = 0,
	LW = 1,
	RR = 2,
	RW = 3,
	N_ACC_TYPE = 4
};
struct acc_cnt_t {
	unsigned long cnt[N_ACC_TYPE];
	acc_cnt_t() {memset(cnt, 0, sizeof(cnt));}
	unsigned long &operator[] (int i) {return cnt[i];}
};
typedef unordered_map<unsigned long, acc_cnt_t> wbuf_t;

// cdf buckets
#define CDF_BUCKET_NUM	512

struct trace_t {

	struct acc_cnt_t acc_cnt;
	wbuf_t wbuf;
	FILE *PSO;
	FILE *RWCNT;

	char *logs;
    struct cache_t *cache;
	unsigned long offset;//mmap offset to log file
	unsigned long len;
    unsigned long cur_pass;
	bool done;
	int num_nodes;
	int tid;
	unsigned long time;
    int is_main_thread;
#ifdef VERIFY
    uint8_t *read_vals;
    unsigned long read_tail;
    uint8_t *write_vals;
    unsigned long write_tail;
#endif
    unsigned long r_cnt, w_cnt;

    trace_t() {
#ifdef VERIFY
        read_vals = new uint8_t[MAX_NUM_READS_TO_VERIFY];
        memset(read_vals, 0, sizeof(uint8_t) * MAX_NUM_READS_TO_VERIFY);
        read_tail = 0;
        write_vals = new uint8_t[MAX_NUM_WRITES_TO_VERIFY];
        memset(write_vals, 0, sizeof(uint8_t) * MAX_NUM_WRITES_TO_VERIFY);
        write_tail = 0;
#endif
        r_cnt = w_cnt = 0;
    }
#ifdef RECORD_CDF
    FILE *cdf_fp;
	unsigned long cdf_cnt_r[CDF_BUCKET_NUM] = {0};
	unsigned long cdf_cnt_w[CDF_BUCKET_NUM] = {0};
#endif
};
extern struct trace_t traces[MAX_NUM_THREAD];

struct load_arg_t {
	int fd;
	struct trace_t *arg;
	unsigned long ts_limit;
};
extern struct load_arg_t load_args[MAX_NUM_THREAD];

// int first;
extern int num_nodes;
extern int num_threads;
extern size_t cache_line_size;
extern size_t dir_block_size;
extern size_t cache_size;
extern size_t tot_mem;
extern FILE *fp;
extern uint8_t *data_in_mem;
extern unsigned int dyn_cache_interval;  // in terms of passes
extern unsigned int target_per_coeff;

inline void simulate_read_cache_latency() {;}
inline void simulate_write_cache_latency() {;}
inline void simulate_network_latency() {;}
inline void simulate_dir_fetch_latency() {;}
inline void simulate_dir_push_latency() {;}
inline void simulate_handle_req_latency() {;}
inline void simulate_handle_inv_latency() {;}
inline void simulate_handle_req_inv_latency() {;}

void print_cdf(struct trace_t *trace);

int load_log(void *void_arg);
void do_log(void *arg);
struct entry_t *find_entry_by_addr(struct directory_t *dir, unsigned long addr);

void do_cache_man(struct cache_t **caches, struct directory_t *dir, int num_nodes, int degree);
void print_directory_status(FILE *fstat, struct directory_t *dir);

// utility functions
int pin_to_core(int core_id);

#ifdef VERIFY
int verify(struct trace_t *traces);
#endif

extern pthread_barrier_t bar_before_load, bar_after_load, bar_before_run, bar_after_run;
#endif
