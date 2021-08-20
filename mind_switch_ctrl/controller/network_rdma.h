#ifndef __MN_NETWORK_RDMA_MODULE_H__
#define __MN_NETWORK_RDMA_MODULE_H__

#include "memory_management.h"
#include "../../include/disagg/network_disagg.h"
#include "../../include/disagg/network_rdma_disagg.h"
#include "../../include/disagg/network_fit_disagg.h"
#include "../../include/disagg/profile_points_disagg.h"

struct mn_status
{
    int node_id;
    //TODO: struct list_head for the list of struct memory_node_mapping list
    struct dest_info *node_info;
    unsigned long alloc_size;
    //TODO: locking for parallel access
    struct list_head alloc_vma_list;
    spinlock_t alloc_lock;
};

ppc *
fit_initalize_structs(ppc *ctx, struct ib_device *ib_dev, int ib_port, int mynodeid);
int rdma_library_init(void);
void rdma_init(void);

void rdma_get_handshake_data( int pair_node_id, struct dest_info* local_info);

// API for sending message through RDMA FIT
inline int ibapi_reply_message(void *addr, int size, uintptr_t descriptor);

ppc *get_rdma_context(void);
struct mn_status **get_memory_node_status(int *num_node);
int rdma_read_data(ppc *ctx, int pair_node_id, void *buf, u64 addr, unsigned long size);
int rdma_write_data(ppc *ctx, int pair_node_id, void *buf, u64 addr, unsigned long size);
int rdma_check_magic_number(ppc *ctx, int pair_node_id);

// Pin CPU
int pin_current_thread(void);

#endif /* __MN_NETWORK_RDMA_MODULE_H__ */
