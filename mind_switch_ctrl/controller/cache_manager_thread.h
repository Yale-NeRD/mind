#ifndef __CACHE_SIZE_MANAGER_H__
#define __CACHE_SIZE_MANAGER_H__

enum {
    CACHE_PROFILE_MERGE_REASON_STATE = 0,
    CACHE_PROFILE_MERGE_REASON_SHARER = 1,
};
#define CACHE_PROFILE_MERGE_REASON_NUM 4
#define MAX_SPLIT_MERGE_PER_EPOCH 1000000

void cache_man_init(void);
struct server_service;
void run_cache_size_thread(struct server_service *cache_man);

void cacheman_run_lock();
void cacheman_run_unlock();
void cacheman_request_unlock();
void cacheman_recover_lock();
void cacheman_recover_unlock();

extern FILE *c_get_datetime_filep(void);
#endif
