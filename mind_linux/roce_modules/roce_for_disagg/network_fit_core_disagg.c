/*
 * Please check "../../include/disagg/network_fit_disagg.h"
 * for reference and development notes
 */

#include "roce_disagg.h"
#include "../../include/disagg/network_rdma_disagg.h"
#include "../../include/disagg/network_disagg.h"
#include "../../include/disagg/network_fit_disagg.h"
#include "../../include/disagg/profile_points_disagg.h"
#include "../../disagg/print_disagg.h"

#include <linux/kthread.h>
#include <linux/sched/signal.h>

#include <asm/byteorder.h>

#include <rdma/ib_verbs.h>
#include <rdma/rdma_cm.h>

/* Profiling */
#ifdef CONFIG_PROFILING_POINTS
static struct profile_point *_PP_NAME(NET_nic_send_wq_rdma);
static struct profile_point *_PP_NAME(NET_nic_poll_cq_rdma);
static struct profile_point *_PP_NAME(NET_nic_prepare_rdma);
static struct profile_point *_PP_NAME(NET_nic_tot_rdma);
#endif

void initialize_roce_core(void)
{
#ifdef CONFIG_PROFILING_POINTS
	_PP_NAME(NET_nic_send_wq_rdma) = _PP_EXPORT_NAME(NET_nic_send_wq_rdma)();
	_PP_NAME(NET_nic_poll_cq_rdma) = _PP_EXPORT_NAME(NET_nic_poll_cq_rdma)();
	_PP_NAME(NET_nic_prepare_rdma) = _PP_EXPORT_NAME(NET_nic_prepare_rdma)();
	_PP_NAME(NET_nic_tot_rdma) = _PP_EXPORT_NAME(NET_nic_tot_rdma)();
#endif
}

/*
 * If we can not get the CQE within 10 seconds
 * There should be something wrong.
 */
#define FIT_POLL_CQ_TIMEOUT_NS	(5000000000L)
#define FIT_POLL_CQ_RETRY_NS	(500000000L)	// 250 ms for heavy invalidation

/* message related functions */
static inline void hlt(void)
{
	asm volatile (
		"1: hlt\n\t"
		"jmp 1b\n\t"
	);
}

inline uintptr_t
cn_reg_mr_phys_addr(struct rdma_context *ctx, void *addr, size_t length)
{
	struct ib_device *ibd = (struct ib_device*)ctx->context;
	return (uintptr_t)phys_to_dma(ibd->dma_device, (phys_addr_t)addr);
}

inline uintptr_t
cn_reg_mr_addr(struct rdma_context *ctx, void *addr, size_t length)
{
#ifdef MIND_USE_PHY_DMA_TO_ROCE
	return cn_reg_mr_phys_addr(ctx, (void *)virt_to_phys(addr), length);
#else
	uintptr_t dma_addr =
		(uintptr_t)ib_dma_map_single((struct ib_device *)ctx->context,
									 addr, length, DMA_BIDIRECTIONAL);
	if (ib_dma_mapping_error((struct ib_device *)ctx->context, dma_addr))
		return (uintptr_t)NULL;

	return dma_addr;
#endif
}

inline void
cn_unmap_mr_addr(struct rdma_context *ctx, u64 dma_addr, size_t length)
{
#ifndef MIND_USE_PHY_DMA_TO_ROCE
	ib_dma_unmap_single((struct ib_device *)ctx->context, dma_addr, length, DMA_BIDIRECTIONAL);
#endif
}

__always_inline static int _cn_internal_poll_sendcq(struct rdma_context *ctx, struct ib_cq *tar_cq,
				    int connection_id, int *check, uintptr_t input_mr_addr, uint32_t rkey);

inline static void check_and_try_reset_cache_dir(uintptr_t input_mr_addr, u32 rkey)
{
	u16 r_state = 0, r_sharer = 0, r_size = 0, r_lock = 0, r_cnt = 0;
	u16 pid = input_mr_addr >> MN_VA_PID_SHIFT;
	u64 vaddr = input_mr_addr & (~MN_VA_PID_BIT_MASK);
	send_cache_dir_full_always_check(pid, vaddr & PAGE_MASK, //CNTHREAD_CACHLINE_MASK,
							&r_state, &r_sharer, &r_size, &r_lock, &r_cnt, 
							CN_SWITCH_RST_ON_UPDATE);	// reset entry if it is a problem
	printk(KERN_WARNING "ERROR - diag: PID: %u, VA: 0x%llx, state: 0x%x, sharer: 0x%x, size: %u, lock: %u, cnt: %u, rkey: 0x%x\n",
			pid, vaddr, r_state, r_sharer, r_size, r_lock, r_cnt, rkey);
}

__always_inline int
cn_send_message_read(
	struct rdma_context *ctx, int connection_id, uint32_t input_mr_rkey,
	uintptr_t input_mr_addr, unsigned long dma_addr, int size)
{
	struct ib_rdma_wr wr;
	const struct ib_send_wr *bad_wr = NULL;
	struct ib_sge sge[2];
	uintptr_t temp_header_addr = 0;
	int poll_status = SEND_REPLY_WAIT;
	int ret, poll_ret;
	PROFILE_POINT_TIME(NET_nic_prepare_rdma)
	PROFILE_POINT_TIME(NET_nic_tot_rdma)

	PROFILE_START(NET_nic_tot_rdma);
	PROFILE_START(NET_nic_prepare_rdma);
	memset(&wr, 0, sizeof(wr));
	memset(&sge, 0, sizeof(sge));

	wr.wr.sg_list = sge;
	wr.remote_addr = (uintptr_t)(input_mr_addr);
	wr.rkey = input_mr_rkey;
	wr.wr.wr_id = (uint64_t)&poll_status;
	wr.wr.opcode = IB_WR_RDMA_READ;
	wr.wr.send_flags = IB_SEND_SIGNALED;
	wr.wr.num_sge = 1;

	/* Get the physical address of user message */
	// ASSUME that we have mapped address
	sge[0].addr = dma_addr;
	sge[0].length = size;
	sge[0].lkey = ctx->proc->lkey;
	pr_rdma("IB_WR_RDMA_READ: conn :%d, length: 0x%lx, mapped[addr: 0x%llx]\n",
			connection_id, (unsigned long)size, (u64)dma_addr);

	ret = ib_post_send(ctx->qp[connection_id], &wr.wr, &bad_wr);
	if (unlikely(ret)) {
		pr_info("Fail to post send to con:%d ret:%d\n",
			connection_id, ret);
		WARN_ON_ONCE(1);
		return ret;
	}
	PROFILE_LEAVE_PTR(NET_nic_prepare_rdma);

	poll_ret = _cn_internal_poll_sendcq(ctx, ctx->send_cq[connection_id],
										 connection_id, &poll_status, input_mr_addr, input_mr_rkey);
	if (unlikely(poll_ret == -ETIMEDOUT)) {
		pr_debug("mode: %d remote addr: 0x%llx, rkey: %#x. "
			 "local addr: 0x%lx header: 0x%lx lkey: %#x\n",
			M_READ, wr.remote_addr, wr.rkey,
			(unsigned long)dma_addr, (unsigned long)temp_header_addr, ctx->proc->lkey);
	}else if (poll_ret == -EIO){
		// NACK: reinitialize QP and return
		check_and_try_reset_cache_dir(input_mr_addr, input_mr_rkey);
		reset_rdma_qp_to_ready_to_send(ctx, 0, connection_id, 1);
		return -1;
	}
	pr_rdma("%s: sent message for r_addr: 0x%llx\n",
			__func__, wr.remote_addr);
	PROFILE_LEAVE_PTR(NET_nic_tot_rdma);
	return 0;
}

__always_inline int
cn_send_message_write(
	struct rdma_context *ctx, int connection_id, uint32_t input_mr_rkey,
	uintptr_t input_mr_addr, uintptr_t addr, int size)
{
	struct ib_rdma_wr wr;
	const struct ib_send_wr *bad_wr = NULL;
	struct ib_sge sge[2];
	uintptr_t temp_addr = 0;
	uintptr_t temp_header_addr = 0;
	int poll_status = SEND_REPLY_WAIT;
	int ret, poll_ret;
	PROFILE_POINT_TIME(NET_nic_send_wq_rdma)
	PROFILE_POINT_TIME(NET_nic_poll_cq_rdma)
retry_write:
	memset(&wr, 0, sizeof(wr));
	memset(&sge, 0, sizeof(sge));

	wr.wr.sg_list = sge;
	wr.remote_addr = (uintptr_t)(input_mr_addr);
	wr.rkey = input_mr_rkey;
	wr.wr.wr_id = (uint64_t)&poll_status;
	wr.wr.opcode = IB_WR_RDMA_WRITE;
	wr.wr.send_flags = IB_SEND_SIGNALED;
	wr.wr.num_sge = 1;

	/* Get the physical address of user message */
	// ASSUME that we have mapped address
	sge[0].addr = addr;
	sge[0].length = size;
	sge[0].lkey = ctx->proc->lkey;
	pr_rdma("IB_WR_RDMA_WRITE: conn :%d, length: 0x%lx, local[addr: 0x%lx] mapped[addr: 0x%llx]\n",
			connection_id, (unsigned long)size, (unsigned long)addr, (u64)temp_addr);

	PROFILE_START(NET_nic_send_wq_rdma);
	ret = ib_post_send(ctx->qp[connection_id], &wr.wr, &bad_wr);
	if (unlikely(ret))
	{
		pr_info("Fail to post send to con:%d ret:%d\n",
				connection_id, ret);
		WARN_ON_ONCE(1);
		return ret;
	}
	PROFILE_LEAVE_PTR(NET_nic_send_wq_rdma);

	PROFILE_START(NET_nic_poll_cq_rdma);
	poll_ret = _cn_internal_poll_sendcq(ctx, ctx->send_cq[connection_id],
										 connection_id, &poll_status, input_mr_addr, input_mr_rkey);
	PROFILE_LEAVE_PTR(NET_nic_poll_cq_rdma);
	if (unlikely(poll_ret == -ETIMEDOUT))
	{
		pr_debug("mode: %d remote addr: 0x%llx, rkey: %#x. "
				 "local addr: 0x%lx header: 0x%lx lkey: %#x\n",
				 M_WRITE, wr.remote_addr, wr.rkey,
				 (unsigned long)temp_addr, (unsigned long)temp_header_addr, ctx->proc->lkey);
	}else if (poll_ret == -EIO){
		// NACK: reinitialize QP and return
		check_and_try_reset_cache_dir(input_mr_addr, input_mr_rkey);
		reset_rdma_qp_to_ready_to_send(ctx, 0, connection_id, 1);
		msleep(500);
		goto retry_write;
	}
	pr_rdma("%s: sent message for r_addr: 0x%llx\n",
			__func__, wr.remote_addr);
	return 0;
}

struct pinned_thread_info
{
	int cpu;
	struct task_struct *p;
	struct list_head next;
};

__always_inline static int _cn_internal_poll_sendcq(struct rdma_context *ctx, struct ib_cq *tar_cq,
				    int connection_id, int *check, uintptr_t input_mr_addr, uint32_t rkey)
{
	/*
	 * This is the safest version.
	 * No batching, no any optimization.
	 */
	int ne, i;
	struct ib_wc wc[2];
	unsigned long start_ns, print_ns, end_ns;
	u16 recover = 0;

	start_ns = jiffies;
	print_ns = start_ns;
	do {
		for (i = 0; i < DISAGG_NET_POLLING_SKIP_COUNTER; i++)
		{
			ne = ib_poll_cq(tar_cq, 1, wc);
			if (unlikely(ne < 0))
			{
				pr_err("Fail to poll send_cq. Err: %d", ne);
				return ne;
			}else if(ne > 0){
				goto received;
			}
			try_invalidation_lookahead(PROFILE_CNTHREAD_INV_ACK_SERV_FROM_PRMPT, SERVE_ACK_PER_WAIT);
		}
		// check timer
		end_ns = jiffies;
		if (unlikely((end_ns > start_ns) && (jiffies_to_usecs(end_ns - start_ns) > FIT_POLL_CQ_RETRY_NS / 1000)))
		{
			if (!recover)
			{
				// send_cache_dir_full_always_check(pid, vaddr & PAGE_MASK, //CNTHREAD_CACHLINE_MASK,
				// 						&r_state, &r_sharer, &r_size, &r_lock, &r_cnt, 
				// 						CN_SWITCH_RST_ON_UPDATE);	// reset entry if it is a problem
				check_and_try_reset_cache_dir(input_mr_addr, rkey);
				recover = 1;
			}
			// #endif
			if (unlikely((end_ns > print_ns) && (jiffies_to_usecs(end_ns - print_ns) > FIT_POLL_CQ_TIMEOUT_NS / 1000)))
			{
				// send_cache_dir_full_always_check(pid, vaddr & PAGE_MASK, //CNTHREAD_CACHLINE_MASK,
				// 								 &r_state, &r_sharer, &r_size, &r_lock, &r_cnt,
				// 								 CN_SWITCH_REG_SYNC_NONE);
				pr_info("\n"
						"*****\n"
						"***** Fail to to get the CQE from send_cq (%p) after %ld seconds!\n"
						"***** CPU: %d connection_id: %d dest node: %d\n"
						"*****\n",
						tar_cq,
						FIT_POLL_CQ_TIMEOUT_NS / NSEC_PER_SEC, smp_processor_id(),
						connection_id, connection_id / NUM_PARALLEL_CONNECTION);
				print_ns = jiffies;
				recover = 0;	// retry recovery
				// ib_req_notify_cq(tar_cq, IB_CQ_NEXT_COMP);	// does it help?
			}
			// WARN_ON_ONCE(1);
			start_ns = jiffies;
		}
	} while (ne < 1);
received:
	for (i = 0; i < ne; i++) {
		if (wc[i].status != IB_WC_SUCCESS) {
			pr_rdma("wc.status: %s", ib_wc_status_msg(wc[i].status));
			return -EIO;
		}
	}
	return 0;
}
