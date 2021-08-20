#ifndef __ROCE_DISAGG_MODULE_H__
#define __ROCE_DISAGG_MODULE_H__

// NOW __CN_ROCE__ and __CN_ROCE_TEST__ are included in Makefile

#include "header_for_ofa_kernel.h"

#include "../../include/disagg/network_disagg.h"
#include "../../include/disagg/network_rdma_disagg.h"
#include "../../include/disagg/network_fit_disagg.h"
#include "../../include/disagg/fork_disagg.h"
#include "../../include/disagg/exec_disagg.h"
#include "../../include/disagg/exit_disagg.h"
#include "../../include/disagg/mmap_disagg.h"
#include "../../include/disagg/fault_disagg.h"
#include "../../include/disagg/cnthread_disagg.h"
#include "../../include/disagg/profile_points_disagg.h"

#include <linux/module.h>

#include <linux/kthread.h>
#include <linux/sched/signal.h>

#include <linux/errno.h>
#include <linux/types.h>

#include <linux/netdevice.h>
#include <linux/ip.h>

#include <linux/unistd.h>
#include <linux/wait.h>

#include <linux/time.h>
#include <linux/ktime.h>

// RMDA related functions
int send_rdma_meta(unsigned int lid, unsigned int psn,
                   unsigned int *qpn, char *gid,
                   unsigned int lkey, unsigned int rkey,
                   unsigned long *ack_buf,
                   struct dest_info *dest);

int send_msg_via_rdma(u32 msg_type, void *payload, u32 len_payload,
                      void *retbuf, u32 max_len_retbuf);

void rdma_init(void);
int rdma_device_init(void);

void set_ib_dev(struct ib_device *ib_dev);

// message functions using RDMA
int send_rdma_read_data(unsigned long dma_addr, u64 addr, unsigned long size, unsigned int rkey, char **ack_buf);
int send_rdma_write_data(void *buf, u64 addr, unsigned long size, unsigned int rkey, int is_dma_addr);
int send_rdma_write_ack(void *buf, u64 addr, unsigned long size, unsigned int rkey);

#endif  /* __NETWORK_SERVER_MODULE_H__ */