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
#include <fstream>
#include <cassert>
#include <map>
#include <list>
#include <queue>
#include <unordered_map>
#include <sched.h>
#include "simulator.hpp"

enum
{
	arg_node_cnt = 1,
	arg_num_threads = 2,
    arg_cache_line_size = 3,
    arg_dir_block_size = 4,
    arg_cache_size = 5,
    arg_tot_mem = 6,
    arg_dyn_cache_int = 7,
    arg_perf_coeff = 8,
    arg_dyn_cache_degree = 9,
	arg_max_dir_entry = 10,
    arg_log1 = 11,
};

struct directory_t *dir;
struct cache_t *caches[MAX_NUM_NODES];
struct trace_t traces[MAX_NUM_THREAD];
struct load_arg_t load_args[MAX_NUM_THREAD];
uint8_t *data_in_mem = NULL;
int num_nodes;
int num_threads;
size_t cache_line_size;
size_t dir_block_size;
size_t cache_size;
size_t tot_mem;
const size_t dir_block_max_size = 2097152;  // 2MB
FILE *fp;
pthread_barrier_t bar_before_load, bar_after_load, bar_before_run, bar_after_run, bar_next;
bool all_done = false;
unsigned int dyn_cache_interval;  // in terms of passes
unsigned int target_per_coeff;
unsigned long pass = 0;

struct args_t{
    trace_t *do_arg;
    load_arg_t *load_arg;
};
struct args_t context_args[MAX_NUM_THREAD];

int helper_load_log(void *void_arg)
{
    return load_log(void_arg);
}

void helper_do_log(void *arg)
{
    return do_log(arg);
}

static bool cpu_map[512] = {false};
pthread_mutex_t cpu_map_mtx;
void helper_thread_ftn(void *args)
{
    struct args_t *load_do_args = (struct args_t *)args;
    int cpu = sched_getcpu();
    unsigned long pass = 0;
#if 0
retry_pin_cpu:
    pthread_mutex_lock(&cpu_map_mtx);
    cpu = sched_getcpu();
    if (!cpu_map[cpu])
    {
        //pin to core first
        cpu_map[cpu] = true;
        pin_to_core(cpu);
        pthread_mutex_unlock(&cpu_map_mtx);
    }else{
        pthread_mutex_unlock(&cpu_map_mtx);
        sleep(1);
        goto retry_pin_cpu;
    }
#endif
    printf("Thread launched id[%d] cpu[%d]\n",
           load_do_args->do_arg->tid, cpu);
    while (!all_done)
    {
        bool already_done = load_do_args->do_arg->done;
        if (pass % 1000 == 0)
		{
			print_cdf(load_do_args->do_arg);
		}
        pthread_barrier_wait(&bar_before_load);
        helper_load_log(load_do_args->load_arg);
        pthread_barrier_wait(&bar_after_load);
        pthread_barrier_wait(&bar_before_run);
        if (!already_done)
        {
            load_do_args->do_arg->cur_pass = pass;
            helper_do_log(load_do_args->do_arg);
        }
        pthread_barrier_wait(&bar_after_run);
        pthread_barrier_wait(&bar_next);
        __sync_synchronize();
        pass ++;
    }

    printf("Terminate thread id[%d] cpu[%d]\n",
           load_do_args->do_arg->tid, cpu);
}

int main(int argc, char **argv)
{
    int dyn_split_merge_degree = 1;
    unsigned long max_dir_entry = 0;
    int log_start = arg_max_dir_entry;  // the first optional arg
    if (argc < arg_max_dir_entry)
    {
        fprintf(stderr, "Incomplete args\n");
        return 1;
    }
    num_nodes = atoi(argv[arg_node_cnt]);
    num_threads = atoi(argv[arg_num_threads]);
    cache_line_size = atol(argv[arg_cache_line_size]) /* * 4096*/;
    dir_block_size = atol(argv[arg_dir_block_size]) /* * 4096*/;
    cache_size = atol(argv[arg_cache_size]) /* * 1024 * 1024*/;
    tot_mem = atol(argv[arg_tot_mem]) /* * 1024 * 1024*/;
    dyn_cache_interval = atoi(argv[arg_dyn_cache_int]);
    target_per_coeff = atoi(argv[arg_perf_coeff]);
    dyn_split_merge_degree = atoi(argv[arg_dyn_cache_degree]);

    // init caches and directories
    size_t num_cache_lines = (tot_mem + cache_line_size - 1) / cache_line_size;    // # preallocated cache lines
    size_t max_cache_lines = (cache_size + cache_line_size - 1) / cache_line_size; // # upper limit cache lines
    size_t num_dir_blocks = (tot_mem + dir_block_size - 1) / dir_block_size;       // # preallocated dir blocks

    printf("Num Nodes: %d, Num Threads: %d\n", num_nodes, num_threads);
    if (argc == arg_log1 + num_threads)
    {
        max_dir_entry = atoi(argv[arg_max_dir_entry]);
        log_start = arg_log1;
    }
    else if (argc == arg_max_dir_entry + num_threads)
    {
        max_dir_entry = num_dir_blocks;
        log_start = arg_max_dir_entry;
    }else{
        fprintf(stderr, "thread number and log files provided not match\n");
        return 1;
    }
    pthread_barrier_init(&bar_before_load, NULL, num_threads + 1);
    pthread_barrier_init(&bar_after_load, NULL, num_threads + 1);
    pthread_barrier_init(&bar_before_run, NULL, num_threads + 1);
    pthread_barrier_init(&bar_after_run, NULL, num_threads + 1);
    pthread_barrier_init(&bar_next, NULL, num_threads + 1);
    pthread_mutex_init(&cpu_map_mtx, NULL);

    //open files
    for (int i = 0; i < num_threads; ++i)
    {
        load_args[i].fd = open(argv[log_start + i], O_RDONLY);
        if (load_args[i].fd < 0)
        {
            printf("fail to open log file: %s\n", argv[log_start + i]);
            return 1;
        }
        load_args[i].arg = &traces[i];
    }
    int num_threads_per_node = num_threads / num_nodes;
    std::string postfix = to_string(num_nodes) + string("n_") + to_string(num_threads_per_node) + string("t.") + to_string(dir_block_size / cache_line_size);
    postfix += string("_") + to_string(dyn_cache_interval) + string(".") + to_string(target_per_coeff);
    postfix += string("_d") + to_string(dyn_split_merge_degree);

    FILE *progress = fopen((string("logs/progress.") + postfix).c_str(), "w");
    FILE *fstat = fopen((string("logs/stat.")  + postfix).c_str(), "w");

    //get start ts
    struct RWlog first_log;
    unsigned long start_ts = -1;
    for (int i = 0; i < num_threads; ++i)
    {
        if (read(load_args[i].fd, &first_log, sizeof(RWlog)) < 0)
            printf("fail to read first log\n");
        start_ts = min(start_ts, first_log.usec);
    }

    // intiailize memory and directory structure
    dir = new directory_t(num_dir_blocks, dir_block_size, max_dir_entry);
#ifdef VERIFY
    data_in_mem = new uint8_t[num_cache_lines * cache_line_size];
    memset(data_in_mem, 0, sizeof(uint8_t) * num_cache_lines * cache_line_size);
#endif
    unsigned long tmp_addr = 0x0;
    for (auto const& i : dir->entry_list)
    {
        i->addr = tmp_addr;
#ifdef VERIFY
        i->val = &data_in_mem[tmp_addr];
#endif
        for (unsigned long off=0; off < i->dir_size; off += MIN_DIR_SIZE)
            dir->entry_hash[tmp_addr + off] = i;    // page granularity mapping
        // to the next cache directory entry
        tmp_addr += dir_block_size;
    }

    for (int i = 0; i < num_nodes; ++i)
    {
        caches[i] = new cache_t(dir, i, num_nodes, cache_line_size,
                                num_cache_lines, max_cache_lines);
#ifdef VERIFY
        for (int j = 0; j < num_cache_lines; ++j)
        {
            caches[i]->lines[j].val = new uint8_t[cache_line_size];
            memset(caches[i]->lines[j].val, 0, sizeof(uint8_t) * cache_line_size);
        }
#endif
    }

    // init traces
    for (int i = 0; i < num_threads; ++i)
    {
        traces[i].num_nodes = num_nodes;
        traces[i].tid = i;
        traces[i].cache = caches[i / num_threads_per_node];
        traces[i].PSO = fopen((string("logs/pso/") + to_string(i) + "_" + postfix).c_str(), "w");
        traces[i].RWCNT = fopen((string("logs/rwcnt/") + to_string(i) + "_" + postfix).c_str(), "w");
        assert(traces[i].PSO && traces[i].RWCNT);
        traces[i].is_main_thread = (i == 0);
#ifdef VERIFY
        for (unsigned long j = 0; j < MAX_NUM_WRITES_TO_VERIFY; ++j)
            traces[i].write_vals[j] = (uint8_t)((j + 1) * (i + 1));
#endif
#ifdef RECORD_CDF
        char cdf_file_name[256] = "";
        sprintf(cdf_file_name, "%s/cdf_C%02d_T%02d.txt", "logs/cdf/", (int)(i / num_threads_per_node), i % num_threads_per_node);
		traces[i].cdf_fp = fopen(cdf_file_name, "w+");
		if (!traces[i].cdf_fp) {
			printf("fail to open cdf output file: %s\n", cdf_file_name);
			return 1;
		}
#endif
    }

    //#ifdef VERBOSE
    fp = fopen("logs/tmp.txt", "w");
    //#endif

    //start load and run logs in time window
    unsigned long ts_limit = start_ts;
    pthread_t thread[MAX_NUM_THREAD];

    for (int i = 0; i < num_threads; ++i)
    {
        context_args[i].do_arg = &traces[i];
        context_args[i].load_arg = &load_args[i];
        if (pthread_create(&thread[i], NULL, (void *(*)(void *))helper_thread_ftn, &context_args[i]))
        {
            printf("Error creating thread %d\n", i);
            return 1;
        }
    }

    while (1)
    {
        ts_limit += TIMEWINDOW_US;

        //load logs
        for (int i = 0; i < num_threads; ++i)
        {
            load_args[i].ts_limit = ts_limit;
        }
        __sync_synchronize();
        pthread_barrier_wait(&bar_before_load);
        pthread_barrier_wait(&bar_after_load);

        //clear accessed mark
        for (int i = 0; i < num_dir_blocks; ++i)
        {
            struct entry_t *entry = find_entry_by_addr(dir, i * dir_block_size);
            entry->accessed = false;
        }
        __sync_synchronize();
        pthread_barrier_wait(&bar_before_run);
        pthread_barrier_wait(&bar_after_run);
        ++pass;

        // run dynamic cache block size management here
        unsigned long tot_invs = 0, r_cnt = 0, w_cnt = 0, entry_cnt = 0;
        double tot_inv_pages = 0, tot_wb_pages = 0;
        unsigned long tot_async_wb = 0;
        for (int i = 0; i < num_nodes; ++i)
        {
            struct cache_t *cache = caches[i];
            tot_invs += cache->tot_invs;
            tot_inv_pages += cache->tot_inv_pages;
            tot_wb_pages += cache->tot_wb_pages;
        }
        if (pass % dyn_cache_interval == 0)
        {
            for (auto const &i : dir->entry_list)
            {
                tot_async_wb += i->event_cnt[ASYNC_INV_WB];
            }
#ifdef DYNAMIC_RESIZE
            // unsigned long perf_tar = tot_wb_pages / num_dir_blocks; // evenly over cache blocks
            do_cache_man(caches, dir, num_nodes, dyn_split_merge_degree);
#endif
            // print out result
            print_directory_status(fstat, dir);
            fprintf(fstat, "inv_window %lu %lu\n", (unsigned long)tot_wb_pages, tot_async_wb);
            fflush(fstat);
        }

        //stat
        for (int i = 0; i < num_nodes; ++i)
        {
            struct cache_t *cache = caches[i];
            cache->tot_invs = cache->tot_inv_pages = cache->tot_wb_pages = 0;
        }
        for (int i = 0; i < num_threads; ++i)
        {
            struct trace_t *trace = &traces[i];
            r_cnt += trace->r_cnt;
            w_cnt += trace->w_cnt;
            trace->r_cnt = trace->w_cnt = 0;
        }
        for (int i = 0; i < num_dir_blocks; ++i)
        {
            struct entry_t *entry = find_entry_by_addr(dir, i * dir_block_size);
            entry_cnt += (entry->accessed ? 1 : 0);
        }

        //pass tot_invs avg_inv_pages avg_wb_pages r_cnt w_cnt
        // fprintf(fstat, "%lu %lu %lf %lf %lu %lu %lu\n", pass, tot_invs,
        //         tot_invs == 0 ? 0 : tot_inv_pages / tot_invs,
        //         tot_invs == 0 ? 0 : tot_wb_pages / tot_invs,
        //         r_cnt, w_cnt, entry_cnt);
        // fflush(fstat);

        //print progress
        unsigned long max_time = 0;
        for (int i = 0; i < num_threads; ++i) {
            max_time = max(max_time, traces[i].time);
	    fflush(traces[i].PSO);
	    fprintf(traces[i].RWCNT, "%lu %lu %lu %lu\n",
			    traces[i].acc_cnt[LR], traces[i].acc_cnt[LW],
			    traces[i].acc_cnt[RR], traces[i].acc_cnt[RW]);
	    fflush(traces[i].RWCNT);
	}
        fprintf(progress, "Pass[%lu] Time[%lu] Inv(tot/pg/wb_pg)[%lu/%lu/%lu] R/W[%lu/%lu]\n", 
                pass, max_time, (unsigned long)(tot_invs), 
                (unsigned long)(tot_inv_pages), (unsigned long)(tot_wb_pages), 
                (unsigned long)(r_cnt), (unsigned long)(w_cnt));
        fflush(progress);

        //histogram per 100 timewindow
        if (pass % 100 == 0)
        {
            // histogram
            fprintf(fstat, "hist ");
            for (int j = 0; j <= dir_block_size / cache_line_size; ++j)
            {
                unsigned long inv_cnt = 0, wb_cnt = 0;
                for (int i = 0; i < num_nodes; ++i)
                {
                    struct cache_t *cache = caches[i];
                    inv_cnt += cache->inv_histogram[j];
                    wb_cnt += cache->wb_histogram[j];
                }
                fprintf(fstat, (j == dir_block_size / cache_line_size) ? "%lu %lu\n" : "%lu %lu ", inv_cnt, wb_cnt);
            }
            fflush(fstat);
        }

        // check end condition
        all_done = true;
        for (int i = 0; i < num_threads; ++i)
        {
            if (!traces[i].done)
                all_done = false;
        }
        if (pass >= MAX_PASS_COUNTER)
        {
            all_done = true;
        }
        //sync on the end of the time window
        __sync_synchronize();
        pthread_barrier_wait(&bar_next);
        if (all_done)
        {
            break;
        }
    }

    // print stats
    // invalidation per dir
    fprintf(fstat, "inval ");
    for (auto const& i : dir->entry_list)
    {
        fprintf(fstat, (i == dir->entry_list.back()) ? "%lu %lu\n" : "%lu %lu ",
                i->event_cnt[ASYNC_INV], i->event_cnt[ASYNC_INV_WB]);
    }
    fflush(fstat);

    for (int i = 0; i < num_threads; ++i)
    {
	    fclose(traces[i].PSO);
	    fclose(traces[i].RWCNT);
        traces[i].cache->terminate = 1;
        printf("Try to join thread [%d]\n", i);
        if (pthread_join(thread[i], NULL))
        {
            printf("Error joining thread %d\n", i);
            return 2;
        }
        close(load_args[i].fd);
#ifdef RECORD_CDF
        fclose(traces[i].cdf_fp);
#endif
    }

#ifdef VERIFY
    verify(traces);
#endif
    //#ifdef VERBOSE
    fclose(fp);
    fclose(fstat);
    //#endif
    //while(1)
    //	sleep(30);
    if (data_in_mem)
    {
        delete[] data_in_mem;
    }



    return 0;
}
