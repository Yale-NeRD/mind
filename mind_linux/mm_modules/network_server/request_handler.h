#ifndef __MN_REQUEST_HANDLER_H__
#define __MN_REQUEST_HANDLER_H__

#include "../../include/disagg/network_fit_disagg.h"

// struct file_info *mn_get_file(u16 sender_id, u16 tgid);

// memory management requets
void set_base_addr(unsigned long base_addr, unsigned long size);
int handle_mem_init(struct mem_header *hdr, void *payload, struct socket *sk, int id);
int handle_mem_cpy(struct mem_header *hdr, void *payload, struct socket *sk, int id);

// RDMA version of main functions for handling requests
int handle_rdma_init(struct mem_header* hdr, void* payload, struct socket *sk, int id);

//debug
int handle_check(struct mem_header* hdr, void* payload);
#endif /* __MN_REQUEST_HANDLER_H__ */
