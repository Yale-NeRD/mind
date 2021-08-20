#ifndef __NETWORK_SERVER_MODULE_H__
#define __NETWORK_SERVER_MODULE_H__

#include "header_for_ofa_kernel.h"

#define __MEMORY_NODE__

#include "../../include/disagg/network_disagg.h"
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

#include <net/inet_connection_sock.h>
#include <net/request_sock.h>
#include "request_handler.h"

// TODO: this should be configurable from kernel config
#define MEM_NODE_ID DISAGG_MEMORY_NODE_ID
#define MEMORY_BASE_LOCATION    0x200000000     // start from 8 GB
#define MEMORY_TOTAL_SIZE       0x1E00000000       // 120 GB
// #define MEMORY_TOTAL_SIZE       0x800000000       // 32 GB (which uses 8 ~ 40 GB)

// Intializer
int init_mn_man(void);
int clear_mn_man(void);

// TCP functions
int tcp_server_send(struct socket *sock, int id, const char *buf,
        const size_t length, unsigned long flags);

int tcp_server_receive(struct socket *sock, int id, struct sockaddr_in *address,
                       unsigned char *buf, int size, unsigned long flags);

int send_one_msg_to_ctrl(u32 msg_type, void *payload, u32 len_payload,
                         void *retbuf, u32 max_len_retbuf);

#endif  /* __NETWORK_SERVER_MODULE_H__ */