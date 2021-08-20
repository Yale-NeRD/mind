#ifndef __MN_NETWORK_RDMA_MODULE_H__
#define __MN_NETWORK_RDMA_MODULE_H__


#include "../../include/disagg/network_disagg.h"
#include "../../include/disagg/network_rdma_disagg.h"
#include "../../include/disagg/network_fit_disagg.h"
#include "../../include/disagg/profile_points_disagg.h"

struct rdma_context *_initalize_struct(struct rdma_context *ctx, struct ib_device *ib_dev, int ib_port, int mynodeid);
int rdma_library_init(void);
void rdma_init(void);

void rdma_get_handshake_data( int pair_node_id, struct dest_info* local_info);
#endif /* __MN_NETWORK_RDMA_MODULE_H__ */
