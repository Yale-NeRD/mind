#include "controller.h"
#include "cacheline_manager.h"
#include "memory_management.h"
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>	//close
#include <fcntl.h>  //socket configuration
#include <arpa/inet.h>
#include <sys/time.h>
#include "cacheline_def.h"
#include "cache_manager_thread.h"
#include "cacheline_manager.h"

static pthread_spinlock_t cacheman_usedlist_lock, cacheman_lock, cacheman_recovery_lock;
static int unlock_requested = 0;

void cache_man_init(void)
{
    pthread_spin_init(&cacheman_usedlist_lock, PTHREAD_PROCESS_PRIVATE);
    pthread_spin_init(&cacheman_lock, PTHREAD_PROCESS_PRIVATE);
    pthread_spin_init(&cacheman_recovery_lock, PTHREAD_PROCESS_PRIVATE);
}

void cacheman_run_lock()
{
    pthread_spin_lock(&cacheman_lock);
}

void cacheman_run_unlock()
{
    pthread_spin_unlock(&cacheman_lock);
}

void cacheman_request_unlock()
{
    unlock_requested = 1;
}

void cacheman_recover_lock()
{
    pthread_spin_lock(&cacheman_recovery_lock);
}

void cacheman_recover_unlock()
{
    pthread_spin_unlock(&cacheman_recovery_lock);
}


static void clear_cache_dir_no_lock(u64 fva)
{
    hlist *hash_node;
    hash_node = cacheline_get_node(fva);

    if (hash_node)
    {
        struct cacheline_dir *dir_ptr = (struct cacheline_dir *)hash_node->value;
        u16 shift = dir_ptr->dir_size + REGION_SIZE_BASE;
        u64 aligned_fva = get_aligned_fva(fva, shift);
        int c_idx = dir_ptr->idx;
        // Delete tcam and register
        pr_cache("Cacheline found - PID+VA: 0x%lx => Idx: %d\n", aligned_fva, c_idx);
        bfrt_del_cacheline(aligned_fva, 64 - shift);
        // Delete from hash list
        delete_from_hash_list(fva, hash_node);
        // Add back to free list
        if (dir_ptr->node_ptr)
        {
            delete_from_used_list(dir_ptr->node_ptr);
        }
        add_to_free_list(dir_ptr);
        check_and_print_cacheline(aligned_fva, c_idx);
    }else{
        // pr_cache("Cacheline not found - FVA: 0x%lx \n", fva);
    }
}

void try_clear_cache_dir(u64 fva)
{
    pthread_spin_lock(&cacheman_usedlist_lock);
    clear_cache_dir_no_lock(fva);
    pthread_spin_unlock(&cacheman_usedlist_lock);
}

inline uint64_t size_index_to_size(uint16_t s_idx)
{
    return ((uint64_t)DYN_MIN_DIR_SIZE) << s_idx;
}

inline static void unlock_cacheline(u32 dir_idx, uint16_t dir_size, int is_split, int is_merge)
{
    uint16_t state, st_update, dir_lock = CACHELINE_LOCKED, tmp_state, tmp_update;
    unsigned int cnt = 0;
    
    // TODO: check whether the directory entry exists or not from the hash lish
    // Check the state first
    bfrt_get_cacheline_reg_state(dir_idx, &state, &st_update);
    barrier();
    while (dir_lock != CACHELINE_UNLOCKED)
    {
        if (cnt % 100 == 0) // 100 ms
        {
            bfrt_set_cacheline_lock(dir_idx, CACHELINE_UNLOCKED);
            barrier();
        }
        usleep(1000);
        bfrt_get_cacheline_reg_lock(dir_idx, &dir_lock);
        barrier();
        if ((dir_lock != CACHELINE_UNLOCKED) && (cnt % 500 == 0))
        {
            pr_dyn_cache("!!! Couldn't be unlocked - Reg[%u]: state [0x%x] update [%u] lock [%u] size [0x%x] || split/merge[%d/%d]\n",
                        (unsigned int)dir_idx, state, (unsigned int)st_update, dir_lock, dir_size, is_split, is_merge);
        }
        cnt ++;
    }
    if (st_update)
    {
        pr_dyn_cache("WARN: under update - Reg[%u]: state [0x%x] update [%u] lock [%u] size [0x%x] || split/merge[%d/%d]\n",
                            (unsigned int)dir_idx, state, (unsigned int)st_update, dir_lock, dir_size, is_split, is_merge);
        bfrt_set_cacheline_state(dir_idx, state);
        barrier();
        tmp_state = CACHELINE_FAIL;
        tmp_update = 1;

        while (tmp_update)
        {
            usleep(1000);
            bfrt_get_cacheline_reg_state(dir_idx, &tmp_state, &tmp_update);  // recheck update status
            barrier();
        }
    }
}

inline static int check_mergeable(uint16_t state, uint16_t prev_state, uint16_t sharer, uint16_t prev_sharer)
{
    if ((state != CACHELINE_IDLE) && (state != CACHELINE_SHARED) && (state != CACHELINE_MODIFIED))
        return 0;
    if ((prev_state != CACHELINE_IDLE) && (prev_state != CACHELINE_SHARED) && (prev_state != CACHELINE_MODIFIED))
        return 0;
    return (state == CACHELINE_IDLE) || (prev_state == CACHELINE_IDLE) || ((state == CACHELINE_SHARED) && (prev_state == CACHELINE_SHARED)) || (sharer == prev_sharer);
}

void run_cache_size_thread(struct server_service *cache_man)
{
    struct list_node *node;
    unsigned long total_inv = 0, inv_threshold, entries=0;
    struct cacheline_dir *dir, *prev_dir;
    uint16_t state, st_update, sharer, dir_size, dir_lock, prev_state, prev_st_update, prev_sharer;
    uint32_t inv_cnt;
    int node_updated = 0;
    int try_split = 0, try_merge = 0;
    uint64_t adjacent_fva;
    hlist *prev_hnode;
    char time_buf[32];
    struct timeval t_before, t_after, t_last_update;
    struct timeval t_search_before, t_search_after;
    struct timeval t_lock_before, t_lock_after;
    long total_lock_time;
    unsigned long time_epoch = 0;

    (void)cache_man;    // make compiler happy
    gettimeofday(&t_last_update, NULL);
    while(run_thread)
    {
#ifdef DYN_CACHE_DISABLE_MANAGER
        sleep(1000);
        continue;
#endif
        unsigned long split_cnt = 0, merge_cnt = 0;
        int free_dir_cnt = get_free_dir_cnt();
        int is_high_pressure = 0;
        gettimeofday(&t_search_before, NULL);
        total_lock_time = 0;
        
        // Acquire cacheman lock in order to prevent modification/deletion of used list from other thread
        cacheman_run_lock();
        pthread_spin_lock(&cacheman_usedlist_lock);
        node = get_first_used_node();
        // 1a) walk through the used list to count invalidation
        total_inv = 0;
        entries = 0;
        is_high_pressure = (free_dir_cnt < (int)(DYN_CACHE_TAR_DIR * DYN_CACHE_HIGH_PRESURE_FREE));
        while (node && !was_last_usedlist_node(node) 
               && (!unlock_requested || is_high_pressure))  // under high pressure, we need to merge entries anyway
        {
            dir = node->data;
            bfrt_get_cacheline_reg_inv(dir->idx, &inv_cnt);
            dir->inv_cnt = inv_cnt;
            total_inv += inv_cnt;
            entries ++;

            // for next iteration
            lock_usedlist();
            node = node->next;
            unlock_usedlist();
        }

        // Calculate the threshold: coefficient * #total inv / #total cache line
        inv_threshold = max(is_high_pressure ? 1 : 0, 
                            // if utilization is over X %, allow merging idle directories
                            total_inv / (unsigned long)(DYN_CACHE_TAR_DIR / REGION_SIZE_TOTAL) / DYN_CACHE_COEFF);
        gettimeofday(&t_search_after, NULL);
        get_timestamp(time_buf, 32);

        // Main spilt / merge loop
        gettimeofday(&t_before, NULL);
        node = get_first_used_node();
        while (node && !was_last_usedlist_node(node) && ((split_cnt + merge_cnt) < MAX_SPLIT_MERGE_PER_EPOCH) && !unlock_requested)
        {
            try_split = try_merge = 0;
            node_updated = 0;
            adjacent_fva = 0;
            prev_hnode = NULL;
            prev_dir = NULL;
            dir = node->data;
            if (dir)
            {
                adjacent_fva = get_aligned_fva(dir->fva, dir->dir_size + REGION_SIZE_BASE);
                adjacent_fva += size_index_to_size(dir->dir_size);
                prev_hnode = cacheline_get_node(adjacent_fva);
                // We need at least 3, because we need at least one empty slot to merge unused entries in the future
                if ((free_dir_cnt > 3) && (dir->inv_cnt > inv_threshold) && (dir->dir_size > REGION_SIZE_4KB))
                {
                    do 
                    {
#ifdef DYN_CACHE_DISABLE_SPLIT
#ifdef DYN_CACHE_DISABLE_LOCK
                        break;
#endif
#endif
                        bfrt_get_cacheline_reg_state(dir->idx, &state, &st_update);
                        if (!st_update && ((state == CACHELINE_SHARED) || (state == CACHELINE_MODIFIED) || (state == CACHELINE_IDLE)))
                            try_split = 1;
                    }while(0);
                }
                // We need at least 1 for merge
                else if(free_dir_cnt >= 1 && (dir->dir_size < REGION_SIZE_2MB) && ((dir->fva % (size_index_to_size(dir->dir_size + 1))) == 0))
                {
                    do 
                    {
#ifdef DYN_CACHE_DISABLE_MERGE
#ifdef DYN_CACHE_DISABLE_LOCK
                            break;
#endif
#endif
                        if (prev_hnode && prev_hnode->value)
                        {
                            prev_dir = prev_hnode->value;
                            if ((prev_dir->dir_size == dir->dir_size))
                            {
                                if (is_high_pressure || (inv_threshold >= DYN_CACHE_LOW_PRESURE_INV_THRESHOLD))
                                {
                                    if (prev_dir->inv_cnt + dir->inv_cnt < inv_threshold)
                                    {
                                        bfrt_get_cacheline_reg_state_sharer(dir->idx, &state, &st_update, &sharer);
                                        bfrt_get_cacheline_reg_state_sharer(prev_dir->idx, &prev_state, &prev_st_update, &prev_sharer);
                                        if (!st_update && !prev_st_update && check_mergeable(state, prev_state, sharer, prev_sharer))
                                            try_merge = 1;
                                    }
                                }
                            }
                        }
                    }while(0);
                }
                pr_dyn_cache_v("DIR::FVA[0x%lx | 0x%lx] INV[%lu | %lu] Try[%d]\n",
                               dir->fva, adjacent_fva, // current and next possible
                               dir->inv_cnt, prev_dir ? prev_dir->inv_cnt : 0, try_split_or_merge);
            }

            if (try_split + try_merge)
            {
                int unlocked_dir = 0;
                // 1) Lock
                cacheman_recover_lock();

                // 2a) Grab one node and lock it
                bfrt_set_cacheline_lock(dir->idx, CACHELINE_LOCKED);

                // 2b) Check status: if empty, do the steps 5) and 6) to remove existing entry
                barrier();
                dir_lock = 0;
                bfrt_get_cacheline_reg(dir->idx, &state, &st_update, &sharer, &dir_size, &dir_lock, &inv_cnt);
                pr_dyn_cache_v("Directory Reg[%u]: state [0x%x] sharer [0x%x] size [0x%x] lock/cnt [%u/%u]\n",
                            (unsigned int)dir->idx, state, sharer, dir_size,
                            (unsigned int)dir_lock, (unsigned int)inv_cnt);
                barrier();
                if (!dir_lock)
                {
                    pr_dyn_cache("!!!Lock is not hold: Reg[%u]: state [0x%x] sharer [0x%x] size [0x%x] lock/cnt [%u/%u]\n",
                                    (unsigned int)dir->idx, state, sharer, dir_size,
                                    (unsigned int)dir_lock, (unsigned int)inv_cnt);
                }

                if (dir_size != dir->dir_size)
                {
                    BUG();
                }

                if (state == CACHELINE_EMPTY)
                {
                    // Remove empty cacheline
                    clear_cache_dir_no_lock(dir->fva);
                    cacheman_recover_unlock();
                    goto try_next_node;
                }

                // Other un-split-able states
                else if((state != CACHELINE_SHARED) && (state != CACHELINE_MODIFIED) && (state != CACHELINE_IDLE))
                {
                    goto unlock_and_try_next_node;
                }

                // 2c) Split
                if (try_split && !st_update && (dir_size > REGION_SIZE_4KB))
                {
#ifndef DYN_CACHE_DISABLE_SPLIT
                    uint16_t new_size_shift;
                    uint64_t new_size = size_index_to_size(dir_size - 1);
                    struct cacheline_dir *l_dir = cacheline_check_and_get(), *r_dir = cacheline_check_and_get();
                    u64 hkey;
                    hlist *old_hnode = cacheline_get_node(dir->fva);
                    if (!l_dir || !r_dir)
                    {
                        if (l_dir)
                        {
                            add_to_free_list(l_dir);
                        }

                        if (r_dir)
                        {
                            add_to_free_list(r_dir);
                        }
                        goto unlock_and_try_next_node;
                    }
                    gettimeofday(&t_lock_before, NULL);

                    // 3) push new entry to the data plane
                    // Add cache directory entries, then match rules
                    new_size_shift = dir_size - 1 + REGION_SIZE_BASE; // From rightmost bit
                    l_dir->fva = get_aligned_fva(dir->fva, new_size_shift);
                    r_dir->fva = l_dir->fva + new_size;
                    l_dir->dir_size = dir_size - 1;
                    r_dir->dir_size = dir_size - 1;

                    pr_dyn_cache_v("Directory Reg[%u]: state [0x%x] sharer [0x%x] size [0x%x] lock/cnt [%u/%u]\n",
                                   (unsigned int)dir->idx, state, sharer, dir_size,
                                   (unsigned int)dir_lock, (unsigned int)inv_cnt);
                    pr_dyn_cache_v("(1) Directory split: [%u | %u], fva [0x%lx/%u | 0x%lx/%u]\n",
                                   (unsigned int)l_dir->idx, (unsigned int)r_dir->idx, l_dir->fva, 64 - new_size_shift, r_dir->fva, 64 - new_size_shift);
                    // Prepare directory entries first
                    bfrt_add_cacheline_reg(l_dir->idx, state, sharer, l_dir->dir_size, CACHELINE_UNLOCKED, 0);
                    bfrt_add_cacheline_reg(r_dir->idx, state, sharer, r_dir->dir_size, CACHELINE_UNLOCKED, 0);
                    barrier();
                    // Prepare match rules for the entries
                    bfrt_add_cacheline(l_dir->fva, 64 - new_size_shift, l_dir->idx);
                    bfrt_add_cacheline(r_dir->fva, 64 - new_size_shift, r_dir->idx);
                    barrier();

                    pr_dyn_cache_v("(2) Directory split: [%u | %u], fva [0x%lx/%u | 0x%lx/%u]\n",
                                   (unsigned int)l_dir->idx, (unsigned int)r_dir->idx, l_dir->fva, 64 - new_size_shift, r_dir->fva, 64 - new_size_shift);
                    gettimeofday(&t_lock_after, NULL);
                    total_lock_time += (t_lock_after.tv_sec - t_lock_before.tv_sec) * 1000000 + (t_lock_after.tv_usec - t_lock_before.tv_usec);
                    
                    // 4) Add new entries to the list: will be added to the front
                    add_to_used_list(l_dir);
                    add_to_used_list(r_dir);

                    // 5) Remove old entry
                    if (dir->node_ptr)
                    {
                        lock_usedlist();
                        node = dir->node_ptr->next;
                        node_updated = 1;
                        unlock_usedlist();
                        delete_from_used_list(dir->node_ptr);   // now dir->node_ptr is freed
                    }else{
                        BUG();
                    }
                    gettimeofday(&t_lock_after, NULL);
                    bfrt_del_cacheline(dir->fva, 64 - (dir_size + REGION_SIZE_BASE));
                    barrier();

                    // Clean up entry
                    pr_dyn_cache_v("(3) Directory split - delete: [%u], fva [0x%lx/%u]\n",
                                   (unsigned int)dir->idx, dir->fva, 64 - (dir_size + REGION_SIZE_BASE));
                    unlocked_dir = 1;
                    delete_from_hash_list(dir->fva, old_hnode);
                    add_to_free_list(dir);
                    
                    // 6) Add new entry to the hash table
                    hkey = l_dir->fva >> CACHELINE_MIN_SHIFT;
                    add_to_hash_list(hash_ftn_u16(hkey), hkey, l_dir);
                    hkey = r_dir->fva >> CACHELINE_MIN_SHIFT;
                    add_to_hash_list(hash_ftn_u16(hkey), hkey, r_dir);

                    split_cnt ++;
#else
                    usleep(50000);
#endif
                }
                // 2c) Merge
                else if (try_merge && !st_update && (dir_size < REGION_SIZE_2MB))
                {
#ifndef DYN_CACHE_DISABLE_MERGE
                    uint64_t next_large_size = size_index_to_size(dir_size + 1);
                    if ((dir->fva % next_large_size) == 0)
                    {
                        // 2d) check adjacent entry (if needed for merge)
                        if (prev_hnode && prev_hnode->value && prev_dir)
                        {
                            uint16_t prev_sharer_ = 0, prev_dir_size = 0;
                            // Read register and check the state and counter
                            bfrt_set_cacheline_lock(prev_dir->idx, CACHELINE_LOCKED);
                            dir_lock = 0;
                            bfrt_get_cacheline_reg(prev_dir->idx, &prev_state, &prev_st_update, &prev_sharer_, &prev_dir_size, NULL, NULL);
                            pr_dyn_cache_v("Prev-directory Reg[%u]: state [0x%x] sharer [0x%x] size [0x%x] lock/cnt [%u/%u]\n",
                                           (unsigned int)prev_dir->idx, prev_state, prev_sharer_, dir_size,
                                           (unsigned int)dir_lock, (unsigned int)inv_cnt);
                            // Check merge-able condition
                            if ((dir_size == prev_dir_size) && !prev_st_update &&
                                check_mergeable(state, prev_state, sharer, prev_sharer_)
                               )
                            {
                                uint16_t new_size_shift;
                                struct cacheline_dir *new_dir = cacheline_check_and_get();
                                u64 hkey;
                                hlist *old_hnode = cacheline_get_node(dir->fva);
                                if (!new_dir)
                                {
                                    printf("WARNING: Out of cache line during merge\n");
                                    goto unlock_prev_and_try_next_node;
                                }

                                // Set up sharer and state
                                sharer |= prev_sharer_;  // Merge the sharer lists
                                if (state != CACHELINE_MODIFIED)
                                    state = (state != CACHELINE_IDLE) ? ((prev_state == CACHELINE_MODIFIED) ? prev_state : state) : prev_state;
                                new_size_shift = dir_size + 1 + REGION_SIZE_BASE; // now from rightmost bit
                                new_dir->fva = get_aligned_fva(dir->fva, new_size_shift);
                                new_dir->dir_size = dir_size + 1;

                                pr_dyn_cache_v("Inside merge-routine: new idx[%u] fva [0x%lx] state [0x%x] sharer [0x%x] size [0x%x]\n",
                                               new_dir->idx, new_dir->fva, state, sharer, new_dir->dir_size);

                                // 3) Push new entry to the data plane
                                bfrt_add_cacheline_reg(new_dir->idx, state, sharer, new_dir->dir_size, CACHELINE_UNLOCKED, 0);
                                barrier();
                                bfrt_add_cacheline(new_dir->fva, 64 - new_size_shift, new_dir->idx);
                                barrier();
                                
                                // 4) Add new entry to the list
                                add_to_used_list(new_dir);    // It will be added to the front

                                // 5) Remove old entry from the hash table
                                lock_usedlist();
                                if (dir->node_ptr && dir->node_ptr->next && dir->node_ptr->next != prev_dir->node_ptr)
                                {
                                    node = dir->node_ptr->next;
                                    node_updated = 1;
                                }
                                else if (prev_dir->node_ptr)
                                {
                                    node = prev_dir->node_ptr->next;
                                    node_updated = 1;
                                }
                                else
                                {
                                    unlock_usedlist();
                                    BUG();
                                }
                                unlock_usedlist();
                                if (dir->node_ptr)
                                {
                                    delete_from_used_list(dir->node_ptr);
                                }
                                else
                                {
                                    BUG();
                                }
                                if (prev_dir->node_ptr)
                                {   
                                    delete_from_used_list(prev_dir->node_ptr);
                                }
                                else
                                {
                                    BUG();
                                }
                                barrier();

                                // Clean up TCAMs (entries can be simply reused)
                                bfrt_del_cacheline(dir->fva, 64 - (prev_dir_size + REGION_SIZE_BASE));
                                bfrt_del_cacheline(prev_dir->fva, 64 - (prev_dir_size + REGION_SIZE_BASE));
                                delete_from_hash_list(dir->fva, old_hnode);
                                delete_from_hash_list(prev_dir->fva, prev_hnode);
                                unlocked_dir = 1;
                                add_to_free_list(dir);
                                add_to_free_list(prev_dir);

                                // 6) Add new entry to the hash table
                                hkey = new_dir->fva >> CACHELINE_MIN_SHIFT;
                                add_to_hash_list(hash_ftn_u16(hkey), hkey, new_dir);
                                merge_cnt ++;
                            }else{
unlock_prev_and_try_next_node:
                                unlock_cacheline(prev_dir->idx, prev_dir_size, try_split, try_merge);
                            }
                        }
                    }
#else
                    usleep(50000);
#endif
                }   // End of merge routine
unlock_and_try_next_node:
                if (!unlocked_dir)
                {
                    // If we do not changed nothing, unlock!
                    bfrt_set_cacheline_inv(dir->idx, 0);
                    unlock_cacheline(dir->idx, dir_size, try_split, try_merge);
                }
                cacheman_recover_unlock();
            }
            else if (dir)
            {
                bfrt_set_cacheline_inv(dir->idx, 0);    // Reset invalidation counter
            }
            else
            {
                BUG();
            }
try_next_node:
            // For next iteration
            if (!node_updated)
            {
                lock_usedlist();
                node = node->next;
                unlock_usedlist();
            }
        }
// unlock_and_continue:
        // Configuration and print out for the epoch
        gettimeofday(&t_after, NULL);
        pthread_spin_unlock(&cacheman_usedlist_lock);
        cacheman_run_unlock();
        unlock_requested = 0;

        get_timestamp(time_buf, 32);
        time_epoch = ((t_after.tv_sec - t_before.tv_sec) * TIME_SEC_IN_US + (t_after.tv_usec - t_before.tv_usec));

        if (t_after.tv_sec > t_last_update.tv_sec + DYN_CACHE_SLEEP_CYCLE - 1)
        {
            FILE *fp = c_get_datetime_filep();
            pr_dyn_cache("[%s] Split: %lu, merge: %lu, free: %d, threshold: %lu, time: %ld us | %ld us/entry, lock (sp): %ld us/entry\n", 
                        time_buf, split_cnt, merge_cnt, get_free_dir_cnt(), inv_threshold,
                        time_epoch, (split_cnt + merge_cnt > 0) ? time_epoch / (long)(split_cnt + merge_cnt) : 0,
                        split_cnt > 0 ? total_lock_time / (long)split_cnt : 0);
            if (fp)
            {
                // Time, free dir entries, invalidation threshold, period of epoch, time per merge/split
                fprintf(fp, "%s, %d, %lu\n", 
                        time_buf, get_free_dir_cnt(), inv_threshold);
                fflush(fp);
            }
            t_last_update.tv_sec = t_after.tv_sec;
            usleep(100);
        }
    }
}