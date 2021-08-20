#ifndef __CACHELINE_MANAGER_H__
#define __CACHELINE_MANAGER_H__

#include <pthread.h>
#include "types.h"
#include "list_and_hash.h"

struct socket;

struct cacheline_dir
{
    u32 idx;
    u64 fva;
    u32 inv_cnt;    // recently read invalidation counter
    u16 dir_size;
    struct list_node *node_ptr;
};

u32 *cacheline_get_cidx(u64 fva);
void *create_new_cache_dir(u64 fva, u16 state, int nid, uint16_t dir_size);
void try_clear_cache_dir(u64 fva);
int cacheline_miss_handler(struct socket *sk, char *ip_str, void *buf, int size);
u64 get_aligned_fva(uint64_t fva, uint16_t shift);  // used for hash ftn calculation

// List related functions
// - Locks
void lock_usedlist(void);
void unlock_usedlist(void);
// - Get
int get_free_dir_cnt(void);
hlist *cacheline_get_node(u64 fva);
struct list_node *get_first_used_node(void);
struct cacheline_dir *cacheline_get_dir_no_lock(u64 fva);
struct cacheline_dir *cacheline_get_dir(u64 fva);
u32 *cacheline_get_cidx(u64 fva);
// - Add / delete
void add_to_used_list(struct cacheline_dir* dir);
void delete_from_used_list(struct list_node *node);
void add_to_hash_list(unsigned int hash, u64 key, struct cacheline_dir *dir);
void add_to_free_list(struct cacheline_dir *dir);
void delete_from_hash_list(u64 fva, hlist *hash_node);

// - Utils
int was_last_usedlist_node (struct list_node *node);
struct cacheline_dir * cacheline_check_and_get(void);
void check_and_print_cacheline(u64 aligned_fva, int c_idx);

#endif
