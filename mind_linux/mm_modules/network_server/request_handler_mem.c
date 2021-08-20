#include "network_server.h"
#include "memory_management.h"
#include "request_handler.h"
#include <linux/io.h>

// memory management requets
static unsigned long mapped_base_addr = 0;
static unsigned long mapped_size = 0;
static void *mapped_area = NULL;

void set_base_addr(unsigned long base_addr, unsigned long size)
{
    mapped_base_addr = base_addr;
    mapped_size = size;

    if (mapped_area)
        iounmap(mapped_area);

    mapped_area = ioremap(mapped_base_addr, mapped_size);
    pr_info("MEMREMAPPED AREA: 0x%lx - 0x%lx\n", 
            (unsigned long)mapped_area, (unsigned long)mapped_area + mapped_size);
}

int handle_mem_init(struct mem_header *hdr, void *payload, struct socket *sk, int id)
{
    struct meminit_reply_struct reply;
    struct meminit_msg_struct *meminit_req = (struct meminit_msg_struct *)payload;
    int ret = -1;
    
    if (!mapped_area)
        goto meminit_send_reply;

    // pr_info("Initialize memory with zero: 0x%lx (size: 0x%llx)\n",
    //         (unsigned long)(mapped_area + meminit_req->offset),
    //         meminit_req->len);

    memset(mapped_area + meminit_req->offset, 0, meminit_req->len);
    ret = 0;
    barrier();

meminit_send_reply:
    reply.ret = ret;
    tcp_server_send(sk, id, (const char *)&reply, sizeof(reply), MSG_DONTWAIT);
    return ret;
}

int handle_mem_cpy(struct mem_header *hdr, void *payload, struct socket *sk, int id)
{
    struct memcpy_reply_struct reply;
    struct memcpy_msg_struct *memcpy_req = (struct memcpy_msg_struct *)payload;
    int ret = -1;

    if (!mapped_area)
        goto memcpy_send_reply;

    // pr_info("Copy memory: 0x%lx <- 0x%lx (size: 0x%llx)\n",
    //         (unsigned long)(mapped_area + memcpy_req->dst_offset),
    //         (unsigned long)(mapped_area + memcpy_req->src_offset),
    //         memcpy_req->len);

    memcpy(mapped_area + memcpy_req->dst_offset, // destination
           mapped_area + memcpy_req->src_offset, // source
           memcpy_req->len);                     // length
    ret = 0;
    barrier();

memcpy_send_reply:
    reply.ret = ret;
    tcp_server_send(sk, id, (const char *)&reply, sizeof(reply), MSG_DONTWAIT);
    return ret;
}