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
#include "simulator.hpp"

// == cdf accounting == //
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


static inline unsigned long calculate_dt(struct timeval *ts)
{
	unsigned long old_t = ts->tv_sec * 1000000 + ts->tv_usec;
	gettimeofday(ts, NULL);
	return ts->tv_sec * 1000000 + ts->tv_usec - old_t;

}

static inline void measure_time_start(struct timeval *ts)
{
#ifdef RECORD_CDF
	gettimeofday(ts, NULL);
#endif
}

static inline unsigned long measure_time_end(struct timeval *ts)
{
#ifdef RECORD_CDF
	return calculate_dt(ts);
#endif
}

static inline void record_time(struct trace_t *trace, unsigned long dt_op, int is_read)
{
#ifdef RECORD_CDF
	// if (trace->cdf_fp)
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
#ifdef RECORD_CDF
	if (trace->cdf_fp)
		fflush(trace->cdf_fp);
#endif
}

void print_cdf(struct trace_t *trace)
{
#ifdef RECORD_CDF
	int i = 0;
	if (trace && trace->cdf_fp)
	{
		fprintf(trace->cdf_fp, "Pass: %lu\n", trace->cur_pass);
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
#endif
}

// == simulation == //
struct entry_t *find_entry_by_addr(struct directory_t *dir, unsigned long addr) {
//printf("%lx %ld\n", addr, &dir->entries[addr/dir_block_size] - dir->entries);
    // return &dir->entries[addr/dir_block_size];
    for (size_t tar_size = MIN_DIR_SIZE; tar_size <= MAX_DIR_SIZE; tar_size <<= 1)
    {
        auto res = dir->entry_hash.find((size_t)(addr / tar_size) * tar_size);
        if (res != dir->entry_hash.end())
            return res->second;
    }
    printf("ERROR - Cannot find hash entry for 0x%lx\n", addr);
    return NULL;
}

int invalidate_one_cache_line(struct cache_t *cache, struct cache_line_t *line,
        unsigned long addr, uint8_t *buf, int type) {
    int ret = 0;

retry_inv_one:
    pthread_mutex_lock(&line->mtx);
#ifdef VERBOSE
    fprintf(fp, "INV %lx [%d] state %d\n", addr, cache->node_id, line->state);
    fflush(fp);
#endif
    //need more consideration on transient states here
    //deadlock for current implementation maybe...
    if (line->state == IM || line->state == IS) {
        pthread_mutex_unlock(&line->mtx);
        nanosleep(&timesleep, NULL);
        goto retry_inv_one;
    } else if (line->state == SM) {
        if (type == REQ_INV)
            line->state = IM;
        else {
            pthread_mutex_unlock(&line->mtx);
            nanosleep(&timesleep, NULL);
            goto retry_inv_one;
        }
    } else if (line->state == SI || line->state == MI) {
        if (line->state == MI) {
            ret = 1;
//stat
            ++cache->tot_wb_pages;
#ifdef VERIFY
            memcpy(buf, line->val, sizeof(uint8_t) * cache_line_size);
#endif
        }
        line->state = II;
        //remove from LRU here to distinguish 2 cases of II state
        pthread_mutex_lock(&cache->fifo_mtx);
        auto itr = cache->fifo_hash.find(addr/cache_line_size*cache_line_size);
//        if (itr == cache->fifo_hash.end())
//            printf("%lx\n", addr);
        assert(itr != cache->fifo_hash.end());
        cache->fifo.erase(itr->second);
        cache->fifo_hash.erase(itr);
        pthread_mutex_unlock(&cache->fifo_mtx);
    } else if (line->state == MODIFIED || line->state == SHARED) {
        if (line->state == MODIFIED) {
            ret = 1;
//stat
            ++cache->tot_wb_pages;
#ifdef VERIFY
            memcpy(buf, line->val, sizeof(uint8_t) * cache_line_size);
#endif
        }
        pthread_mutex_lock(&cache->fifo_mtx);
        auto itr = cache->fifo_hash.find(addr/cache_line_size*cache_line_size);
        assert(itr != cache->fifo_hash.end());
        cache->fifo.erase(itr->second);
        cache->fifo_hash.erase(itr);
        pthread_mutex_unlock(&cache->fifo_mtx);

        line->state = INVALID;
    } else if (line->state == INVALID || line->state == II) {
        /*
         * II is possible when a cache line is evicted but not actually used because the same
         * 2MB is evicted together
         */
        pthread_mutex_unlock(&line->mtx);
        // nanosleep(&timesleep, NULL);
        return ret;
    }
/*
#ifdef VERBOSE
    fprintf(fp, "INV %lx [%d]\n", addr, cache->node_id);
    fflush(fp);
#endif
*/
    pthread_mutex_unlock(&line->mtx);
//stat
    ++cache->tot_inv_pages;
    return ret;
}

int ack_one_cache_line(struct directory_t *dir, unsigned long addr,
                       uint8_t *buf, int writeback_data, int state, int sharers)
{
    //pthread_mutex_lock(&dir->dir_lock);
    //fprintf(fp, "dir lock %lx\n", addr);
    //fflush(fp);
    struct entry_t *entry = find_entry_by_addr(dir, addr);
    pthread_mutex_lock(&entry->mtx);
#ifdef VERBOSE
    fprintf(fp, "ACK %lx state %d writeback %d\n", addr, entry->state, writeback_data);
    fflush(fp);
#endif
    if ((entry->state == SI && !writeback_data) ||
        entry->state == MI)
    {
#ifdef VERIFY
        if (writeback_data)
            memcpy(entry->val + (addr % entry->dir_size), buf, sizeof(uint8_t) * cache_line_size);
#endif
        //entry->state = INVALID;
    } /*else if (entry->state == INVALID){
        //maybe have to allow this, state transition may have been performed by other computing blades
    } */
    else
    {
#ifdef VERBOSE
        fprintf(fp, "ack to unexpected entry %lx state %d data %d\n", addr, entry->state, writeback_data);
        fflush(fp);
#endif
    }
    --(entry->ack_cnt);
    if (entry->ack_cnt == 0)
    {
        entry->state = state;
        entry->sharers = sharers;
    }
    pthread_mutex_unlock(&entry->mtx);
    //pthread_mutex_unlock(&dir->dir_lock);
    //fprintf(fp, "dir unlock %lx\n", addr);
    //fflush(fp);
    return 0;
}

int pin_to_core(int core_id)
{
    // return 0;
    //core_id += 30;

    int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    if (core_id < 0 || core_id >= num_cores)
        return -1;

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    pthread_t current_thread = pthread_self();
    return pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
}

int send_evict_to_dir(struct cache_t *cache, struct directory_t *dir, unsigned long base_addr, unsigned len,
                      int *writeback_datas, uint8_t *vals, int node_id)
{
    struct entry_t *entry;

    pthread_mutex_lock(&cache->network_lock);
    pthread_mutex_lock(&dir->dir_lock);

    entry = find_entry_by_addr(dir, base_addr);
    pthread_mutex_lock(&entry->mtx);
#ifdef VERBOSE
    fprintf(fp, "E lock2 [%d] state %d [%lx %lx]\n", node_id, entry->state, base_addr, base_addr + len);
    fflush(fp);
#endif
    for (unsigned off = 0; off < len; off += cache_line_size, base_addr += cache_line_size)
    {
    retry_inv_entry:
        int writeback_data = writeback_datas[off / cache_line_size];
        if (writeback_data < 0)
            continue;

        if (entry->state == SHARED && !writeback_data)
        {
        }
        else if (entry->state == MODIFIED && writeback_data)
        {
#ifdef VERIFY
            memcpy(entry->val + off, vals + off, sizeof(uint8_t) * cache_line_size);
#endif
        }
        else if (entry->state == SI || entry->state == MI)
        {
            //printf("unexpected entry state %d\n", entry->state);
            pthread_mutex_unlock(&entry->mtx);
            goto retry_inv_entry;
        }
        else
        {
            //allow this when invalidation happens to the victim
            //printf("unexpected entry state %d writeback_data %d\n", entry->state, writeback_data);
            continue;
        }
    }
    entry->sharers &= ~(1 << node_id);
    if (entry->sharers == 0)
        entry->state = INVALID;
    pthread_mutex_unlock(&entry->mtx);

    pthread_mutex_unlock(&dir->dir_lock);
    pthread_mutex_unlock(&cache->network_lock);
    return 0;
}

int finally_evict_one_cache_line(struct cache_t *cache, struct cache_line_t *line, unsigned long addr)
{
    pthread_mutex_lock(&line->mtx);
#ifdef VERBOSE
    fprintf(fp, "E lock3 %lx [%d] state %d\n", addr, cache->node_id, line->state);
    fflush(fp);
#endif
    if (line->state == INVALID)
    {
        pthread_mutex_unlock(&line->mtx);
        return 0;
    }
    if (line->state != II)
    {
        pthread_mutex_lock(&cache->fifo_mtx);
        auto itr = cache->fifo_hash.find(addr / cache_line_size * cache_line_size);
        //        if (itr == cache->fifo_hash.end())
        //            printf("%lx\n", addr);
        assert(itr != cache->fifo_hash.end());
        cache->fifo.erase(itr->second);
        cache->fifo_hash.erase(itr);
        pthread_mutex_unlock(&cache->fifo_mtx);
    }
    line->state = INVALID;
    pthread_mutex_unlock(&line->mtx);
    return 0;
}

int prepare_evict_one_cache_line(struct cache_t *cache, struct cache_line_t *line, unsigned long addr, uint8_t *buf)
{
    int ret = 0;
retry_inv_one:
    pthread_mutex_lock(&line->mtx);
#ifdef VERBOSE
    fprintf(fp, "E lock1 %lx [%d] state %d\n", addr, cache->node_id, line->state);
    fflush(fp);
#endif
    //need more consideration on transient states here
    //deadlock for current implementation maybe...
    if (is_transient_state(line->state))
    {
        pthread_mutex_unlock(&line->mtx);
        goto retry_inv_one;
    }
    else if (line->state == MODIFIED)
    {
        line->state = MI;
        ret = 1;
#ifdef VERIFY
        memcpy(buf, line->val, sizeof(uint8_t) * cache_line_size);
#endif
    }
    else if (line->state == SHARED)
    {
        line->state = SI;
    }
    else if (line->state == INVALID)
    {
        ret = -1;
        line->state = II;
        /*
         * mark the cache line state to II when it is INVALID, 2 possible cases
         * 1.cache line is not used but evicted as a part of 2MB
         * 2.cache line has been invalidated by inv msg before eviction start
         */
    }
    pthread_mutex_unlock(&line->mtx);
    return ret;
}

int evict(struct cache_t *cache) {
    uint8_t *vals = new uint8_t[MAX_DIR_SIZE];
    int *writeback_datas = new int[MAX_DIR_SIZE/cache_line_size];
    struct cache_line_t *line;
    int cpu = sched_getcpu();
    //pin to core first
	pin_to_core(cpu);
    // pin_to_core(num_threads + cache->node_id);

    size_t cache_line_thres = cache->max_cache_lines * 8 / 10;
    printf("start evict routine, cpu[%d] cache line thres: %lu\n", cpu, cache_line_thres);
#ifdef VERBOSE
    unsigned long evict_cnt = 0;
#endif
    while (!cache->terminate) {
        if (cache->fifo.size() < cache_line_thres)
            continue;

        pthread_mutex_lock(&cache->fifo_mtx);
        line = cache->fifo.back();
        pthread_mutex_unlock(&cache->fifo_mtx);

        unsigned long addr = (line - cache->lines) * cache_line_size;
        struct entry_t *entry = find_entry_by_addr(cache->dir, addr);
        unsigned inv_range = entry->dir_size;
        unsigned long base_addr = addr / inv_range * inv_range;

        for (unsigned long off = 0, cur_addr = base_addr; off < inv_range;
                off += cache_line_size, cur_addr += cache_line_size) {
            line = &cache->lines[cur_addr/cache_line_size];
            writeback_datas[off/cache_line_size] = prepare_evict_one_cache_line(cache, line, cur_addr, vals + off);
        }

        simulate_network_latency();
        send_evict_to_dir(cache, cache->dir, base_addr, inv_range, writeback_datas, vals, cache->node_id);

        for (unsigned long off = 0, cur_addr = base_addr; off < inv_range;
                off += cache_line_size, cur_addr += cache_line_size) {
            //if (writeback_datas[off/cache_line_size] >= 0) {
                line = &cache->lines[cur_addr/cache_line_size];
                finally_evict_one_cache_line(cache, line, cur_addr);
            //}
        }
    }
    delete[] vals;
    delete[] writeback_datas;
    return 0;
}

int invalidate(struct cache_t *cache) {
    struct cache_line_t *line;
    unsigned long base_addr, addr;
    unsigned len;
    int type;
    int state_after_transition, sharers_after_transition;
    uint8_t *buf = new uint8_t[cache_line_size];
    int writeback_data;
    int cpu = sched_getcpu();

    printf("start invalidate routine cpu [%d]\n", cpu);
    //pin to core first
	pin_to_core(cpu);
    // pin_to_core(num_threads + num_nodes + cache->node_id);

    while (!cache->terminate) {
        if (cache->inv_data_queue.empty())
        {
            continue;
        }
        // recheck
        pthread_mutex_lock(&cache->q_lock);
        if (cache->inv_data_queue.empty()) {
            pthread_mutex_unlock(&cache->q_lock);
            nanosleep(&timesleep, NULL);
            continue;
        }
        struct inv_data_t *inv_data = cache->inv_data_queue.front();
        cache->inv_data_queue.pop_front();
        assert(inv_data->state == IDLE);
        pthread_mutex_unlock(&cache->q_lock);

        pthread_mutex_lock(&inv_data->inv_data_lock);
        //sync
        addr = inv_data->addr;
        len = inv_data->len;
        base_addr = addr / len * len;
        type = inv_data->type;
        state_after_transition = inv_data->state_after_transition;
        sharers_after_transition = inv_data->sharers_after_transition;
        line = &cache->lines[addr/cache_line_size];
        //stat
        unsigned long old_tot_inv_pages = cache->tot_inv_pages,
                      old_tot_wb_pages = cache->tot_wb_pages;
        invalidate_one_cache_line(cache, line, addr, inv_data->synced_data, type);
        inv_data->state = DONE_SYNC_WORK;
        pthread_mutex_unlock(&inv_data->inv_data_lock);

#ifdef VERBOSE
        fprintf(fp, "BATCH INV [%d] %lx[%lx %lx]\n", cache->node_id, addr, base_addr, base_addr + len);
        fflush(fp);
#endif

        //async
        unsigned long old_async_inv_pages = cache->tot_inv_pages,
                      old_async_wb_pages = cache->tot_wb_pages;
        for (unsigned off = 0; off < len; off += cache_line_size, base_addr += cache_line_size)
        {
            if (base_addr == addr)
                continue;
            line = &cache->lines[base_addr/cache_line_size];
            writeback_data = invalidate_one_cache_line(cache, line, base_addr, buf, type);
            ack_one_cache_line(cache->dir, base_addr, buf, writeback_data,
                    state_after_transition, sharers_after_transition);
        }

        //stat
        ++(cache->tot_invs);
        ++(cache->inv_histogram[min(dir_block_size / cache_line_size,
                                    cache->tot_inv_pages - old_tot_inv_pages)]);
        ++(cache->wb_histogram[min(dir_block_size / cache_line_size,
                                   cache->tot_wb_pages - old_tot_wb_pages)]);
        find_entry_by_addr(cache->dir, addr)->event_cnt[ASYNC_INV] += min(dir_block_size / cache_line_size,
                                                                          cache->tot_inv_pages - old_async_inv_pages);
        find_entry_by_addr(cache->dir, addr)->event_cnt[ASYNC_INV_WB] += min(dir_block_size / cache_line_size,
                                                                             cache->tot_wb_pages - old_async_wb_pages);
    }

    delete[] buf;

    return 0;
}

/*
 * this function simulate the behavior of directory's invalidation request to computing blades
 * the actual invalidation on computing blades is handled in function "invalidate"
 * returns whether current node is the only holder of entry
 */
int inv_sharers(struct directory_t *dir, struct entry_t *entry, int node_id, int sharers,
        unsigned long addr, unsigned len, uint8_t *val, int state, int new_sharers) {
    addr = addr / cache_line_size * cache_line_size;

    if ((sharers & ~(1 << node_id)) == 0)
        return 1;

    if (entry->state == SHARED)
        entry->state = SI;
    else if (entry->state == MODIFIED)
        entry->state = MI;
    else printf("unexpected entry state %d\n", entry->state);

    //calc ack cnt
    int num_sharers = 0;
    for (int sharer = 0; sharer < num_nodes; ++sharer)
        num_sharers += ((sharer != node_id && ((1 << sharer) & sharers)) ? 1 : 0);
    entry->ack_cnt = num_sharers * (len / cache_line_size - 1);

    //TODO spooling to avoid deadlock
    struct inv_data_t **inv_datas = new struct inv_data_t*[num_nodes];

    for (int sharer = 0; sharer < num_nodes; ++sharer) {
        if (!((1 << sharer) & sharers) || sharer == node_id)
            continue;
        struct cache_t *cache = caches[sharer];
        struct inv_data_t *inv_data = inv_datas[sharer] = new inv_data_t(cache_line_size);
#ifdef VERBOSE
        fprintf(fp, "send inv %lx to sharer %d\n", addr, sharer);
        fflush(fp);
#endif
        pthread_mutex_lock(&inv_data->inv_data_lock);
        assert(inv_data->state == IDLE);
        inv_data->addr = addr;
        inv_data->len = len;
        inv_data->type = (entry->state == MI ? INV : REQ_INV);
        inv_data->synced_data = val;
        inv_data->state_after_transition = state;
        inv_data->sharers_after_transition = new_sharers;
        pthread_mutex_unlock(&inv_data->inv_data_lock);

        pthread_mutex_lock(&cache->q_lock);
        cache->inv_data_queue.push_back(inv_data);
        pthread_mutex_unlock(&cache->q_lock);
    }

    //wait only for one cache line
    for (int sharer = 0; sharer < num_nodes; ++sharer) {
        if (!((1 << sharer) & sharers) || sharer == node_id)
            continue;
        struct inv_data_t *inv_data = inv_datas[sharer];
    retry_wait_sync_done:
        pthread_mutex_lock(&inv_data->inv_data_lock);
        if (inv_data->state == DONE_SYNC_WORK) {
            delete inv_data;
        } else {
            pthread_mutex_unlock(&inv_data->inv_data_lock);
            nanosleep(&timesleep, NULL);
            goto retry_wait_sync_done;
        }
    }

    delete[] inv_datas;
    return 0;
}

int dir_read(struct directory_t *dir, int node_id, unsigned long addr, void *buf) {
    int ret = ACK;
    addr = addr / cache_line_size * cache_line_size;

    //pthread_mutex_lock(&dir->dir_lock);
    int err;
    if (err = pthread_mutex_timedlock(&dir->dir_lock, &timeout)) {
        assert(err == ETIMEDOUT);
#ifdef VERBOSE
        fprintf(fp, "timeout detected %lx\n", addr);
        fflush(fp);
#endif
        return NACK;
    }

//fprintf(fp, "dir time lock r %lx\n", addr);
//fflush(fp);

    struct entry_t *entry = find_entry_by_addr(dir, addr);
    pthread_mutex_lock(&entry->mtx);
#ifdef VERBOSE
    fprintf(fp, "DR [%d] %lx %d sharer %x\n", node_id, addr, entry->state, entry->sharers);
    fflush(fp);
#endif
    if (entry->state == INVALID || entry->state == SHARED) {
#ifdef VERIFY
        memcpy(buf, entry->val + (addr%entry->dir_size), sizeof(uint8_t) * cache_line_size);
#endif
        entry->state = SHARED;
        if ((entry->sharers & (1 << node_id)))
        {
            // accounting
            entry->event_cnt[ANY_RA]++;
        }
        entry->sharers |= (1 << node_id);   // only when it was not in the list?
        simulate_dir_fetch_latency();
    } else if (entry->state == MODIFIED) {
        // unsigned inv_range = dir_block_size;
        unsigned inv_range = entry->dir_size;
        bool owner_is_self = inv_sharers(dir, entry, node_id, entry->sharers,
                addr, inv_range, entry->val + (addr%inv_range),
                SHARED, 1 << node_id);
#ifdef VERIFY
        memcpy(buf, entry->val + (addr%inv_range), sizeof(uint8_t) * cache_line_size);
#endif
        if (!owner_is_self && cache_line_size == inv_range) {
            entry->state = SHARED;
            entry->sharers = (1 << node_id);
        }

        // accounting
        if (!owner_is_self)
        {
            entry->event_cnt[ANY_RA]++;
        }
    } else {
        ret = NACK;
        goto out;
    }
out:
    pthread_mutex_unlock(&entry->mtx);
    pthread_mutex_unlock(&dir->dir_lock);
//fprintf(fp, "dir time unlock r %lx\n", addr);
//fflush(fp);
    simulate_network_latency();
    return ret;
}

int read_cache(struct trace_t *trace, struct cache_t *cache, unsigned long addr) {
    int ret;
    int acc_type = -1;
    uint8_t *buf = NULL;
    struct cache_line_t *line = &cache->lines[addr/cache_line_size];

    //stat
    struct entry_t *entry = find_entry_by_addr(cache->dir, addr);
    if (!entry->accessed)
        entry->accessed = true;

retry_read_cache:
    pthread_mutex_lock(&line->mtx);
    if (line->state == INVALID) {
	    acc_type = RR;
#ifdef VERBOSE
        fprintf(fp, "R lock1 %lx [%d] state %d\n", addr, cache->node_id, line->state);
        fflush(fp);
#endif
        //while (cache->fifo.size() >= cache->max_cache_lines);

        line->state = IS;
        pthread_mutex_unlock(&line->mtx);

        //pthread_mutex_lock(&cache->network_lock);
//fprintf(fp, "network lock before dir read [%d]\n", cache->node_id);
//fflush(fp);
        simulate_network_latency();
#ifdef VERIFY
        buf = new uint8_t[cache_line_size];
#endif
        ret = dir_read(cache->dir, cache->node_id, addr, buf);
//fprintf(fp, "network lock after dir read [%d]\n", cache->node_id);
//fflush(fp);
        //pthread_mutex_unlock(&cache->network_lock);

        //if NACK, abort the read/write from very beginning
        //case 1: entry is in transition state(async invalidation)
        //case 2: potential dead lock
        if (ret == NACK) {
#ifdef VERBOSE
            fprintf(fp, "IS abort %lx [%d]\n", addr, cache->node_id);
            fflush(fp);
#endif
            pthread_mutex_lock(&line->mtx);
            assert(line->state == IS);
            line->state = INVALID;
            pthread_mutex_unlock(&line->mtx);
#ifdef VERIFY
            delete[] buf;
#endif
            nanosleep(&timesleep, NULL);
            // std::this_thread::sleep_for(std::chrono::nanoseconds(10));
            goto retry_read_cache;
        }

        pthread_mutex_lock(&line->mtx);
        line->state = SHARED;
#ifdef VERBOSE
        fprintf(fp, "R lock2 %lx [%d] state %d\n", addr, cache->node_id, line->state);
        fflush(fp);
#endif
#ifdef VERIFY
        memcpy(line->val, buf, sizeof(uint8_t) * cache_line_size);
        delete[] buf;
#endif
        unsigned long aligned_addr = addr / cache_line_size * cache_line_size;
        pthread_mutex_lock(&cache->fifo_mtx);
        auto itr = cache->fifo_hash.find(aligned_addr);
        if (itr == cache->fifo_hash.end()) {
            cache->fifo.push_front(line);
            cache->fifo_hash[aligned_addr] = cache->fifo.begin();
//printf("P %lx\n", addr/cache_line_size*cache_line_size);
        }
        pthread_mutex_unlock(&cache->fifo_mtx);
        pthread_mutex_unlock(&line->mtx);
        goto retry_read_cache;
    } else if (state_can_read(line->state)) {
	if (acc_type < 0)
		acc_type = LR;
#ifdef VERIFY
        trace->read_vals[trace->read_tail++] = line->val[addr % cache_line_size];
#endif
#ifdef RWVERBOSE
        fprintf(fp, "R %lx %hhu [%d %lu]\n", addr, line->val[addr % cache_line_size], trace->tid, trace->read_tail - 1);
        fflush(fp);
#endif
        //stat
        ++trace->r_cnt;
        /*
        if ((trace->r_cnt + trace->w_cnt) % 100000 == 0) {
            fprintf(fp, "r_cnt[%lu] w_cnt[%lu]\n", trace->r_cnt, trace->w_cnt);
            fflush(fp);
        }
        */

        simulate_read_cache_latency();
        pthread_mutex_unlock(&line->mtx);
    } 
    // else if (is_transient_state(line->state)) {
    //     pthread_mutex_unlock(&line->mtx);
    //     goto retry_read_cache;
    // } 
    else {
        pthread_mutex_unlock(&line->mtx);
        nanosleep(&timesleep, NULL);
        // std::this_thread::sleep_for(std::chrono::nanoseconds(10));
        goto retry_read_cache;
    }
    
    auto &acc_cnt = trace->acc_cnt;
    auto &wbuf = trace->wbuf;
    //if (acc_type == LR) {
	    unsigned long aligned_addr = addr / cache_line_size * cache_line_size;
	    auto itr = wbuf.find(aligned_addr);
	    if (itr != wbuf.end()) {
		auto &old_acc_cnt = wbuf[aligned_addr];
	    	fprintf(trace->PSO, "%lu %lu %lu %lu\n", acc_cnt[LR]-old_acc_cnt[LR], acc_cnt[LW]-old_acc_cnt[LW],
				acc_cnt[RR]-old_acc_cnt[RR], acc_cnt[RW]-old_acc_cnt[RW]);
		wbuf.erase(itr);
	    }
    //}
    ++trace->acc_cnt[acc_type];

    return 0;
}

int dir_write(struct directory_t *dir, int node_id, unsigned long addr, void *buf) {
    int ret = ACK;
    addr = addr / cache_line_size * cache_line_size;

    //pthread_mutex_lock(&dir->dir_lock);
    int err;
    if (err = pthread_mutex_timedlock(&dir->dir_lock, &timeout)) {
        assert(err == ETIMEDOUT);
#ifdef VERBOSE
        fprintf(fp, "timeout detected %lx\n", addr);
        fflush(fp);
#endif
        return NACK;
    }

//fprintf(fp, "dir time lock w %lx\n", addr);
//fflush(fp);

    struct entry_t *entry = find_entry_by_addr(dir, addr);
    pthread_mutex_lock(&entry->mtx);
#ifdef VERBOSE
    fprintf(fp, "DW [%d] %lx %d sharer %x\n", node_id, addr, entry->state, entry->sharers);
    fflush(fp);
#endif
    if (entry->state == INVALID) {
#ifdef VERIFY
        memcpy(buf, entry->val + (addr%entry->dir_size), sizeof(uint8_t) * cache_line_size);
#endif
        entry->state = MODIFIED;
        entry->sharers = 1 << node_id;
        simulate_dir_fetch_latency();

        // accounting
        entry->event_cnt[ANY_RA]++;
    } else if (entry->state == SHARED || entry->state == MODIFIED){
        unsigned inv_range = entry->dir_size;
        bool owner_is_self = inv_sharers(dir, entry, node_id, entry->sharers,
                addr, inv_range, entry->val + (addr%inv_range),
                MODIFIED, 1 << node_id);
#ifdef VERIFY
        memcpy(buf, entry->val + (addr%inv_range), sizeof(uint8_t) * cache_line_size);
#endif
        if (owner_is_self || cache_line_size == inv_range) {
            entry->state = MODIFIED;
            entry->sharers = 1 << node_id;
        }

        // accounting
        if (!owner_is_self)
        {
            entry->event_cnt[ANY_RA]++;
        }
    } else {
        ret = NACK;
        goto out;
    }
out:
    pthread_mutex_unlock(&entry->mtx);
    pthread_mutex_unlock(&dir->dir_lock);
//fprintf(fp, "dir time unlock w %lx\n", addr);
//fflush(fp);
    simulate_network_latency();
    return ret;
}

int write_cache(struct trace_t *trace, struct cache_t *cache, unsigned long addr) {
    int ret;
    int acc_type = -1;
    uint8_t *buf = NULL;
    struct cache_line_t *line = &cache->lines[addr/cache_line_size];

    //stat
    struct entry_t *entry = find_entry_by_addr(cache->dir, addr);
    if (!entry->accessed)
        entry->accessed = true;

retry_write_cache:
    pthread_mutex_lock(&line->mtx);
    if (line->state == INVALID || line->state == SHARED) {
	    acc_type = RW;
#ifdef VERBOSE
        fprintf(fp, "W lock1 %lx [%d] state %d\n", addr, cache->node_id, line->state);
        fflush(fp);
#endif
        if (line->state == INVALID) {
            //while (cache->fifo.size() >= cache->max_cache_lines);
            line->state = IM;
        } else {
            line->state = SM;
        }
        pthread_mutex_unlock(&line->mtx);

        //pthread_mutex_lock(&cache->network_lock);
//fprintf(fp, "network lock before dir write [%d]\n", cache->node_id);
//fflush(fp);
        simulate_network_latency();
#ifdef VERIFY
        buf = new uint8_t[cache_line_size];
#endif
        ret = dir_write(cache->dir, cache->node_id, addr, buf);
//fprintf(fp, "network unlock after dir write [%d]\n", cache->node_id);
//fflush(fp);
        //pthread_mutex_unlock(&cache->network_lock);

        //if NACK, abort the read/write from very beginning
        //case 1: entry is in transition state(async invalidation)
        //case 2: potential dead lock
        if (ret == NACK) {
            pthread_mutex_lock(&line->mtx);
            assert(line->state == IM || line->state == SM);
            if (line->state == IM) {
#ifdef VERBOSE
                fprintf(fp, "IM abort %lx [%d]\n", addr, cache->node_id);
                fflush(fp);
#endif
                line->state = INVALID;
            } else if (line->state == SM) {
#ifdef VERBOSE
                fprintf(fp, "SM abort %lx [%d]\n", addr, cache->node_id);
                fflush(fp);
#endif
                line->state = SHARED;
            }
            pthread_mutex_unlock(&line->mtx);
#ifdef VERIFY
            delete[] buf;
#endif
            nanosleep(&timesleep, NULL);
            // std::this_thread::sleep_for(std::chrono::nanoseconds(10));
            goto retry_write_cache;
        }

        pthread_mutex_lock(&line->mtx);
        line->state = MODIFIED;
#ifdef VERBOSE
        fprintf(fp, "W lock2 %lx [%d] state %d\n", addr, cache->node_id, line->state);
        fflush(fp);
#endif
#ifdef VERIFY
        memcpy(line->val, buf, sizeof(uint8_t) * cache_line_size);
        delete[] buf;
#endif
        unsigned long aligned_addr = addr / cache_line_size * cache_line_size;
        pthread_mutex_lock(&cache->fifo_mtx);
        auto itr = cache->fifo_hash.find(aligned_addr);
        if (itr == cache->fifo_hash.end()) {
            cache->fifo.push_front(line);
            cache->fifo_hash[aligned_addr] = cache->fifo.begin();
//printf("P %lx\n", addr/cache_line_size*cache_line_size);
        }
        pthread_mutex_unlock(&cache->fifo_mtx);
        pthread_mutex_unlock(&line->mtx);
        goto retry_write_cache;
    } else if (state_can_write(line->state)) {
	    if (acc_type < 0)
		    acc_type = LW;
#ifdef VERIFY
        line->val[addr % cache_line_size] = trace->write_vals[trace->write_tail++];
#endif
#ifdef RWVERBOSE
        fprintf(fp, "W %lx %hhu [%d %lu]\n", addr, line->val[addr % cache_line_size], trace->tid, trace->write_tail - 1);
        fflush(fp);
#endif
        /*
        if ((trace->r_cnt + trace->w_cnt) % 100000 == 0) {
            fprintf(fp, "r_cnt[%lu] w_cnt[%lu]\n", trace->r_cnt, trace->w_cnt);
            fflush(fp);
        }
        */
        ++trace->w_cnt;
        simulate_write_cache_latency();
        pthread_mutex_unlock(&line->mtx);
    } else if (is_transient_state(line->state)) {
        pthread_mutex_unlock(&line->mtx);
        goto retry_write_cache;
    }
	
    auto &acc_cnt = trace->acc_cnt;
    auto &wbuf = trace->wbuf;
    if (acc_type == RW) {
	    unsigned long aligned_addr = addr / cache_line_size * cache_line_size;
	    wbuf.insert(make_pair(aligned_addr, acc_cnt));
    }
    ++trace->acc_cnt[acc_type];

    return 0;
}

void do_log(void *arg)
{
	struct trace_t *trace = (struct trace_t*) arg;

	//multimap<unsigned int, void *> len2addr;
	unsigned long old_ts = 0, dt_op = 0;
	unsigned long i = 0;

    //while (!ready(trace->tid));

	struct timeval ts, ts_op;
	gettimeofday(&ts, NULL);
	char *cur;
	for (i = 0; i < trace->len ; ++i) {
		volatile char op = trace->logs[i * sizeof(RWlog)];
		cur = &(trace->logs[i * sizeof(RWlog)]);
		if (op == 'R') {
			struct RWlog *log = (struct RWlog *)cur;
			//interval_between_access(log->usec - old_ts);
			unsigned long addr = log->addr & MMAP_ADDR_MASK;
//printf("before R %lu\n", addr);
            measure_time_start(&ts_op);
            read_cache(trace, trace->cache, addr);
            dt_op = measure_time_end(&ts_op);
			record_time(trace, dt_op, 1);
//printf("after R %lu\n", addr);

			old_ts = log->usec;
		} else if (op == 'W') {
			struct RWlog *log = (struct RWlog *)cur;
		    //interval_between_access(log->usec - old_ts);
			unsigned long addr = log->addr & MMAP_ADDR_MASK;
//printf("before W %lu\n", addr);
            measure_time_start(&ts_op);
            write_cache(trace, trace->cache, addr);
            dt_op = measure_time_end(&ts_op);
			record_time(trace, dt_op, 0);
//printf("after W %lu\n", addr);
			old_ts = log->usec;
		} else if (op == 'M') {
			//struct Mlog *log = (struct Mlog *)cur;
			//interval_between_access(log->hdr.usec);
			//void *ret_addr = mmap((void *)(log->start & MMAP_ADDR_MASK), log->len, PROT_READ|PROT_WRITE, TEST_ALLOC_FLAG, -1, 0);
			//unsigned int len = log->len;
			//len2addr.insert(pair<unsigned int, void *>(len, ret_addr));
			//old_ts += log->hdr.usec;
		} else if (op == 'B') {
			//struct Blog *log = (struct Blog *)cur;
			//interval_between_access(log->usec - old_ts);
			//brk((void *)(log->addr & MMAP_ADDR_MASK));
			//old_ts = log->usec;
		} else if (op == 'U') {
			//struct Ulog *log = (struct Ulog *)cur;
			//interval_between_access(log->hdr.usec);
			//multimap<unsigned int, void *>::iterator itr = len2addr.find(log->len);
			//if (itr == len2addr.end())
//				printf("no mapping to unmap\n");
			//	;
			//else {
			//	munmap(itr->second, log->len);
			//	len2addr.erase(itr);
			//}
			//old_ts += log->hdr.usec;
		} else {
			printf("unexpected log: %c at line: %lu\n", op, i);
		}
		//if (i % 1000000 == 0)
		//	printf("%lu\n", i);
	}

	unsigned long old_t = ts.tv_sec * 1000000 + ts.tv_usec;
	gettimeofday(&ts, NULL);
	unsigned long dt = ts.tv_sec * 1000000 + ts.tv_usec - old_t;
	trace->time += dt;

    if (trace->is_main_thread)
	    printf("[%lu] done in %lu us, total run time: %lu us\n", trace->cur_pass, dt, trace->time);
	//for mmap log loading
	//munmap(trace->logs, trace->len * sizeof(RWlog));
}

int load_log(void *void_arg) {
	struct load_arg_t *load_arg = (struct load_arg_t *)void_arg;
	int fd = load_arg->fd;
	struct trace_t *arg = load_arg->arg;
	unsigned long ts_limit = load_arg->ts_limit;
    unsigned long rw_cnt = 0;

	// printf("ts_limit: %lu, offset: %lu\n", ts_limit, arg->offset);
	assert(sizeof(RWlog) == sizeof(Mlog));
	assert(sizeof(RWlog) == sizeof(Blog));
	assert(sizeof(RWlog) == sizeof(Ulog));

	if (arg->logs) {
	//	printf("munmap %p\n", arg->logs);
		munmap(arg->logs, LOG_NUM_TOTAL * sizeof(RWlog));
	}
	arg->logs = (char *)mmap(NULL, LOG_NUM_TOTAL * sizeof(RWlog), PROT_READ, MAP_PRIVATE, fd, arg->offset);
	//printf("arg->logs: %p, fd: %d\n", arg->logs, fd);

	unsigned long new_offset = 0;
	//walk through logs to find the end of timewindow also trigger demand paging
	for (char *cur = arg->logs; cur < arg->logs + LOG_NUM_TOTAL * sizeof(RWlog); cur += sizeof(RWlog)) {
		if (*cur == 'R' || *cur == 'W' || *cur == 'B') {
            //printf("%c %lx\n", *cur, ((struct RWlog *)cur)->addr & MMAP_ADDR_MASK);
            ++rw_cnt;
			if ((((struct RWlog *)cur)->usec >= ts_limit || (rw_cnt >= LOG_NUM_ONCE)) && !new_offset)
            {
				new_offset = (arg->offset + (cur - arg->logs)) / LOG_MAP_ALIGN * LOG_MAP_ALIGN;
                if (new_offset)
                    break;
            }
		} else if (*cur == 'M' || *cur == 'U') {
			continue;
		} else {
            printf("unexpected char %c (maybe end of file)\n", *cur);
			new_offset = (arg->offset + (cur - sizeof(RWlog) - arg->logs)) / LOG_MAP_ALIGN * LOG_MAP_ALIGN;
			arg->done = true;
            break;
		}
	}
	//if offset is the same as the old one due to align
	if (arg->offset != new_offset)
		assert(new_offset);
//printf("new_offset: %lu\n", new_offset);
	arg->len = (new_offset - arg->offset) / sizeof(RWlog);
	arg->offset = new_offset;
	// printf("finish loading %lu logs, %lu Read/Write\n", arg->len, rw_cnt);

	return 0;
}

#ifdef VERIFY
    int verify(struct trace_t *traces) {
        for (int tid = 0; tid < num_threads; ++tid) {
            int fd = open((string("./test_logs/random_") + to_string(tid) + string("_0.res")).c_str(), O_RDONLY);
            uint8_t *gold_vals = new uint8_t[MAX_NUM_READS_TO_VERIFY];
            read(fd, gold_vals, MAX_NUM_READS_TO_VERIFY);
            struct trace_t *trace = &traces[tid];
            for (unsigned long i = 0; i < trace->read_tail; ++i) {
                uint8_t gold_val = gold_vals[i];
                uint8_t get_val = trace->read_vals[i];
                if (get_val != gold_val) {
                    printf("tid[%d] rid[%lu] expect[%hhu] get[%hhu]\n", tid, i, gold_val, get_val);
                    printf("test failed\n");
                    return 1;
                }
            }
            close(fd);
            delete[] gold_vals;
        }
        printf("test passed\n");
        return 0;
    }
#endif
