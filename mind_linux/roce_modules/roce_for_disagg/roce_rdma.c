#include "roce_disagg.h"
#include "../../include/disagg/print_disagg.h"

static struct rdma_context roce_ctx;
static struct ib_device *ibapi_dev;

void set_ib_dev(struct ib_device *ib_dev)
{
    ibapi_dev = ib_dev;
}

void async_event_handler(struct ib_event_handler *ieh, struct ib_event *ie)
{
    printk(KERN_DEFAULT "RDMA_API: async_event_handler\n");
}

u64 cn_reg_mr_ftn(void *addr, unsigned long size)
{
    return cn_reg_mr_addr(&roce_ctx, addr, size);
}

// Module and RoCE connection initizliazation
void rdma_init(void)
{
    msleep(DISAGG_RDMA_INIT_SLEEP_TIME_IN_MS);
    initialize_roce_core();

    // try to connect server
    printk(KERN_DEFAULT "RDMA_API: start FIT layer initialization...\n");
    roce_ctx.ib_port_id = 1;
    cn_conn_switch(&roce_ctx, ibapi_dev, roce_ctx.ib_port_id, get_local_node_id());
    printk(KERN_DEFAULT "RDMA_API: FIT layer has been initialized\n");

    /* WE CAN PUT TEST CODE HERE */
    while (1)
    {

        if (kthread_should_stop())
        {
            break;
        }

        if (signal_pending(current))
        {
            __set_current_state(TASK_RUNNING);
            break;
        }

        msleep_interruptible(5 * 1000);
    }

    //TODO: gracefully exit and release resources of RDMA
    // if (roce_ctx)
    //     kfree(roce_ctx);

    do_exit(0);
}

/*
 * One-sided RDMA read and write over RC
 */
// addr: the address offset, absolute address will be (the base address of the pair_node) + (addr)
__always_inline int send_rdma_read_data(unsigned long dma_addr, u64 addr, unsigned long size, unsigned int rkey, char **ack_buf)
{
    // barrier();
    int conn_id = smp_processor_id();
    int ret = 0;
    if (ack_buf)
    {
        *ack_buf = roce_ctx.local_rdma_recv_rings[DISAGG_QP_ACK_OFFSET + conn_id];
        pr_rdma(KERN_DEFAULT "RDMA ACK BUF: 0x%lx, DMA: 0x%lx\n",
                (unsigned long)*ack_buf, (unsigned long)roce_ctx.local_rdma_ring_mrs[DISAGG_QP_ACK_OFFSET + conn_id].dma_addr);
    }
    ret = rdma_read_data(&roce_ctx, DISAGG_CONTROLLER_NODE_ID, dma_addr, addr, size, rkey);

    return ret;
}

__always_inline int send_rdma_write_data(void *buf, u64 addr, unsigned long size, unsigned int rkey, int is_dma_addr)
{
    // barrier();
    uintptr_t temp_addr = 0;

    int ret, conn_id;
    if (smp_processor_id() < DISAGG_NUM_CPU_CORE_IN_COMPUTING_BLADE - 2)
        conn_id = DISAGG_QP_EVICT_OFFSET_START;
    else
    {
        conn_id = DISAGG_QP_EVICT_OFFSET_END - (smp_processor_id() - (DISAGG_NUM_CPU_CORE_IN_COMPUTING_BLADE - 2));
        if (unlikely(conn_id < DISAGG_QP_EVICT_OFFSET_START))
            conn_id = DISAGG_QP_EVICT_OFFSET_START;
    }
    if (!is_dma_addr)
        temp_addr = cn_reg_mr_addr(&roce_ctx, buf, size);
    else
        temp_addr = (uintptr_t)buf;
    pr_rdma(KERN_DEFAULT "RDMA WRITE: Addr: 0x%lx, Buf: 0x%lx, isDma: %d\n",
            (unsigned long)addr, (unsigned long)buf, is_dma_addr);
    ret = rdma_write_data(&roce_ctx, DISAGG_CONTROLLER_NODE_ID, temp_addr, addr, size, rkey, conn_id, 1);
    if (!is_dma_addr)
        cn_unmap_mr_addr(&roce_ctx, temp_addr, size);	// dma address here
    return ret;
}

//ctx->qp[pair_conn_id + conn_idx]->qp_num
__always_inline int send_rdma_write_ack(void *buf, u64 addr, unsigned long size, unsigned int rkey)
{
    int conn_id = DISAGG_QP_INV_ACK_OFFSET_START + atomic_inc_return(&roce_ctx.next_inval_ack_conn) % DISAGG_QP_NUM_INVAL_BUF;
    // == now we set up QP inside rdma_write_data() ==
    // rkey |= ((roce_ctx.qp[conn_id]->qp_num) << CACHELINE_ROCE_QP_EMBED_OFFSET);
    // rkey |= (roce_ctx.qp[conn_id]->qp_num);
    // ===============================================
    // Here, buf is the dma address
    return rdma_write_data(&roce_ctx, DISAGG_CONTROLLER_NODE_ID, (uintptr_t)buf, addr, size, rkey, conn_id, 0);
}
