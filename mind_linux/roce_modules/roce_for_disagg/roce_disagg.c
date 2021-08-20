#include "roce_disagg.h"

#include <asm/traps.h> // X86_PF_WRITE

#ifndef MODULE_NAME
#define MODULE_NAME "roce_for_disaggregation"
#endif

// #define ROCE_DEBUG_RDMA_RW
MODULE_LICENSE("GPL");

static spinlock_t send_rdma_lock;

#ifdef ROCE_DEBUG_RDMA_RW
static unsigned long calculate_page_sum(void* buf)
{
    unsigned long ret = 0;
    unsigned long *data = buf;
    int i;
    for (i = 0; i < PAGE_SIZE / sizeof(unsigned long); i++)
    {
        ret += *(data + i);
    }
    return ret;
}
#endif

// pointer of profile
#ifdef CONFIG_PROFILING_POINTS
static struct profile_point *_PP_NAME(NET_total);
static struct profile_point *_PP_NAME(NET_send_rdma);
#endif

// convert RDMA request with RDMA READ or WRITE
int send_msg_via_rdma(u32 msg_type, void *payload, u32 len_payload,
                      void *retbuf, u32 max_len_retbuf)
{
    int ret = -1;
    u32 rkey = 0;   // which will embed permission request

    PROFILE_POINT_TIME(NET_total)
    PROFILE_POINT_TIME(NET_send_rdma)

    if (!retbuf)
        return -ERR_DISAGG_NET_INCORRECT_BUF;

    /* Network */
    if (msg_type == DISSAGG_PFAULT)
    {
        struct fault_msg_struct *req = (struct fault_msg_struct *)payload;
        struct fault_reply_struct *reply = (struct fault_reply_struct *)retbuf;
        u64 full_va;

        PROFILE_START(NET_total);
        req->address = req->address & PAGE_MASK;
        full_va = generate_full_addr(req->tgid, req->address);
        rkey = VM_READ;
        if (req->error_code & X86_PF_WRITE)
            rkey |= VM_WRITE;
        rkey <<= MN_RKEY_PERMISSION_SHIFT;

        ret = send_rdma_read_data((unsigned long)reply->data, full_va, PAGE_SIZE, rkey, &reply->ack_buf); // read one page

        // if success, then access was valid (and verified by the switch)
        reply->ret = 0;
        if (ret >= 0)
        {
            reply->ret = DISAGG_FAULT_READ;
            reply->data_size = PAGE_SIZE;
        }
        PROFILE_LEAVE_PTR(NET_total);
    }
    else if (msg_type == DISSAGG_DATA_PUSH || msg_type == DISSAGG_DATA_PUSH_OTHER || msg_type == DISSAGG_DATA_PUSH_TARGET || msg_type == DISSAGG_DATA_PUSH_DUMMY)
    {
        struct fault_data_struct *req = (struct fault_data_struct *)payload;
        struct fault_reply_struct *reply = (struct fault_reply_struct *)retbuf;
        u64 full_va;
#ifdef ROCE_DEBUG_RDMA_RW
        int ret_tmp = -1;
        void *buf = kmalloc(PAGE_SIZE, GFP_KERNEL); // do we need margin?
        unsigned long sent_sum, recv_sum;
        if (!buf)
        {
            //error
            BUG();
        }
#endif
        req->address = req->address & PAGE_MASK;
        full_va = generate_full_addr(req->tgid, req->address);
        if (msg_type == DISSAGG_DATA_PUSH || msg_type == DISSAGG_DATA_PUSH_OTHER)  // passthrough switch
            rkey = VM_NONE << MN_RKEY_PERMISSION_SHIFT;
//
        else if (msg_type == DISSAGG_DATA_PUSH_DUMMY)   // dummy: will not be used
            rkey = VM_READ << MN_RKEY_PERMISSION_SHIFT;
//
        else if (msg_type == DISSAGG_DATA_PUSH_TARGET)
        {
            rkey = VM_WRITE << MN_RKEY_PERMISSION_SHIFT; // this is writing back to memory and invaliation requester
            rkey |= req->req_qp;
        }
        ret = send_rdma_write_data(req->data, full_va, PAGE_SIZE, rkey,
                                   (msg_type == DISSAGG_DATA_PUSH_OTHER) || (msg_type == DISSAGG_DATA_PUSH_TARGET)); // addr is dma addr

#ifdef ROCE_DEBUG_RDMA_RW
        // check written data
        ret_tmp = send_rdma_read_data(buf, full_va, PAGE_SIZE, rkey); // read one page

        // print out sent data
        sent_sum = calculate_page_sum(req->data);
        recv_sum = calculate_page_sum(buf);
        kfree(buf);
        buf = NULL;
        if (ret >= 0 && ret_tmp >= 0 && sent_sum != recv_sum)
        {
            printk(KERN_DEFAULT "Sent data [0x%lx]: 0x%lx <-> 0x%lx\n",
                   req->address, sent_sum, recv_sum);
            BUG();
        }
        printk("<f>");
#endif

        // if success, then access was valid (and verified by the switch)
        if (ret >= 0)
        {
            // data_size SHOULD BE 4KB (=PAGE_SIZE)
            reply->ret = 0;
        }
    }
    else if (msg_type == DISSAGG_ROCE_INVAL_ACK)
    {
        struct fault_data_struct *req = (struct fault_data_struct *)payload;
        u64 full_va;

        full_va = generate_full_addr(req->tgid, req->address);
        rkey = VM_INV_ACK << MN_RKEY_PERMISSION_SHIFT;
        ret = send_rdma_write_ack(req->data, full_va, req->data_size, rkey);
    }
    else if (msg_type == DISSAGG_ROCE_FIN_ACK || msg_type == DISSAGG_ROCE_EVICT_ACK)
    {
        struct fault_data_struct *req = (struct fault_data_struct *)payload;
        u64 full_va;

        full_va = generate_full_addr(req->tgid, req->address);
        if (msg_type == DISSAGG_ROCE_FIN_ACK)
            rkey = (VM_READ | VM_WRITE) << MN_RKEY_PERMISSION_SHIFT;
        else
            rkey = MN_RKEY_VM_EVICTION << MN_RKEY_PERMISSION_SHIFT;
        ret = send_rdma_write_ack(req->data, full_va, req->data_size, rkey);
    }
    return ret;
}

/* TCP related functions */
int send_rdma_meta(unsigned int lid, unsigned int psn, unsigned int *qpn, char *gid,
                   unsigned int lkey, unsigned int rkey, unsigned long *ack_buf,
                   struct dest_info *dest)
{
    struct rdma_msg_struct payload;
    struct rdma_msg_struct *reply;
    int ret;
    int i;
    int gid_size = min(sizeof(payload.gid), sizeof(union ib_gid));

    reply = kzalloc(sizeof(struct rdma_msg_struct), GFP_KERNEL);
    if (!reply)
        return -ENOMEM;

    payload.ret = 0;
    payload.node_id = get_local_node_id();
#ifdef CONFIG_COMPUTING_NODE
    payload.node_type = DISAGG_RDMA_COMPUTE_TYPE;
#else
    payload.node_type = DISAGG_RDMA_MEMORY_TYPE;
#endif
    payload.lid = lid;
    payload.psn = psn;
    for (i = 0; i < NUM_PARALLEL_CONNECTION; i++)
        payload.qpn[i] = qpn[i];
    payload.lkey = lkey;
    payload.rkey = rkey;
    for (i = 0; i < NUM_PARALLEL_CONNECTION; i++)
        payload.ack_buf[i] = ack_buf[i];
    memcpy(payload.gid, gid, gid_size);
    payload.addr = 0;
    payload.size = 0;

    pr_info("Gid size: %d\n", gid_size);
    for (i = 0; i < gid_size; i++)
    {
        pr_info("0x%02x", gid[i]);
    }
    pr_info("\n");

    ret = send_msg_to_memory(DISAGG_RDMA, &payload, sizeof(payload),
                             reply, sizeof(struct rdma_msg_struct));

    if (ret < sizeof(payload))
        ret = -EINTR;
    else if (reply->ret)
    {                     // only 0 is success
        ret = reply->ret; // set error
    }
    else
    {
        ret = 0;

        // set up data
        if (dest)
        {
            dest->node_id = reply->node_id;
            dest->lid = reply->lid;
            dest->psn = reply->psn;
            for (i = 0; i < NUM_PARALLEL_CONNECTION; i++)
                dest->qpn[i] = reply->qpn[i];
            memset((void *)&(dest->gid), 0, sizeof(dest->gid));
            memcpy((void *)&(dest->gid), reply->gid, min(sizeof(dest->gid), sizeof(reply->gid)));
            dest->lkey = reply->lkey;
            dest->rkey = reply->rkey;
            dest->base_addr = reply->base_addr;
        }
        set_local_node_id(reply->node_id_res);
    }
    kfree(reply);
    return ret;
}

static int __init disagg_rdma_init(void)
{
    int ret = 0;
    // initialize device
    spin_lock_init(&send_rdma_lock);
    ret = rdma_device_init();
#ifdef CONFIG_PROFILING_POINTS
    _PP_NAME(NET_total) = _PP_EXPORT_NAME(NET_total)();
    _PP_NAME(NET_send_rdma) = _PP_EXPORT_NAME(NET_send_rdma)();
#endif
    return 0;
}

static void __exit disagg_rdma_exit(void)
{
	return ;
}

module_init(disagg_rdma_init)
module_exit(disagg_rdma_exit)
