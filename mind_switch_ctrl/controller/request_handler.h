#ifndef __MN_REQUEST_HANDLER_H__
#define __MN_REQUEST_HANDLER_H__

#include <stdio.h>
#include "memory_management.h"
#include "./include/disagg/network_disagg.h"
#include "./include/disagg/network_rdma_disagg.h"
#include "./include/disagg/network_fit_disagg.h"
#include "list_and_hash.h"
#include <pthread.h>

#define MN_PID_HASH_BIT         8
#define MN_CACHE_HASH_BIT       16  // will be extended to 16
#define RDMA_CTRL_GID "00000000000000000000ffff0a0a0a01" // hardcoded GID for 10.10.10.1

struct unique_tgid_node
{
    u32                 utgid;
    struct task_struct  *tsk;
    int                 local_ref;        // If it brecome 0, it should be freed
};

struct node_tgid_hash
{
    u16 node_id;
    u16 tgid;
    struct unique_tgid_node *utgid_node;
};

struct task_struct *mn_get_task(u16 sender_id, u16 tgid);
struct task_struct *mn_get_task_by_utgid(u32 utgid);

int mn_insert_new_task_mm(u16 sender_id, u16 tgid, struct task_struct* tsk);
int mn_link_to_task_mm(u16 sender_id, u16 tgid, u32 utgid);
int mn_delete_task(u16 sender_id, u16 tgid);
void increase_utgid_ref(u16 sender_id, u16 tgid);

// Main functions for handling requests
struct socket;
int handle_fork(struct mem_header* hdr, void* payload, struct socket *sk, int id);
int handle_exec(struct mem_header *hdr, void *payload, struct socket *sk, int id);
int handle_exit(struct mem_header* hdr, void* payload, struct socket *sk, int id);
int handle_mmap(struct mem_header* hdr, void* payload, struct socket *sk, int id);
int handle_brk(struct mem_header* hdr, void* payload, struct socket *sk, int id);
int handle_munmap(struct mem_header* hdr, void* payload, struct socket *sk, int id);
int handle_mremap(struct mem_header* hdr, void* payload, struct socket *sk, int id);

int handle_pfault(struct mem_header* hdr, void* payload, struct socket *sk, int id);

// Debug
int handle_check(struct mem_header *hdr, void *payload);

// RDMA support: local version of dest_info (different than kernel's one)
struct dest_info
{
    int node_id;
    int lid;
    int qpn[NUM_PARALLEL_CONNECTION];
    int psn;
    u64 base_addr;
    u64 size;
    u64 ack_buf[NUM_PARALLEL_CONNECTION];
    u8 mac[ETH_ALEN];
    char gid[sizeof(DISAGG_RDMA_GID_FORMAT)];
    u32 lkey;
    u32 rkey;
    struct socket *sk;
    struct socket *out_sk;
};

struct mn_status
{
    int node_id;
    //TODO: struct list_head for the list of struct memory_node_mapping list
    struct dest_info *node_info;
    unsigned long alloc_size;
    //TODO: locking for parallel access
    struct list_node alloc_vma_list;
    spinlock_t alloc_lock;
};

void initialize_node_list(void);
struct dest_info *get_node_info(unsigned int nid);
int sw_handle_rdma_init(struct mem_header *hdr, void *payload, struct socket *sk, int id, char *ip_addr);
struct dest_info *ctrl_set_node_info(unsigned int nid, int lid, u8 *mac,
                                     unsigned int *qpn, int psn, char *gid,
                                     u32 lkey, u32 rkey,
                                     u64 addr, u64 size, u64 *ack_buf,
                                     struct socket *sk);
int get_memory_node_num(void);
struct mn_status **get_memory_node_status(int *num_mn);
void debug_increase_mem_node_count(void);

//RDMA version of main functions for handling requests
struct thpool_buffer;
int handle_fork_rdma(struct common_header* chdr, void* payload, struct thpool_buffer *tb);
int handle_exec_rdma(struct common_header* chdr, void* payload);
int handle_exit_rdma(struct common_header* chdr, void* payload, struct thpool_buffer *tb);
int handle_mmap_rdma(struct common_header* chdr, void* payload, struct thpool_buffer *tb);
int handle_brk_rdma(struct common_header* chdr, void* payload, struct thpool_buffer *tb);
int handle_munmap_rdma(struct common_header* chdr, void* payload, struct thpool_buffer *tb);
int handle_mremap_rdma(struct common_header* chdr, void* payload, struct thpool_buffer *tb);

int handle_pfault_rdma(struct common_header* chdr, void* payload, struct thpool_buffer *tb);
int handle_data_rdma(struct common_header* chdr, void* payload);
int handle_check_rdma(struct common_header* chdr, void* payload);

// Utils
char *get_memory_node_ip(int nid);
int get_nid_from_ip_str(char *ip_addr, int is_mem_node);
void error_and_exit(const char *err_msg, const char *ftn, long line);

#endif
