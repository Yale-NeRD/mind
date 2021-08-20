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

// divide a directory into two parts
static int divide_dir_entry(std::list<struct entry_t *>::iterator &iter, struct directory_t *dir)
{
    struct entry_t *entry = (*iter);
    size_t new_size = entry->dir_size >> 1;
    struct entry_t *new_entry = new entry_t;
    unsigned int retry_cnt = 0;
retry:
    pthread_mutex_lock(&dir->dir_lock);
    pthread_mutex_lock(&entry->mtx);
    if (entry->ack_cnt)
    {
        pthread_mutex_unlock(&entry->mtx);
        pthread_mutex_unlock(&dir->dir_lock);
        sleep(1);   // wait for the other threads
        retry_cnt ++;
        if (retry_cnt <= MAX_WAIT_INV)
            goto retry; 
        return -1;
    }
    // new entry
    new_entry->addr = entry->addr + (new_size);
    new_entry->dir_size = new_size;
    new_entry->val = &data_in_mem[new_entry->addr];
    new_entry->state = entry->state;
    new_entry->sharers = entry->sharers;
    // update hash list
    for (unsigned long off = 0; off < new_entry->dir_size; off += MIN_DIR_SIZE)
        dir->entry_hash[new_entry->addr + off] = new_entry;
    dir->entry_list.insert(++iter, new_entry);
    // existing entry
    entry->dir_size = new_size;
    // printf("[Dyn Cache] Dir splitted: 0x%lx & 0x%lx\n", entry->addr, new_entry->addr);
    pthread_mutex_unlock(&entry->mtx);
    pthread_mutex_unlock(&dir->dir_lock);
    return 0;
}

// this function will merge the directory at iter with the one at (iter - 1)
static int merge_dir_entry(std::list<struct entry_t *>::iterator &iter, struct directory_t *dir)
{
    struct entry_t *entry = *iter;
    struct entry_t *prev_entry = *(--iter);
    size_t new_size = entry->dir_size << 1;
    unsigned int retry_cnt = 0;
    std::list<struct entry_t *>::iterator prev_iter = iter;
    (++iter);   // to point current entry
retry:
    pthread_mutex_lock(&dir->dir_lock);
    pthread_mutex_lock(&prev_entry->mtx);
    pthread_mutex_lock(&entry->mtx);
    if (prev_entry->ack_cnt || entry->ack_cnt)
    {
        pthread_mutex_unlock(&entry->mtx);
        pthread_mutex_unlock(&prev_entry->mtx);
        pthread_mutex_unlock(&dir->dir_lock);
        sleep(1);   // wait for the other threads
        retry_cnt ++;
        if (retry_cnt <= MAX_WAIT_INV)
            goto retry; 
        return -1;
    }
    // new entry
    prev_entry->dir_size = new_size;
    // For shared -> shared
    if (entry->state == SHARED || prev_entry->state == SHARED)
    {
        prev_entry->state = SHARED;
        prev_entry->sharers |= entry->sharers;
    }
    // For modified -> modified
    else if (entry->state == MODIFIED)
    {
        prev_entry->state = MODIFIED;
        prev_entry->sharers = entry->sharers;
    }//then only possible cases are (prev=MODIFIED && entry=INVALID) or (INVALID && INVAILID)
    // update counters
    unsigned long prev_cnt = prev_entry->event_cnt[ASYNC_INV_WB];
    for (int i=0; i<NUM_EVENT; i++)
        prev_entry->event_cnt[i] += entry->event_cnt[i];
    // delete entry from list and hashmap
    for (unsigned long off = 0; off < entry->dir_size; off += MIN_DIR_SIZE)
        // dir->entry_hash.erase(entry->addr);
        dir->entry_hash[entry->addr + off] = prev_entry;    // redirect to the new entry
    dir->entry_list.erase(iter);
    // (--iter);   // to point prev entry
    iter = prev_iter;
    // if (entry->event_cnt[ASYNC_INV_WB])
    //     printf("[Dyn Cache] Dir merged: %lu + %lu -> %lu\n",
    //         prev_cnt, entry->event_cnt[ASYNC_INV_WB], prev_entry->event_cnt[ASYNC_INV_WB]);
    pthread_mutex_unlock(&entry->mtx);
    delete entry;
    pthread_mutex_unlock(&prev_entry->mtx);
    pthread_mutex_unlock(&dir->dir_lock);
    return 0;
}

static inline bool has_transient_lines(struct cache_t **caches, struct entry_t *entry)
{
    struct cache_line_t *line;
    for (int i = 0; i < num_nodes; i++)
    {
        for (unsigned long cur_addr = entry->addr; cur_addr < entry->addr + entry->dir_size;
             cur_addr += cache_line_size)
        {
            if (is_transient_state(caches[i]->lines[cur_addr/cache_line_size].state))
            {
                return true;
            }
        }
    }
    return false;
}

static inline bool is_mergable(struct entry_t *entry)
{
    return (entry->state <= MODIFIED) && (!entry->ack_cnt);
}

static inline bool is_mergable2(struct entry_t *entry, struct entry_t *prev_entry)
{
    return (entry->dir_size == prev_entry->dir_size) && (entry->state + prev_entry->state <= MODIFIED);  // I,I or S,I or S,S or I,M
}

// main function to manage cache block size
void do_cache_man(struct cache_t **caches, struct directory_t *dir, int num_nodes, int degree)
{
    // wait for ongoing inv
    for (int i = 0; i < num_nodes; i++)
    {
        while(!caches[i]->inv_data_queue.empty())   // || !caches[i]->fifo.empty())
        {
            sleep(1);
        }
    }
    // Iterate over directories to check counter
    unsigned long total_inv = 0, perf_tar = 0, dir_cnt = 0;
    for (auto const& it : dir->entry_list)
    {
        it->event_cnt[ASYNC_INV_WB_PREV] = it->event_cnt[ASYNC_INV_WB] - it->event_cnt[ASYNC_INV_WB_PREV];
        total_inv += it->event_cnt[ASYNC_INV_WB_PREV];
        dir_cnt ++;
    }
    perf_tar = max((unsigned long)1, total_inv / std::max(dir_cnt, (unsigned long)MAX_DIR_ENTRIES / 10) / target_per_coeff);
    // perf_tar = max((unsigned long)1, total_inv / (cache_size / MAX_DIR_SIZE) / target_per_coeff);
    printf("[Dyn Cache] Invalidated pages: %lu, dir: %lu, performance target: %lu\n",
           total_inv, dir_cnt, perf_tar);
    unsigned int split_cnt = 0, merge_cnt = 0;
    std::list<struct entry_t *>::iterator iter;
    for (iter = dir->entry_list.begin(); iter != dir->entry_list.end(); ++iter)
    {
        // Check invalidation counter and divide it into two blocks
        if ((*iter)->dir_size >= MIN_DIR_SIZE * (1 << degree) && (*iter)->event_cnt[ASYNC_INV_WB_PREV] > perf_tar)
        {
            if (dir_cnt < dir->max_dir_entries - 1)
            {
                if (!is_transient_state((*iter)->state) && !has_transient_lines(caches, (*iter))) // safe to split?
                {
                    // std::list<struct entry_t *>::iterator iter_first = iter;
                    // std::queue<std::list<struct entry_t *>::iterator> next_split;
                    // next_split.push(iter);
                    // for (int d = 0; d < degree; d++)
                    {
                        // int q_size = next_split.size();
                        // printf("[Dyn Cache] iter (before) 0x%lx\n", (*iter)->addr);
                        // for (int i = 0; i < q_size; i++)
                        {
                            // iter = next_split.front();
                            // next_split.pop();
                            if (!divide_dir_entry(iter, dir))
                            {
                                split_cnt ++;
                                dir_cnt ++;
                                // next_split.push(--iter);    // current
                                // next_split.push(++iter);    // new
                            }   // then go to next
                            // printf("[Dyn Cache] iter (after) 0x%lx\n", (*iter)->addr);
                        }
                    }
                }
            }
        }
        // Check invalidation counter and merge it into one
        else
        {
            size_t next_size = (*iter)->dir_size << degree;
            if (((next_size) <= MAX_DIR_SIZE) && is_mergable(*iter) &&
                ((*iter)->addr % next_size == 0)    // the first entry within this range (next_size)
                )
            {
                // std::queue<std::list<struct entry_t *>::iterator> next_merge;
                std::list<struct entry_t *>::iterator prev_iter = iter;
                int num_modified = 0;
                int num_shared = 0;
                unsigned long total_inv = 0;
                bool check_merge = false;   //true;
                // check merge-ability
		        prev_iter ++;
                // while (prev_iter != dir->entry_list.end() && (*prev_iter) && (*prev_iter)->addr < (*iter)->addr + next_size)
                if (prev_iter != dir->entry_list.end() && (*prev_iter) && (*prev_iter)->addr < (*iter)->addr + next_size)
                {
                    if (is_mergable(*prev_iter) && is_mergable2(*iter, *prev_iter) && !has_transient_lines(caches, (*prev_iter)))
                    {
                        // next_merge.push(prev_iter);
                        if ((*prev_iter)->state == MODIFIED)
                            num_modified ++;
                        else if ((*prev_iter)->state == SHARED)
                            num_shared ++;
                        total_inv += (*prev_iter)->event_cnt[ASYNC_INV_WB_PREV];
                        check_merge = true;
                    }else{
                        check_merge = false;
                        // break;
                    }
                    // ++ prev_iter;
                }
                if (check_merge && total_inv < perf_tar && ((num_modified == 0) || ((num_modified == 1) && (num_shared == 0))))   // only one modified is allowed
                {
                    // while (next_merge.size() > 1)
		            if (prev_iter != iter)
                    {
                        // prev_iter = next_merge.front();
                        // next_merge.pop();
                        // iter = next_merge.front();
                        // next_merge.pop();
                        // --prev_iter;
                        // if (((*iter)->dir_size == (*prev_iter)->dir_size) &&
                        //     is_mergable(*prev_iter) && is_mergable2(*iter, *prev_iter) &&
                        //     ((*prev_iter)->event_cnt[ASYNC_INV_WB_PREV] + 
                        //     (*iter)->event_cnt[ASYNC_INV_WB_PREV] < perf_tar))
                        {
                            if (!merge_dir_entry(prev_iter, dir))
                            {
                                merge_cnt ++;   // only for no error cases
                                dir_cnt --;
                            }
                        }
                        // next_merge.push(iter);
                    }
                }
            }
        }
    }
    printf("[Dyn Cache] Split entries: %u\n", split_cnt);
    printf("[Dyn Cache] Merged entries: %u\n", merge_cnt);
    for (auto const& it : dir->entry_list)
    {
        if (it->event_cnt[ASYNC_INV_WB])
            it->event_cnt[ASYNC_INV_WB_PREV] = it->event_cnt[ASYNC_INV_WB];
    }
    return;
}

static unsigned int log_2_from_min_dir(size_t x)
{
    unsigned int res = 0;
    x /= MIN_DIR_SIZE;
    while (x >>= 1)
        ++res;
    return res;
}

void print_directory_status(FILE *fstat, struct directory_t *dir)
{
    unsigned int max_level = log_2_from_min_dir(MAX_DIR_SIZE);
    unsigned long *dir_count = new unsigned long[max_level + 1];
    unsigned long *dir_inv_count = new unsigned long[max_level + 1];
    memset(dir_count, 0, sizeof(*dir_count) * (max_level + 1));
    memset(dir_inv_count, 0, sizeof(*dir_inv_count) * (max_level + 1));

    for (auto const& it : dir->entry_list)
    {
        // printf("DIR [%lu] -> [%u]\n", it->dir_size, log_2_from_min_dir(it->dir_size));
        int idx = log_2_from_min_dir(it->dir_size);
        dir_count[idx]++;
        dir_inv_count[idx] += it->event_cnt[ASYNC_INV_WB];
    }

    fprintf(fstat, "dir_entry ");
    for (int i=0; i<=max_level; i++)
    {
        fprintf(fstat, i == max_level ? "%lu\n" : "%lu ", dir_count[i]);
    }
    fprintf(fstat, "dir_inv ");
    for (int i=0; i<=max_level; i++)
    {
        fprintf(fstat, i == max_level ? "%lu\n" : "%lu ", dir_inv_count[i]);
    }
    fflush(fstat);
    delete [] dir_count;
    delete [] dir_inv_count;
}
