#include "types.h"
#include "cacheline_def.h"
#include "cacheline_manager.h"
#include "list_and_hash.h"
#include "vm_flags.h"
#include "debug.h"
#include "controller.h"
#include "request_handler.h"

#include <endian.h>
#include <stdio.h>
#include <netinet/in.h>
#ifndef __STDC_NO_ATOMICS__
#include <stdatomic.h>
#endif

static const unsigned long cacheline_number = DYN_CACHE_TAR_DIR;

// Rkey encodings / masks
static const u32 cacheline_rkey_state_mask = CACHELINE_ROCE_RKEY_STATE_MASK; // last 4 bits
static const u32 cacheline_rkey_invalidation_mask = CACHELINE_ROCE_RKEY_INVALIDATION_MASK; // 9th bit from right

// Locks
static pthread_spinlock_t cacheline_usedlist_lock;
static pthread_spinlock_t cacheline_freelist_lock;
static pthread_spinlock_t cacheline_hashtb_lock;
// Lists and hash lists
static struct list_node cacheline_free_cache;
static struct list_node cacheline_used_cache;
static struct hash_table cache_directory;

// Counters
static _Atomic int free_dir_cnt;
static _Atomic int dir_print_cnt;

static void cacheline_freelist_init(void)
{
    struct cacheline_dir *tmp = NULL;
    unsigned long i = 0;

    list_init(&cacheline_free_cache);
    for (i = 0; i < cacheline_number; i++)
    {
        tmp = malloc(sizeof(struct cacheline_dir));
        tmp->idx = i;
        tmp->inv_cnt = 0;
        list_insert_at_tail(&cacheline_free_cache, tmp);
    }

    list_init(&cacheline_used_cache);
    ht_create(&cache_directory, (1 << MN_CACHE_HASH_BIT));  // hash table size
}

void cacheline_init(void)
{
    // Initialize global data structures
    pthread_spin_init(&cacheline_freelist_lock, PTHREAD_PROCESS_PRIVATE);
    pthread_spin_init(&cacheline_usedlist_lock, PTHREAD_PROCESS_PRIVATE);
    pthread_spin_init(&cacheline_hashtb_lock, PTHREAD_PROCESS_PRIVATE);
    atomic_init(&free_dir_cnt, cacheline_number);
    atomic_init(&dir_print_cnt, 0);
    cacheline_freelist_init();
}

static void cacheline_freelist_clear(void)
{
    struct list_node *tmp, *next;

    printf("Start to clean up cache free/used lists\n");
    pthread_spin_lock(&cacheline_freelist_lock);
    tmp = cacheline_free_cache.next;
    while (tmp && tmp != &cacheline_free_cache)
    {
        next = tmp->next;
        if (tmp->data)
            free(tmp->data); // The cacheline_dir structure
        tmp->data = NULL;
        list_delete_node_no_header(tmp);
        tmp = next;
    }
    pthread_spin_unlock(&cacheline_freelist_lock);
    printf("Cache free lists cleared\n");

    pthread_spin_lock(&cacheline_usedlist_lock);
    tmp = cacheline_used_cache.next;
    while (tmp && tmp != &cacheline_used_cache)
    {
        next = tmp->next;
        if (tmp->data)
            free(tmp->data); // The cacheline_dir structure
        list_delete_node_no_header(tmp);
        tmp = next;
    }
    pthread_spin_unlock(&cacheline_usedlist_lock);
    printf("Cache used lists cleared\n");

    pthread_spin_lock(&cacheline_hashtb_lock);
    ht_free(&cache_directory);
    pthread_spin_unlock(&cacheline_hashtb_lock);
    printf("Cache directory hash lists cleared\n");
}

void cacheline_clear(void)
{
    cacheline_freelist_clear();
}

int get_free_dir_cnt(void)
{
    return atomic_load(&free_dir_cnt);
}

struct cacheline_dir * cacheline_check_and_get(void)
{
    struct list_node *free_node = NULL;
    struct cacheline_dir *dir_ptr = NULL;
    pthread_spin_lock(&cacheline_freelist_lock);
    if (cacheline_free_cache.next == &cacheline_free_cache || cacheline_free_cache.next == NULL)
    {
        free_node = NULL;
    }else{
        free_node = cacheline_free_cache.next;
        list_detach_node(free_node);
    }
    pthread_spin_unlock(&cacheline_freelist_lock);
    if (free_node)
    {
        dir_ptr = (struct cacheline_dir *)free_node->data;
        dir_ptr->fva = 0;
        dir_ptr->inv_cnt = 0;
        dir_ptr->dir_size = 0;
        free(free_node);
        atomic_fetch_sub(&free_dir_cnt, 1);
    }
    return dir_ptr;
}

void lock_usedlist(void)
{
    pthread_spin_lock(&cacheline_usedlist_lock);
}

void unlock_usedlist(void)
{
    pthread_spin_unlock(&cacheline_usedlist_lock);
}

struct list_node *get_first_used_node(void)
{
    struct list_node *res;
    pthread_spin_lock(&cacheline_usedlist_lock);
    res = cacheline_used_cache.next;
    pthread_spin_unlock(&cacheline_usedlist_lock);
    return res;
}

int was_last_usedlist_node (struct list_node *node)
{
    return (!node || (node == &cacheline_used_cache) || (node == &cacheline_free_cache)) ? 1 : 0;
}

void add_to_used_list(struct cacheline_dir* dir)
{
    pthread_spin_lock(&cacheline_usedlist_lock);
    list_insert_at_head(&cacheline_used_cache, dir);
    dir->node_ptr = cacheline_used_cache.next;
    pthread_spin_unlock(&cacheline_usedlist_lock);
}

void delete_from_used_list(struct list_node *node)
{
    pthread_spin_lock(&cacheline_usedlist_lock);
    list_delete_node_no_header(node);   // Detach and remove the node
    pthread_spin_unlock(&cacheline_usedlist_lock);
}

void add_to_free_list(struct cacheline_dir *dir)
{
    pthread_spin_lock(&cacheline_freelist_lock);
    list_insert_at_tail(&cacheline_free_cache, dir);
    pthread_spin_unlock(&cacheline_freelist_lock);
    atomic_fetch_add(&free_dir_cnt, 1);
}

void add_to_hash_list_no_lock(unsigned int hash, u64 key, struct cacheline_dir *dir)
{
    ht_put(&cache_directory, hash, key, dir);
}

void add_to_hash_list(unsigned int hash, u64 key, struct cacheline_dir *dir)
{
    pthread_spin_lock(&cacheline_hashtb_lock);
    add_to_hash_list_no_lock(hash, key, dir);
    pthread_spin_unlock(&cacheline_hashtb_lock);
}

void delete_from_hash_list(u64 fva, hlist *hash_node)
{
    u64 hkey = fva >> CACHELINE_MIN_SHIFT;
    pthread_spin_lock(&cacheline_hashtb_lock);
    ht_free_node(&cache_directory, hash_ftn_u16(hkey), hash_node);
    pthread_spin_unlock(&cacheline_hashtb_lock);
}

// Used for sending both ACK and NACK
static void send_ack(struct socket *sk, u64 fva, void *buf, int size, char *ip_str, u32 ret_msg)
{
    char *va_dest = ((char *)buf) + CACHELINE_ROCE_OFFSET_TO_VADDR;
    char *rkey_dest = ((char *)buf) + CACHELINE_ROCE_OFFSET_TO_RKEY;
    int ret = -1;
    memset(buf, 0, size);
    fva = htobe64(fva);
    memcpy(va_dest, &fva, sizeof(fva));
    ret_msg = htobe32(ret_msg);
    memcpy(rkey_dest, &ret_msg, sizeof(ret_msg));
    // Destination port
    ((struct sockaddr_in *)&sk->client_addr)->sin_port = htons(DEFAULT_UDP_PORT);
    ret = sendto(sk->sock_fd, buf, size, MSG_DONTWAIT,
                (struct sockaddr *)&sk->client_addr, sk->caddr_len);
    if (ret > 0)
    {
        // printf("ACK [%u] send to %s: FVA: 0x%lx\n", ret_msg, ip_str, fva);
    }
    else
        printf("Failed to send ACK to %s: FVA: 0x%lx\n", ip_str, fva);
}

hlist *cacheline_get_node_no_lock(u64 fva)
{
    u64 hkey = fva >> CACHELINE_MIN_SHIFT;
    return ht_get_node(&cache_directory, hash_ftn_u16(hkey), hkey);
}

hlist *cacheline_get_node(u64 fva)
{
    hlist *res;
    pthread_spin_lock(&cacheline_hashtb_lock);
    res = cacheline_get_node_no_lock(fva);
    pthread_spin_unlock(&cacheline_hashtb_lock);
    return res;
}

struct cacheline_dir *cacheline_get_dir_no_lock(u64 fva)
{
    struct cacheline_dir *res = NULL;
    u64 hkey = fva >> CACHELINE_MIN_SHIFT;
    res = (struct cacheline_dir *)ht_get(&cache_directory, hash_ftn_u16(hkey), hkey);
    return res;
}

struct cacheline_dir *cacheline_get_dir(u64 fva)
{
    struct cacheline_dir *res = NULL;
    pthread_spin_lock(&cacheline_hashtb_lock);
    res = cacheline_get_dir_no_lock(fva);
    pthread_spin_unlock(&cacheline_hashtb_lock);
    return res;
}

u32 *cacheline_get_cidx(u64 fva)
{
    struct cacheline_dir *res = cacheline_get_dir(fva);
    if (res)
        return &res->idx;
    return NULL;
}

inline static int add_check_is_print_now(void)
{
    int cnt = atomic_fetch_add(&dir_print_cnt, 1) + 1;
    return (cnt % 1000 == 0 || (unsigned long)atomic_load(&free_dir_cnt) == cacheline_number);
}

void check_and_print_cacheline(u64 aligned_fva, int c_idx)
{
    if (add_check_is_print_now())
    {
        printf("Cleared cache [free: %u]- PID+VA: 0x%lx => Idx: %d\n",
                atomic_load(&free_dir_cnt), aligned_fva, c_idx);
        atomic_store(&dir_print_cnt, 0);
    }
}

inline u64 get_aligned_fva(uint64_t fva, uint16_t shift)
{
    return (fva >> shift) << shift;
}

inline void *create_new_cache_dir(u64 fva, u16 state, int nid, uint16_t dir_size)
{
    struct cacheline_dir *free_dir = NULL;
    u16 sharer = 0;
    int prev_exist = 0;
    u64 hkey = fva >> CACHELINE_MIN_SHIFT;
    hlist *hash_node;
    
    pthread_spin_lock(&cacheline_hashtb_lock);
    // Check the existing entry
    hash_node = cacheline_get_node_no_lock(fva);
    pr_cache("Entry for FVA: 0x%llx, hash: 0x%lx, key: 0x%lx\n",
             fva, (unsigned long)hash_ftn_u16(hkey), (unsigned long)hkey);
    if (dir_size > REGION_SIZE_2MB)
        dir_size = REGION_SIZE_2MB;
    if (!hash_node)
    {
        uint16_t dir_lock = CACHELINE_UNLOCKED;
        u64 aligned_fva = get_aligned_fva(fva, REGION_SIZE_BASE + dir_size);
        uint32_t inv_cnt = 0;
        // Check cacheline is full
        free_dir = cacheline_check_and_get();
        // TODO: no free cacheline, evict one - must not be happen as we have enough slots
        if (!free_dir)
        {
            goto error;
        }
        free_dir->fva = fva;
        free_dir->dir_size = dir_size;
        // Now we have free_dir holding free cacheline

        // Add cacheline information
        // - Generate new entry consisting of sharer list, current mode
        // - sharer: retrieve node id and mark the current node bit as 1
        // - Compute node id starts from 1, but we use 0-based indexing here
        if (state != CACHELINE_IDLE)
            sharer = 1 << (nid - 1);

        // Cacheline TCAM to register index connection (match rule)
        bfrt_add_cacheline_reg(free_dir->idx, state, sharer, dir_size, dir_lock, inv_cnt);
        bfrt_add_cacheline(aligned_fva, (64 - (dir_size + REGION_SIZE_BASE)), free_dir->idx);
        add_to_used_list(free_dir);
        add_to_hash_list_no_lock(hash_ftn_u16(hkey), hkey, free_dir);
        if (add_check_is_print_now())
        {
            printf("Populated cache [new: %d, free %u] - PID+VA: 0x%lx => Idx: %d, State: 0x%x, Sharer: 0x%04x\n",
                !prev_exist, atomic_load(&free_dir_cnt), aligned_fva, free_dir->idx, state, sharer);
            dir_print_cnt = 0;
        }
        pthread_spin_unlock(&cacheline_hashtb_lock);
        return free_dir;
    }else{
error:
    pthread_spin_unlock(&cacheline_hashtb_lock);
    printf("Existing cache directory!! FVA: 0x%lx\n", fva);
    return NULL; // Existing node in the hash list
    }
}

static int populate_cache_dir(struct socket *sk, char *ip_str, void *buf, int size, u64 fva, u32 rkey)
{
    u64 vaddr;
    u16 pid, state;
    int nack = CACHELINE_ROCE_RKEY_NACK_MASK;   // 1 bit, so also value it self
    // make compiler happy
    (void)vaddr;
    (void)pid;

    pid = (unsigned int)(fva >> 48);
    vaddr = fva & 0xFFFFFFFFFFFF; // Virtual address
    pr_cache("FVA: 0x%lx => PID: %u, VA: 0x%lx\n", fva, pid, vaddr);

    // 1) state: retrieve permission (stored in rkey, convert endian)
    if (rkey & (VM_WRITE << MN_RKEY_PERMISSION_SHIFT))
        state = CACHELINE_MODIFIED;
    else
        state = CACHELINE_SHARED;

    // TODO: find proper directory size instead of using the smallest (0 = 4 KB)
    if (!create_new_cache_dir(fva, state, (get_nid_from_ip_str(ip_str, 0)), 0)) 
    {
        // Out of cache directory: this is nack for debugging
        send_ack(sk, fva, buf, size, ip_str, nack);
        return -1; //DEBUG
    }

    // Send ack back to the computing node
    send_ack(sk, fva, buf, size, ip_str, state);
    return 0;
}

// This is the function forwarded memory access requests are handled
int cacheline_miss_handler(struct socket *sk, char *ip_str, void *buf, int size)
{
    u64 fva;
    u32 rkey, state;
    char time_buf[32] = {0};
    // Find vaddr
    fva = *((u64 *)(&(((char *)buf)[CACHELINE_ROCE_OFFSET_TO_VADDR])));
    fva = be64toh(fva); // network endian to host endian (e.g., big to little in usual X86)
    // Find rkey
    rkey = *((u32 *)(&(((char *)buf)[CACHELINE_ROCE_OFFSET_TO_RKEY])));
    rkey = be32toh(rkey);
    state = (rkey & cacheline_rkey_state_mask);

    pr_cache("Cachemiss handler - FVA: 0x%lx, Rkey: 0x%x, State: 0x%04x\n",
           fva, rkey, state);

    if (rkey & cacheline_rkey_invalidation_mask)
    {
        get_timestamp(time_buf, 32);
        printf("[%s] Invalidation forwarded-PID+VA: 0x%lx, Rkey: 0x%x, State: 0x%04x\n",
               time_buf, fva, rkey, state);
    }

    if (state == CACHELINE_EMPTY)
    {
        // Since the directory become idle, we can delete the directory
        // clear_cache_dir(fva);
        // NOW, the empty lines would be cleared by cache manager thread, 
        // when it walks through the directory entries
        return 0;
    }
    else if(state > 0)  // Forwarded after cache update
    {
        return 0;
    }
    else    // Forwarded before cache update (=no cache directory)
    {
        get_timestamp(time_buf, 32);
        pr_dyn_cache("[%s] NEW_CACHE: UDP msg from: %s, FVA: 0x%lx, Rkey: 0x%x, State: 0x%04x\n", 
                     time_buf, ip_str, fva, rkey, state);
        return populate_cache_dir(sk, ip_str, buf, size, fva, rkey);
    }
}
