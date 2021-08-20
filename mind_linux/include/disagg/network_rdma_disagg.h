#ifndef __NETWORK_RDMA_DISAGGREGATION_H__
#define __NETWORK_RDMA_DISAGGREGATION_H__

#ifndef __CONTROLLER__
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/types.h>

#include "header_for_ofa_kernel.h"
#endif
#include "network_fit_disagg.h"

#ifndef __packed
#define __packed __attribute__((packed))
#endif

#define DISAGG_RDMA_COMPUTE_TYPE    1
#define DISAGG_RDMA_MEMORY_TYPE     2

#define DISAGG_RDMA_GID_FORMAT "00000000000000000000000000000000"
#define DISAGG_RDMA_INIT_SLEEP_TIME_IN_MS   10000    // 10 sec
#define DISAGG_RDMA_POLLING_SKIP_COUNTER    250000

/* send information of this machine, same data will be sent back from the server */
struct rdma_msg_struct {
    int ret;
	u32	node_id;
	u32	node_type;
    u32 node_id_res;    // when the receiver of this msg needs to update its node id
    u64 addr;
    u64 size;
    u32 rkey;
    u32 lkey;
    // CPU blade: DISAGG_QP_PER_COMPUTE, memory blade: DISAGG_MAX_COMPUTE_NODE * DISAGG_QP_PER_COMPUTE
    u32 qpn[DISAGG_MAX_COMPUTE_NODE * DISAGG_QP_PER_COMPUTE];
    // Only used by CPU blade
    u64 ack_buf[DISAGG_QP_PER_COMPUTE];    
    u32 psn;
    u32 lid;    // not needed for RoCE
    u8 mac[ETH_ALEN];
    //only first half will be used (carrying raw data not string)
    char gid[sizeof(DISAGG_RDMA_GID_FORMAT)];
    u64 base_addr;
} __packed;

typedef int (*rdma_msg_callback)(u32, void *, u32, void *, u32);
void set_rdma_msg_callback(rdma_msg_callback callbk);

typedef u64 (*page_init_callback)(void *, unsigned long);  // VA -> DMA address
void set_rdma_rmap_callback(page_init_callback callbk);

typedef void *(*get_inval_buf_callback)(int);
void set_inval_buf_callback(get_inval_buf_callback callbk);

struct dest_info;
#endif
