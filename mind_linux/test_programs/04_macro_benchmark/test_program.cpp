// Test program to allocate new memory
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cstdlib>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <pthread.h>
#include <sched.h>
#include "../../include/disagg/config.h"
#include <fstream>
#include <cassert>
#include <map>
#include "test_program.h"

// #define VERBOSE

static int latency_to_bkt(unsigned long lat_in_us)
{
	if (lat_in_us < 100)
		return (int)lat_in_us;
	else if (lat_in_us < 1000)
		return 100 + ((lat_in_us - 100) / 10);
	else if (lat_in_us < 10000)
		return 190 + ((lat_in_us - 1000) / 100);
	else if (lat_in_us < 100000)
		return 280 + ((lat_in_us - 10000) / 1000);
	else if (lat_in_us < 1000000)
		return 370 + ((lat_in_us - 100000) / 10000);
	return CDF_BUCKET_NUM - 1;	// over 1 sec
}

// Global variables
int num_nodes;
int node_id = -1;
int num_threads;
unsigned int max_step = 10000;
static unsigned long step = 0;
static pthread_barrier_t s_barrier;
static pthread_barrier_t load_barrier, run_barrier, update_barrier, cont_barrier;
static pthread_mutex_t cpu_pin_mutex = PTHREAD_MUTEX_INITIALIZER;

static int calc_mask_sum(unsigned int mask)
{
	int sum = 0;
	while (mask > 0)
	{
		if (mask & 0x1)
			sum++;
		mask >>= 1;
	}
	return sum;
}

static bool check_ready_nodes(struct metadata_t *meta_ptr, int cur_step)
{
	int done_cnt = 0;
	for (int j = 0; j < MAX_NUM_NODES; ++j)
	{
		if (meta_ptr->node_mask[j] <= cur_step)
		{
			;
		}else{
			done_cnt ++;
		}
	}
	return (done_cnt >= num_nodes ? 1 : 0);
}

static int phase = 0;
int init(struct trace_t *trace)
{
	if(trace && trace->meta_buf)
	{
		struct metadata_t *meta_ptr = (struct metadata_t *)trace->meta_buf;
		// write itself
		if (trace->master_thread)
		{
#ifdef VERBOSE
			printf("Start step [%lu]\n", step);
#endif
			meta_ptr->node_mask[trace->node_idx] = step + 1;

			// check nodes
			int i = 0;
			while (!check_ready_nodes(meta_ptr, step))
			{
				if (i % 100 == 0)
				{
					char node_bits[256] = "";
					for (int j = 0; j < MAX_NUM_NODES; ++j)
						sprintf(node_bits, "%s[%u]", node_bits, meta_ptr->node_mask[j]);
#ifdef VERBOSE
					printf("Waiting nodes [%03d]: %d [step:%lu -> %u]/[%u] || [%s]\n",
						   (i / 100) % 100, trace->num_nodes, step, meta_ptr->node_mask[trace->node_idx], max_step, node_bits);
#endif
				}
				if (i % 200 == 0)
				{
#ifdef meta_data_test
					if (meta_ptr->node_mask[trace->node_idx] != step + 1)
						meta_ptr->node_mask[trace->node_idx] = step + 1;
#endif
				}
				usleep(10000);
				i++;
			}
#ifdef VERBOSE
			printf("All nodes are initialized: %d [%u]\n", trace->num_nodes, meta_ptr->node_mask[trace->node_idx]);
#endif
			phase = !phase;
			pthread_barrier_wait(&s_barrier);
#ifdef VERBOSE
			fprintf(stderr, "Start step [%lu] after barrier\n", step);
#endif
			return 0;
		}else{
			pthread_barrier_wait(&s_barrier);
			return 0;
		}
	}
	return -1;
}

int fini(struct metadata_t *meta_buf, int num_nodes, int node_id)
{
	int done_cnt = 0;
	int i = 0;

	meta_buf->fini_node_step[node_id] = step;

	while (done_cnt < num_nodes)
	{
		usleep(10000);
		done_cnt = 0;
		for (int j = 0; j < MAX_NUM_NODES; ++j)
		{
			if (meta_buf->fini_node_step[j] < step)
			{
				;
			}
			else
			{
				done_cnt ++;
			}
		}
		if (i % 200 == 0)
		{
#ifdef VERBOSE
			printf("Waiting for next step [%lu] || init[%u]\n", step, meta_buf->node_mask[node_id]);
#endif
			meta_buf->fini_node_step[node_id] = step;
			if (meta_buf->node_mask[node_id] != step)
			{
				printf("ERROR: inconsist step [%lu]<->[%u]\n", step, meta_buf->node_mask[node_id]);
				meta_buf->node_mask[node_id] = step;
			}
		}
		++i;
	}
	return 0;
}

int pin_to_core(int core_id)
{
    int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    if (core_id < 0 || core_id >= num_cores)
        return -1;

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    pthread_t current_thread = pthread_self();
    return pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
}

inline void interval_between_access(long delta_time_usec) {
	if (delta_time_usec <= 0)
		return;
	else {
		struct timespec ts;
		unsigned long app_time_nsec = (delta_time_usec << 1) / 3;
		if (app_time_nsec > SLEEP_THRES_NANOS) {
			ts.tv_sec = 0;
			ts.tv_nsec = app_time_nsec;
			nanosleep(&ts, NULL);
		}
	}
}

static inline unsigned long calculate_dt(struct timeval *ts)
{
	unsigned long old_t = ts->tv_sec * 1000000 + ts->tv_usec;
	gettimeofday(ts, NULL);
	return ts->tv_sec * 1000000 + ts->tv_usec - old_t;

}

static inline void measure_time_start(struct timeval *ts)
{
#ifdef __TEST_TIME_MEASUREMENT__
	gettimeofday(ts, NULL);
#endif
}

static inline unsigned long measure_time_end(struct timeval *ts)
{
#ifdef __TEST_TIME_MEASUREMENT__
	return calculate_dt(ts);
#endif
}

static inline void record_time(struct trace_t *trace, unsigned long dt_op, int is_read)
{
#ifdef __TEST_TIME_MEASUREMENT__
	if (trace)
	{
		if (is_read)
		{
			trace->cdf_cnt_r[latency_to_bkt(dt_op)]++;
		}
		else
		{
			trace->cdf_cnt_w[latency_to_bkt(dt_op)]++;
		}
	}
#endif
}

static inline void flush_cdf_record(struct trace_t *trace)
{
#ifdef __TEST_TIME_MEASUREMENT__
	if (trace->cdf_fp)
		fflush(trace->cdf_fp);
#endif
}

static void print_cdf(struct trace_t *trace)
{
	int i = 0;
	if (trace && trace->cdf_fp)
	{
		fprintf(trace->cdf_fp, "Pass: %lu\n", step);
		// read
		fprintf(trace->cdf_fp, "Read:\n");
		for (i = 0; i < CDF_BUCKET_NUM; i++)
			fprintf(trace->cdf_fp, "%lu\n", trace->cdf_cnt_r[i]);
		// write
		fprintf(trace->cdf_fp, "Write:\n");
		for (i = 0; i < CDF_BUCKET_NUM; i++)
			fprintf(trace->cdf_fp, "%lu\n", trace->cdf_cnt_w[i]);
		fprintf(trace->cdf_fp, "\n");
	}
	flush_cdf_record(trace);
}


void do_log(void *arg)
{
	struct trace_t *trace = (struct trace_t*) arg;
	unsigned len = trace->len;
	if (init(trace))
	{
		fprintf(stderr, "Initialization error!\n");
		return;
	}

#ifndef meta_data_test
	multimap<unsigned int, void *> len2addr;
	unsigned long old_ts = 0;
	unsigned long i = 0;

	struct timeval ts, ts_op;
	unsigned long dt = 0, dt_op = 0;
	unsigned long print_int = trace->len / PRINT_INTERVAL;
	char *cur;
	gettimeofday(&ts, NULL);

	// fprintf(stderr, "Start test [%d]\n", trace->tid);
	for (i = 0; i < trace->len; ++i) {
		volatile char op = trace->logs[i * sizeof(RWlog)];
		cur = &(trace->logs[i * sizeof(RWlog)]);
		if (op == 'R')
		{
			struct RWlog *log = (struct RWlog *)cur;
			interval_between_access(log->usec - old_ts);
			char *data_buf = trace->data_buf;
			unsigned long addr = (log->addr & MMAP_ADDR_MASK);
			measure_time_start(&ts_op);
			volatile char val = data_buf[addr];
			dt_op = measure_time_end(&ts_op);
			record_time(trace, dt_op, 1);
			old_ts = log->usec;
		}
		else if (op == 'W')
		{
			struct RWlog *log = (struct RWlog *)cur;
			interval_between_access(log->usec - old_ts);
			char *data_buf = trace->data_buf;
			unsigned long addr = (log->addr & MMAP_ADDR_MASK);
			measure_time_start(&ts_op);
			data_buf[addr] = 0;
			dt_op = measure_time_end(&ts_op);
			record_time(trace, dt_op, 0);
			old_ts = log->usec;
		}
		else if (op == 'M')
		{
			struct Mlog *log = (struct Mlog *)cur;
			interval_between_access(log->hdr.usec);
			void *ret_addr = mmap((void *)(log->start & MMAP_ADDR_MASK), log->len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
			unsigned int len = log->len;
			len2addr.insert(pair<unsigned int, void *>(len, ret_addr));
			old_ts += log->hdr.usec;
		}
		else if (op == 'B')
		{
			struct Blog *log = (struct Blog *)cur;
			int res = 0;
			interval_between_access(log->usec - old_ts);
			res = brk((void *)(log->addr & MMAP_ADDR_MASK));
			old_ts = log->usec;
		}
		else if (op == 'U')
		{
			struct Ulog *log = (struct Ulog *)cur;
			interval_between_access(log->hdr.usec);
			multimap<unsigned int, void *>::iterator itr = len2addr.find(log->len);
			if (itr == len2addr.end())
				;
			else {
				munmap(itr->second, log->len);
				len2addr.erase(itr);
			}
			old_ts += log->hdr.usec;
		}
		else
		{
			fprintf(stderr, "unexpected log: %c at line: %lu\n", op, i);
		}
	}
	dt = calculate_dt(&ts);
#endif
	trace->time += dt;
	trace->last_dt = dt;
#ifdef VERBOSE
	if (trace->master_thread)
		fprintf(stderr, "total run time is %lu us\n", trace->time);
#endif
}

int load_trace(void *void_arg) {
	
	struct load_arg_t *load_arg = (struct load_arg_t *)void_arg;
	int fd = load_arg->fd;
	struct trace_t *arg = load_arg->arg;	
	unsigned long ts_limit = load_arg->ts_limit;

	if (arg->logs) {
		munmap(arg->logs, LOG_NUM_TOTAL * sizeof(RWlog));
		arg->logs = NULL;
	}
	arg->logs = (char *)mmap(NULL, LOG_NUM_TOTAL * sizeof(RWlog), PROT_READ, MAP_PRIVATE, fd, arg->offset);

	unsigned long new_offset = 0;
	// Walk through logs to find the end of timewindow also trigger demand paging
	for (char *cur = arg->logs; cur != arg->logs + LOG_NUM_TOTAL * sizeof(RWlog); cur += sizeof(RWlog)) {
		if (*cur == 'R' || *cur == 'W' || *cur == 'B') {
			if (((struct RWlog *)cur)->usec >= ts_limit && !new_offset)
				new_offset = (arg->offset + (cur - arg->logs)) / LOG_MAP_ALIGN * LOG_MAP_ALIGN;
		} else if (*cur == 'M' || *cur == 'U') {
			continue;
		} else {
			new_offset = (arg->offset + (cur - sizeof(RWlog) - arg->logs)) / LOG_MAP_ALIGN * LOG_MAP_ALIGN;
#ifdef VERBOSE
			printf("unexpected op %c\n", *cur);
#endif
			arg->done = true;
            break;
		}
	}
	// If the offset is the same as the old one due to align
	if (arg->offset != new_offset)
		assert(new_offset);

	arg->len = (new_offset - arg->offset) / sizeof(RWlog);
	arg->offset = new_offset;
	return 0;
}

int load_trace_helper(void *void_arg)
{
	struct load_arg_t *load_arg = (struct load_arg_t *)void_arg;
	while(!load_arg->all_done)
	{
		pthread_barrier_wait(&update_barrier);
		load_trace(void_arg);
		pthread_barrier_wait(&load_barrier);
		pthread_barrier_wait(&run_barrier);
		pthread_barrier_wait(&cont_barrier);
	}
}

static int used_cpu[MAX_NUM_THREAD] = {0};
static void pin_this_thread(int thread_id)
{
	int cpu_id;
	while(1)
	{
		pthread_mutex_lock(&cpu_pin_mutex);
		cpu_id = sched_getcpu();
		if (used_cpu[cpu_id])
		{
			pthread_mutex_unlock(&cpu_pin_mutex);
			usleep(1000000);
			continue;
		}
		pin_to_core(cpu_id);
		used_cpu[cpu_id] = thread_id + 1;
#ifdef VERBOSE
		fprintf(stderr, "Pin Thread [%d] -> CPU [%d]\n", thread_id, cpu_id);
#endif
		pthread_mutex_unlock(&cpu_pin_mutex);
		break;
	}
}

static FILE *res, *progress;
unsigned long total_max_time = 0;
static void print_time(void)
{
	char progress_text[256] = "";
	if (progress)
	{
		memset(progress_text, 0, 256);
		sprintf(progress_text, "Pass[%lu] Node[%d] || Time [%lu] || ", step, node_id, total_max_time);
		for (int i=0; i<num_threads; i++)
		{
			sprintf(progress_text, "%s %lu, ", progress_text, args[i].time);
		}
		fprintf(progress, "%s in us\n", progress_text);
		fflush(progress);
		printf("%s in us\n", progress_text);
		fflush(stdout);
	}
}

void do_log_helper(void *arg)
{
	struct trace_t *trace = (struct trace_t *)arg;
	unsigned long dt_cur_step = 0;

	pin_to_core(trace->tid);
#ifdef VERBOSE
	fprintf(stderr, "Pin Thread [%d] -> CPU [%d]\n", trace->tid, trace->tid);
#endif
	while (!trace->all_done)
	{
		if (trace->master_thread)
		{
			// print out maximum time
			if (step % 1000 == 0)
			{
				// update maximum time
				for (int i=0; i<num_threads; i++)
				{
					if (args[i].time > total_max_time)
						total_max_time = args[i].time;
				}
				print_time();
			}
		}

		if (step % 1000 == 0)
		{
			print_cdf(trace);
		}
		pthread_barrier_wait(&update_barrier);
		pthread_barrier_wait(&load_barrier);
		do_log(arg);
		pthread_barrier_wait(&run_barrier);
		pthread_barrier_wait(&cont_barrier);
	}
}

enum
{
	arg_node_cnt = 1,
	arg_node_id = 2,
	arg_num_threads = 3,
	arg_max_step = 4,
	arg_root = 5,
	arg_log1 = 6,
};

int main(int argc, char **argv)
{
   	int ret;
	char *buf_test = NULL;

	assert(sizeof(RWlog) == sizeof(Mlog));
	assert(sizeof(RWlog) == sizeof(Blog));
	assert(sizeof(RWlog) == sizeof(Ulog));

	if (argc < arg_log1)
	{
		fprintf(stderr, "Incomplete args\n");
		return 1;
	}
	num_nodes = atoi(argv[arg_node_cnt]);
	node_id = atoi(argv[arg_node_id]);
	num_threads = atoi(argv[arg_num_threads]);
	max_step = atoi(argv[arg_max_step]) + 1;
	printf("Num Nodes: %d, Num Threads: %d\n", num_nodes, num_threads);
	if (argc != arg_log1 + num_threads) {
		fprintf(stderr, "thread number [%d] and log files provided not match [%d <-> %d]\n",
			num_threads, argc, arg_log1 + num_threads);
                return 1;
	}

	// open files
	for (int i = 0; i < num_threads; ++i)
	{
		load_args[i].fd = open(argv[arg_log1 + i], O_RDONLY);
#ifdef VERBOSE
		printf("Open: %s\n", argv[arg_log1 + i]); 
#endif
		if (load_args[i].fd < 0) {
			printf("fail to open log input file %d\n", load_args[i].fd);
			return 1;
		}
		load_args[i].arg = &args[i];
	}
	char tmp_file[256], progress_file[256];
	sprintf(tmp_file, "%s/tmp.txt", argv[arg_root]);
	sprintf(progress_file, "%s/progress.txt", argv[arg_root]);
	res = fopen(tmp_file, "w");
	progress = fopen(progress_file, "w");
	if (!res || !progress)
	{
		printf("Cannot open progress log files: %s=>%p, %s=>%p\n", tmp_file, res, progress_file, progress);
		return -1;
	}

	pthread_barrier_init(&s_barrier, NULL, num_threads);
	// over main, loader, and runner
	pthread_barrier_init(&load_barrier, NULL, 2 * num_threads + 1);
	pthread_barrier_init(&run_barrier, NULL, 2 * num_threads + 1);
	pthread_barrier_init(&update_barrier, NULL, 2 * num_threads + 1);
	pthread_barrier_init(&cont_barrier, NULL, 2 * num_threads + 1);
	//get start ts
	struct RWlog first_log;
	unsigned long start_ts = -1;
	for (int i = 0; i < num_threads; ++i) 
	{
		int size = read(load_args[i].fd, &first_log, sizeof(RWlog));
		start_ts = min(start_ts, first_log.usec);
		load_args[i].all_done = false;
	}
	// ======== memory mapping =========
#ifndef __NORMAL_MAP__
	char *meta_buf = (char *)mmap(NULL, TEST_META_ALLOC_SIZE, PROT_READ | PROT_WRITE, TEST_ALLOC_FLAG, -1, 0);
#else
	char *meta_buf = (char *)mmap(NULL, TEST_META_ALLOC_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
#endif
	if (!meta_buf || meta_buf == (void *)-1)
	{
		printf("Error: cannot allocate buffer [0x%lx]\n", (unsigned long)meta_buf);
		return -1;
	}

	/* put this back when run with switch */
#ifndef __NORMAL_MAP__
	char *data_buf = (char *)mmap(NULL, TEST_MACRO_ALLOC_SIZE, PROT_READ | PROT_WRITE, TEST_ALLOC_FLAG, -1, 0);
#else
	char *data_buf = (char *)mmap(NULL, TEST_MACRO_ALLOC_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
#endif
	if (!data_buf || data_buf == (void *)-1)
	{
		printf("Error: cannot allocate buffer [0x%lx]\n", (unsigned long)data_buf);
		return -1;
	}
#ifdef VERBOSE
	printf("protocol testing buf addr is: %p\n", data_buf);
	printf("Allocated: Meta [0x%llx - 0x%llx], Data [0x%llx - 0x%llx]\n",
		   (unsigned long long)meta_buf, (unsigned long long)meta_buf + TEST_META_ALLOC_SIZE,
		   (unsigned long long)data_buf, (unsigned long long)data_buf + TEST_MACRO_ALLOC_SIZE);
#endif
	// =================
	for (int i = 0; i < num_threads; ++i)
	{
		char cdf_file_name[256] = "";
		args[i].node_idx = node_id;
		args[i].meta_buf = meta_buf;
		args[i].data_buf = data_buf;
		args[i].num_nodes = num_nodes;
		args[i].master_thread = (i == 0);
		args[i].tid = i;
		args[i].all_done = false;
		args[i].write_res = false;
		args[i].time = 0;
		args[i].last_dt = 0;
		sprintf(cdf_file_name, "%s/cdf_C%02d_T%02d.txt", argv[arg_root], node_id, i);
		args[i].cdf_fp = fopen(cdf_file_name, "w+");
		if (!args[i].cdf_fp) {
			printf("fail to open cdf output file: %s\n", cdf_file_name);
			return 1;
		}
	}

	// start load and run logs in time window
	unsigned long ts_limit = start_ts;
#ifdef VERBOSE
	printf("Start main loop\n");
#endif
	pthread_t load_thread[MAX_NUM_THREAD];
	pthread_t run_thread[MAX_NUM_THREAD];

	// create threads
	for (int i = 0; i < num_threads; ++i)
	{
		if (pthread_create(&load_thread[i], NULL, (void *(*)(void *))load_trace_helper, &load_args[i]))
		{
			printf("Error creating loader thread %d\n", i);
			return 1;
		}

		if (pthread_create(&run_thread[i], NULL, (void *(*)(void *))do_log_helper, &args[i]))
		{
			printf("Error creating runner thread %d\n", i);
			return 1;
		}
	}

	step = 0;
	while (1)
	{
		ts_limit += TIMEWINDOW_US;
		for (int i = 0; i < num_threads; ++i) {
			load_args[i].ts_limit = ts_limit;
		}
		pthread_barrier_wait(&update_barrier);
		pthread_barrier_wait(&load_barrier);
		pthread_barrier_wait(&run_barrier);

		// sync on the end of the time window
		++step;
#ifdef VERBOSE
		fprintf(stderr, "Prepare for next step [%lu]\n", step);
#endif
		fini((metadata_t *)meta_buf, num_nodes, node_id);

		bool all_done = true;
		for (int i = 0; i < num_threads; ++i)
			if (!args[i].done)
				all_done = false;
		
		// check exti condition
		if (step > max_step)
		{
			all_done = true;
		}

		if (all_done)
		{
			for (int i = 0; i < num_threads; ++i)
			{
				args[i].all_done = true;
				load_args[i].all_done = true;
			}
			asm volatile("mfence" : : : "memory");	// barrier - may be not needed..
			pthread_barrier_wait(&cont_barrier);
			break;
		}
		pthread_barrier_wait(&cont_barrier);
	}
	printf("Wait for 60 sec before exit...\n");
	fflush(stdout);
	sleep(60);

	// loaders
	for (int i = 0; i < num_threads; ++i)
	{
		if (pthread_join(load_thread[i], NULL))
		{
			printf("Error joining thread %d\n", i);
			return 2;
		}
	}
	// runners
	for (int i = 0; i < num_threads; ++i)
	{
		if (pthread_join(run_thread[i], NULL))
		{
			printf("Error joining thread %d\n", i);
			return 2;
		}
	}

	for (int i = 0; i < num_threads; ++i) {
		close(load_args[i].fd);
		fclose(args[i].cdf_fp);
	}
	fclose(res);
	fclose(progress);
	return 0;
}
