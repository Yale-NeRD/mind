/*
 * Please check "../../include/disagg/network_fit_disagg.h"
 * for reference and development notes
 */

#include "roce_disagg.h"
#include "../../include/disagg/network_rdma_disagg.h"
#include "../../include/disagg/network_disagg.h"
#include "../../include/disagg/network_fit_disagg.h"
#include "../../include/disagg/cnthread_disagg.h"

#include <linux/kthread.h>
#include <linux/sched/signal.h>

#include <asm/byteorder.h>

#include <rdma/ib_verbs.h>
#include <rdma/rdma_cm.h>
#include <rdma/ib_cache.h>

// management of node infos
static struct dest_info node_infos[DISAGG_MAX_NODE];

// profiling
#ifdef CONFIG_PROFILING_POINTS
static struct profile_point *_PP_NAME(NET_send_rdma_fit);
static struct profile_point *_PP_NAME(NET_send_rdma_lock);
#endif

unsigned int get_node_global_lid(unsigned int nid)
{
	return node_infos[nid].lid;
}

unsigned int get_node_first_qpn(unsigned int nid)
{
	return node_infos[nid].qpn[0];
}

struct dest_info *set_node_info(unsigned int nid, int lid, u8 *mac, unsigned int* qpn, int psn, union ib_gid *gid)
{
	int i;
	node_infos[nid].node_id = nid;
	node_infos[nid].lid = lid;
	for (i=0; i<NUM_PARALLEL_CONNECTION; i++)
		node_infos[nid].qpn[i] = qpn[i];
    node_infos[nid].psn = psn;
    memcpy((void*)&node_infos[nid].gid, (void*)gid, sizeof(*gid));
    return get_node_info(nid);
}

static void gid_to_wire_gid(const union ib_gid *gid, char *wgid)
{
	char tmp_gid[sizeof(DISAGG_RDMA_GID_FORMAT)];
	int i;

	memcpy(tmp_gid, gid, sizeof(union ib_gid));
	for (i = 0; i < sizeof(union ib_gid); ++i)
		sprintf(&wgid[i * 2], "%02x", tmp_gid[i] & 0xff);
    wgid[32] = '\0';
}

__always_inline struct dest_info *get_node_info(unsigned int nid)
{
    return &node_infos[nid];
}

void print_node_info(unsigned int nid)
{
	int i;
	char gid_char[sizeof(DISAGG_RDMA_GID_FORMAT) + 1] = {0};
	gid_to_wire_gid(&node_infos[nid].gid, gid_char);
	pr_debug("DMA_FIT_API - node info[%d]: lid: %d, psn: %d, gid: %s\n",
			  nid, node_infos[nid].lid, node_infos[nid].psn, gid_char);
	pr_debug("            - qpn list:");
	for (i = 0; i < NUM_PARALLEL_CONNECTION; i++)
		pr_debug(" %d", node_infos[nid].qpn[i]);
	pr_debug("\n");
}

struct rdma_context* cn_init_context(struct rdma_context *ctx, int size, int rx_depth, int port,
				  struct ib_device *ib_dev, int mynodeid)
{
	int i;
	int num_total_connections = NUM_PARALLEL_CONNECTION;
	int rem_node_id;
    struct ib_cq_init_attr cq_attr = {};

	ctx->node_id = mynodeid;
	ctx->send_flags = IB_SEND_SIGNALED;
	ctx->rx_depth = rx_depth;
	ctx->num_connections = num_total_connections;
	ctx->num_node = DISAGG_MAX_NODE;
	ctx->num_parallel_connection = NUM_PARALLEL_CONNECTION;
	ctx->context = (struct ib_context *)ib_dev;
	atomic_set(&ctx->next_inval_data_conn, 0);
	atomic_set(&ctx->next_inval_ack_conn, 0);

	ctx->pd = ib_alloc_pd(ib_dev, 0);
	if (IS_ERR_OR_NULL(ctx->pd)) 
	{
		printk(KERN_ALERT "RDMA_FIT_API: Fail to initialize pd / ctx->pd\n");
		return NULL;
	}

	ctx->proc = ib_get_dma_mr(ctx->pd, IB_ACCESS_LOCAL_WRITE | IB_ACCESS_REMOTE_WRITE | IB_ACCESS_REMOTE_READ);
	if (IS_ERR_OR_NULL(ctx->proc)) 
	{
		printk(KERN_ERR "RDMA_FIT_API: Fail to get dma mr\n");
		return NULL;
	}
	pr_debug("RDMA_FIT_API: proc lkey: %x rkey: %x\n", ctx->proc->lkey, ctx->proc->rkey);

	ctx->num_alive_connection = kmalloc(ctx->num_node*sizeof(atomic_t), GFP_KERNEL);
	atomic_set(&ctx->num_alive_nodes, 1);
	memset(ctx->num_alive_connection, 0, ctx->num_node*sizeof(atomic_t));
	for(i = 0; i < ctx->num_node; i++)
		atomic_set(&ctx->num_alive_connection[i], 0);

	atomic_set(&ctx->num_completed_threads, 0);

	ctx->cq = kmalloc(NUM_POLLING_THREADS * sizeof(struct ib_cq *), GFP_KERNEL);
	if (!ctx->cq) {
		pr_err("RDMA_FIT_API: OOM - couldn't allocate CQ\n");
		return NULL;
	}

	memset(&cq_attr, 0, sizeof(struct ib_cq_init_attr));
	for(i = 0; i < NUM_POLLING_THREADS; i++) {
		/*
		 * XXX
		 * why choose rx_depth*4+1 this maginc number? Reason???
		 */
        cq_attr.cqe = rx_depth * 4 + 1;
		ctx->cq[i] = ib_create_cq((struct ib_device *)ctx->context, NULL, NULL, NULL, &cq_attr);
		if (IS_ERR_OR_NULL(ctx->cq[i]))
		{
			printk(KERN_ERR "RDMA_FIT_API: Fail to create recv_cq %d. Error: %d",
				i, PTR_ERR_OR_ZERO(ctx->cq[i]));
			return NULL;
		}
	}

	ctx->qp = kmalloc(num_total_connections * sizeof(struct ib_qp *), GFP_KERNEL);
	ctx->send_cq = kmalloc(num_total_connections * sizeof(struct ib_cq *), GFP_KERNEL);
	ctx->connection_count = kmalloc(num_total_connections * sizeof(atomic_t), GFP_KERNEL);
	for (i = 0; i < num_total_connections; i++)
		atomic_set(&ctx->connection_count[i], 0);

	for (i = 0; i < num_total_connections; i++) 
	{
		struct ib_qp_attr attr;

		memset(&attr, 0, sizeof(attr));

		rem_node_id = i/NUM_PARALLEL_CONNECTION;
		pr_debug("RDMA_FIT_API: mynodeid %d i %d connecting node %d\n", ctx->node_id, i, rem_node_id);
		if (rem_node_id == ctx->node_id)
			continue;

		/*
		 * XXX
		 * why rx_depth+1 ???
		 */
        cq_attr.cqe = rx_depth+1;
		ctx->send_cq[i] = ib_create_cq((struct ib_device *)ctx->context, NULL, NULL, NULL, &cq_attr);
		if (IS_ERR_OR_NULL(ctx->send_cq[i])) 
        {
			printk(KERN_ERR "RDMA_FIT_API: Fail to create send_CQ-%d Error: %d\n",
				i, PTR_ERR_OR_ZERO(ctx->send_cq[i]));
			return NULL;
		}

		{
			struct ib_qp_init_attr init_attr;
			memset(&init_attr, 0, sizeof(struct ib_qp_init_attr));
			init_attr.send_cq = ctx->send_cq[i];
			init_attr.recv_cq = ctx->cq[i % NUM_POLLING_THREADS];
			init_attr.cap.max_inline_data = MAX_INLINED_DATA;
			init_attr.cap.max_send_wr = MAX_OUTSTANDING_SEND;
			init_attr.cap.max_recv_wr = rx_depth;
			init_attr.cap.max_send_sge = 16;
			init_attr.cap.max_recv_sge = 16;
			init_attr.srq = NULL;
			init_attr.qp_type = IB_QPT_RC;
			init_attr.sq_sig_type = IB_SIGNAL_REQ_WR;
#ifndef _DISAGG_USE_RC_CONNECTION_
			if (DISAGG_QP_INV_ACK_OFFSET_START <= i && i <= DISAGG_QP_INV_ACK_OFFSET)
			{
				init_attr.qp_type = IB_QPT_UC;
			}
#endif
            ctx->qp[i] = ib_create_qp(ctx->pd, &init_attr);
            if (IS_ERR_OR_NULL(ctx->qp[i])) {
                printk(KERN_ERR "RDMA_FIT_API: Fail to create qp[%d]. Error: %d",
                    i, PTR_ERR_OR_ZERO(ctx->qp[i]));
                return NULL;
            }
            ib_query_qp(ctx->qp[i], &attr, IB_QP_CAP, &init_attr);
            ctx->send_flags |= IB_SEND_INLINE;
		}
	}
	return ctx;
}

enum ib_mtu mtu;
static int sl;

void *_alloc_memory(unsigned int length)
{
	void *tempptr;
	tempptr = kzalloc(length, GFP_KERNEL);	// Modify from kzalloc to kmalloc
	if(!tempptr)
		printk(KERN_CRIT "%s: alloc error\n", __func__);
	return tempptr;
}

static inline struct cn_ib_mr *
_cn_reg_mr(struct rdma_context *ctx, void *addr, size_t length, enum ib_access_flags access, int phy)
{
	struct cn_ib_mr *ret;
	struct ib_mr *proc;

	proc = ctx->proc;

	ret = (struct cn_ib_mr *)kmalloc(sizeof(struct cn_ib_mr), GFP_KERNEL);

	if (phy)
		ret->dma_addr = (void *)cn_reg_mr_phys_addr(ctx, (void *)virt_to_phys(addr), length);
	else
		ret->dma_addr = (void *)ib_dma_map_single((struct ib_device *)ctx->context, addr, length, DMA_BIDIRECTIONAL);

	ret->addr = addr;
	ret->length = length;
	ret->lkey = proc->lkey;
	ret->rkey = proc->rkey;
	ret->node_id = ctx->node_id;
	return ret;
}

static inline struct cn_ib_mr *
cn_reg_mr(struct rdma_context *ctx, void *addr, size_t length, enum ib_access_flags access)
{
	return _cn_reg_mr(ctx, addr, length, access, 0);
}

static inline struct cn_ib_mr *
cn_reg_mr_phy(struct rdma_context *ctx, void *addr, size_t length, enum ib_access_flags access)
{
	return _cn_reg_mr(ctx, addr, length, access, 1);
}

struct rdma_context *_initalize_struct(struct rdma_context *ctx, struct ib_device *ib_dev, int ib_port, int mynodeid)
{
    int	size = 8192;
	int rx_depth = RECV_DEPTH;
    int	ret;

retry_qport:
	ret = ib_query_port(ib_dev, ib_port, &ctx->portinfo);
	if (ret < 0) {
		pr_err("RDMA_FIT_API: Fail to query port\n");
		return NULL;
	}

	pr_info("RDMA_FIT_API: %s() after query CPU%d port: %d LID: %d state: %d\n",
		__func__, smp_processor_id(), ib_port,
		ctx->portinfo.lid, ctx->portinfo.state);

	if (ctx->portinfo.state != IB_PORT_ACTIVE) {
		mdelay(1000);
		pr_info("RDMA_FIT_API: %s() CPU%d port: %d LID: %d state: %d\n",
			__func__, smp_processor_id(), ib_port,
			ctx->portinfo.lid, ctx->portinfo.state);
		goto retry_qport;
	}
	pr_info("RDMA_FIT_API: Query returned LID: %d\n", ctx->portinfo.lid);

	/* This function will create a lot stuff including CQ, QP */
	ctx = cn_init_context(ctx, size, rx_depth, ib_port, ib_dev, mynodeid);
	if (!ctx) {
		pr_err("RDMA_FIT_API: Fail to init ctx\n");
		return NULL;
	}

    return ctx;
}

static int cn_reset_qp(struct rdma_context *ctx, int connection_id, int port, int psn,
			   enum ib_mtu mtu, int sl, int destlid, int destqpn, int destpsn,
               union ib_gid destgid)
{
	struct ib_qp_attr attr = {.qp_state = IB_QPS_RESET};
	if (ib_modify_qp(ctx->qp[connection_id], &attr, IB_QP_STATE))
	{
		printk(KERN_ALERT "RDMA_FIT_API: Fail to reset qp[%d]\n", connection_id);
		return 1;
	}
	return 0;
}

static int _cn_connect_qp(struct rdma_context *ctx, int connection_id, int port, int psn,
			   enum ib_mtu mtu, int sl, int destlid, int destqpn, int destpsn,
               union ib_gid destgid)
{
	int flags = IB_QP_STATE;
	struct ib_qp_attr attr = {
		.qp_state = IB_QPS_INIT,
		.pkey_index = 0,
		.port_num = port,
		.qp_access_flags = IB_ACCESS_REMOTE_WRITE | IB_ACCESS_REMOTE_READ |
							IB_ACCESS_LOCAL_WRITE | IB_ACCESS_REMOTE_ATOMIC,
		.path_mtu = IB_MTU_4096,
		.retry_cnt = 7,
		.rnr_retry = 7};
#ifndef _DISAGG_USE_RC_CONNECTION_
	if (DISAGG_QP_INV_ACK_OFFSET_START <= connection_id && connection_id <= DISAGG_QP_INV_ACK_OFFSET)
		attr.qp_access_flags = IB_ACCESS_REMOTE_WRITE | IB_ACCESS_LOCAL_WRITE;
#endif
	if (ib_modify_qp(ctx->qp[connection_id], &attr,
						IB_QP_STATE |
						IB_QP_PKEY_INDEX |
						IB_QP_PORT |
						IB_QP_ACCESS_FLAGS))
	{
		printk(KERN_ALERT "RDMA_FIT_API: Fail to modify qp[%d]\n", connection_id);
		ib_destroy_qp(ctx->qp[connection_id]);
		return 1;
	}
	pr_rdma("RDMA_FIT_API: modified qp[%d]\n", connection_id);

	memset(&attr, 0, sizeof(attr));
	attr.qp_state = IB_QPS_RTR;
	attr.ah_attr.ah_flags = IB_AH_GRH;
	attr.ah_attr.port_num = port;
	attr.ah_attr.sl = sl;
	attr.path_mtu = mtu;
	attr.dest_qp_num = destqpn;
	attr.rq_psn = destpsn;
	attr.ah_attr.grh.sgid_index = 3; //	//IPv4 & RoCEv2
	attr.ah_attr.grh.dgid = destgid;
	attr.ah_attr.grh.hop_limit = 0xFF;
	attr.ah_attr.type = RDMA_AH_ATTR_TYPE_ROCE;
	flags |= (IB_QP_AV | IB_QP_PATH_MTU | IB_QP_DEST_QPN | IB_QP_RQ_PSN); // UC
#ifndef _DISAGG_USE_RC_CONNECTION_
	if (DISAGG_QP_INV_ACK_OFFSET_START > connection_id || connection_id > DISAGG_QP_INV_ACK_OFFSET)
#endif
	{
		attr.min_rnr_timer = 12;
		attr.max_dest_rd_atomic = MAX_OUTSTANDING_READ; //MAX_OUTSTANDING_SEND
		flags |= (IB_QP_MAX_DEST_RD_ATOMIC | IB_QP_MIN_RNR_TIMER); // RC
	}

	pr_rdma("Try Modify QP (1/2) conn: %d, port: %d, dpsn: %d, dqpn: %d\n",
			  connection_id, port, destpsn, destqpn);

	if (ib_modify_qp(ctx->qp[connection_id], &attr, flags))
	{
		printk(KERN_ALERT "Fail to modify QP to RTR at connection %d\n", connection_id);
		return 1;
	}

	attr.qp_state = IB_QPS_RTS;
	attr.sq_psn	= psn;
	flags = (IB_QP_STATE | IB_QP_SQ_PSN);
#ifndef _DISAGG_USE_RC_CONNECTION_
	if (DISAGG_QP_INV_ACK_OFFSET_START > connection_id || connection_id > DISAGG_QP_INV_ACK_OFFSET)
#endif
	{
		// RC
		// recommanded value from Mellanox manual = 14, time = 4.096 * 2^[timeout] usec
		// 19;	// 2.14 sec -> just longer than inv. ACK timeout (1.0 sec or DISAGG_NET_ACK_TIMEOUT_IN_MS)
		attr.timeout = 17;	// 16
		if (connection_id >= DISAGG_QP_EVICT_OFFSET_START)
		{
			attr.timeout = 15;	// 15: ~ 130 ms -> no harm for very fast retransmission for data push
		}
		attr.retry_cnt = 7;
		attr.rnr_retry = 7;
		attr.max_rd_atomic = MAX_OUTSTANDING_READ; //was 1
		flags |= (IB_QP_TIMEOUT | IB_QP_RETRY_CNT | IB_QP_RNR_RETRY | IB_QP_MAX_QP_RD_ATOMIC);
	}

	if (ib_modify_qp(ctx->qp[connection_id], &attr, flags))
	{
		printk(KERN_ALERT "Fail to modify QP to RTS at connection %d\n", connection_id);
		return 2;
	}

	pr_rdma("Successfully built QP for conn %d destqpn %d\n", connection_id, destqpn);
	return 0;
}

void reset_rdma_qp_to_ready_to_send(struct rdma_context *ctx, int rem_node_id, int conn_id, int reset_first)
{
	int ret = 0;
	int psn = ctx->psn;
	struct dest_info *rem_info = get_node_info(0);
	int ib_port = ctx->ib_port_id;
	pr_info("cur connection %d mynode %d myqpn[%d] %d remnode %d remotelid %d remoteqpn %d\n",
			conn_id, ctx->node_id, conn_id, ctx->qp[conn_id]->qp_num,
			rem_node_id, rem_info->lid, rem_info->qpn[conn_id]);

	if (reset_first)
	{
		ret = cn_reset_qp(ctx, conn_id, ib_port, psn, mtu, sl,
			rem_info->lid, rem_info->qpn[conn_id],
			rem_info->psn, rem_info->gid);
	}

	if (ret)	// error
	{
		return;
	}
	ret = _cn_connect_qp(ctx, conn_id, ib_port, psn, mtu, sl,
			rem_info->lid, rem_info->qpn[conn_id],
			rem_info->psn, rem_info->gid);
	if (ret)
	{
		printk("fail to connect to node %d conn %d\n", rem_node_id, conn_id);
	}
}

static int cn_connect_qp(struct rdma_context *ctx, int rem_node_id, int ib_port)//, int mynodeid)
{
	int i;
	int cur_connection;
	int per_node_conn_idx;

	for (i = 0; i < NUM_PARALLEL_CONNECTION; i++)
	{
		per_node_conn_idx = atomic_read(&ctx->num_alive_connection[rem_node_id]);
		cur_connection = (rem_node_id * ctx->num_parallel_connection) + per_node_conn_idx;
		reset_rdma_qp_to_ready_to_send(ctx, rem_node_id, cur_connection, 0);
		atomic_inc(&ctx->num_alive_connection[rem_node_id]);
		if(atomic_read(&ctx->num_alive_connection[rem_node_id]) == NUM_PARALLEL_CONNECTION)
		{
			atomic_inc(&ctx->num_alive_nodes);
			pr_debug("%s: complete %d connection, MN: %d\n", __func__, NUM_PARALLEL_CONNECTION, rem_node_id);
		}
	}
	return 0;
}

static void initialize_profiling_points(void)
{
#ifdef CONFIG_PROFILING_POINTS
	// initialize profiling points
	_PP_NAME(NET_send_rdma_fit) = _PP_EXPORT_NAME(NET_send_rdma_fit)();
	_PP_NAME(NET_send_rdma_lock) = _PP_EXPORT_NAME(NET_send_rdma_lock)();
#endif
}

static void *inval_buf[DISAGG_QP_NUM_INVAL_BUF] = {NULL};
void *get_inval_buf(int buf_id)
{
	return inval_buf[buf_id];
}

struct rdma_context *cn_conn_switch(struct rdma_context *ctx, struct ib_device *ib_dev, int ib_port, int mynodeid)
{
	int i;
	struct cn_ib_mr *ret_mr;
	int	ret;
    struct dest_info dest;
    struct rdma_context *ret_ctx;
	unsigned long ack_buf[DISAGG_QP_PER_COMPUTE];

	mtu = IB_MTU_4096;
	sl = 0;

	// here, ib_port == ctx->ib_port_id
	ret_ctx = _initalize_struct(ctx, ib_dev, ib_port, mynodeid);
    if (!ret_ctx)
    {
		return NULL;
    }
    ctx = ret_ctx;

	initialize_profiling_points();

	// psn
	get_random_bytes(&ctx->psn, sizeof(ctx->psn));
	ctx->psn &= 0xffffff;
    pr_info("RDMA_FIT_API: PSN: %d\n", ctx->psn);	// not used in controller

    // gid
    rdma_query_gid(ib_dev, ib_port, 3, &ctx->gid);
    pr_info("RDMA_FIT_API: gid at %lx\n", (unsigned long)&ctx->gid);

	// Allocate rmaps for ACKs
	ctx->local_rdma_recv_rings = kmalloc(NUM_PARALLEL_CONNECTION * sizeof(void *), GFP_KERNEL);
	ctx->local_rdma_ring_mrs = kmalloc(NUM_PARALLEL_CONNECTION * sizeof(struct cn_ib_mr), GFP_KERNEL);
	for(i = 0; i < NUM_PARALLEL_CONNECTION; i++)
	{
		unsigned long buf_size = DISAGG_ACK_BUF_SIZE;
		if (DISAGG_QP_INV_ACK_OFFSET_START <= i && i <= DISAGG_QP_INV_ACK_OFFSET)
			buf_size = DISAGG_INV_ACK_CIRC_BUF_SIZE;
		else if(DISAGG_QP_DUMMY_OFFSET_START <= i && i <= DISAGG_QP_DUMMY_OFFSET_END)
			buf_size = DISAGG_NACK_BUF_SIZE;
		ctx->local_rdma_recv_rings[i] = _alloc_memory(buf_size);
		ret_mr = cn_reg_mr_phy(ctx, ctx->local_rdma_recv_rings[i], buf_size,
									IB_ACCESS_LOCAL_WRITE | IB_ACCESS_REMOTE_WRITE | IB_ACCESS_REMOTE_READ);
		memcpy(&ctx->local_rdma_ring_mrs[i], ret_mr, sizeof(struct cn_ib_mr));
		ack_buf[i] = (unsigned long)ctx->local_rdma_ring_mrs[i].dma_addr;
		if (DISAGG_QP_INV_ACK_OFFSET_START <= i && i <= DISAGG_QP_INV_ACK_OFFSET)
		{
			inval_buf[i - DISAGG_QP_INV_ACK_OFFSET_START] = ctx->local_rdma_recv_rings[i];
			pr_info("RDMA_FIT_API: Invalidation buffer[%i] DMA at 0x%lx\n", i, ack_buf[i]);
		}
		else
		{
			pr_info("RDMA_FIT_API: buffer for ACK[%i] DMA at 0x%lx\n", i, ack_buf[i]);
		}
	}

	// send request to controller (=switch)
    {
        int pair_conn_id = i = 0; //i * NUM_PARALLEL_CONNECTION;
		int conn_idx;
		unsigned int qpn_list[NUM_PARALLEL_CONNECTION];

		for (conn_idx = 0; conn_idx < NUM_PARALLEL_CONNECTION; conn_idx++)
		{
			qpn_list[conn_idx] = ctx->qp[pair_conn_id + conn_idx]->qp_num;
		}
		memset(&dest, 0, sizeof(dest));
retry_send_meta:
		ret = send_rdma_meta(ctx->portinfo.lid, ctx->psn,
							qpn_list, (char *)&(ctx->gid), ctx->proc->lkey, ctx->proc->rkey, 
							ack_buf, &dest);
		if (ret)
		{
			msleep(1000);
			pr_debug("RDMA_FIT_API: retry connecting to the server[%d], err: %d\n", i, ret);
			goto retry_send_meta;
        }
        pr_debug("RDMA_FIT_API: TCP handshake with [%d] successed\n", i);

        set_node_info(i, dest.lid, 0, (unsigned int*)dest.qpn, dest.psn, &dest.gid);
        
        printk(KERN_DEFAULT "RDMA_FIT_API: node info updated for [%d]\n", i);
        print_node_info(i);
	}
	ctx->node_id = get_local_node_id();

	for (i = 0; i < NUM_PARALLEL_CONNECTION; i++)
		spin_lock_init(&ctx->conn_lock[i]);

	pr_debug("RDMA_FIT_API: Try to initialize QPs for other nodes\n");
	cn_connect_qp(ctx, 0, ib_port);

	// set callback for RDMA device
	set_rdma_msg_callback(send_msg_via_rdma);
	set_rdma_rmap_callback(cn_reg_mr_ftn);
	set_inval_buf_callback(get_inval_buf);
	barrier();

	// run cache thread
	disagg_cnth_end_init_phase();
	msleep(500);

	// ===== TEST ===== //
	pr_debug("Try to test RDMA read/write/multiplexing\n");
	test_rdma(ctx, &dest);
	pr_debug("Test completed\n");
	// =============== //
	return ctx;
}

/*
 * RDMA read and write
 */
// addr: the address offset, absolute address will be (the base address of the pair_node) + (addr)
__always_inline int rdma_read_data(struct rdma_context *ctx, int pair_node_id, unsigned long dma_addr, u64 addr, unsigned long size, unsigned int rkey)
{
	int connection_id;
	int res;
	char *ack_buf_ptr;

	if (unlikely(!dma_addr))
		return -1;

	connection_id = smp_processor_id();
	// ** WE ALREADY HAVE CPU LOCK **//
	spin_lock(&ctx->conn_lock[connection_id]);
	// set up remote key for ack (to receive RDMA ack)
	rkey |= (ctx->qp[connection_id]->qp_num);
	ack_buf_ptr = ctx->local_rdma_recv_rings[DISAGG_QP_ACK_OFFSET + connection_id];
	memset(ack_buf_ptr, 0, 32);	// just enough — for ACK/NACK

	// SEND actual data
	res = cn_send_message_read(
		ctx, connection_id, rkey,
		(uintptr_t)(addr), dma_addr, size);

	spin_unlock(&ctx->conn_lock[connection_id]);
	return res;
}

__always_inline int rdma_write_data(struct rdma_context *ctx, int pair_node_id, uintptr_t buf, u64 addr, 
unsigned long size, unsigned int rkey, int conn_id, int is_data)
{
	int res, cnt = 0;
	unsigned long start, end, proc_time;
	PROFILE_POINT_TIME(NET_send_rdma_fit)
	PROFILE_POINT_TIME(NET_send_rdma_lock)

	// if (!remote || !buf)
	if (!buf)
		return -1;

	PROFILE_START(NET_send_rdma_lock);
	start = jiffies;
	while (!spin_trylock(&ctx->conn_lock[conn_id]))
	{
		// try next_one
		if (is_data)
		{
			conn_id = DISAGG_QP_EVICT_OFFSET_START + atomic_inc_return(&ctx->next_inval_data_conn) % DISAGG_QP_NUM_INVAL_DATA;
		}else{
			conn_id = DISAGG_QP_INV_ACK_OFFSET_START + atomic_inc_return(&ctx->next_inval_ack_conn) % DISAGG_QP_NUM_INVAL_BUF;
		}

		cnt++;
		if (cnt >= DISAGG_RDMA_POLLING_SKIP_COUNTER)
		{
			cnt = 0;
			usleep_range(50, 50); //TODO: DEBUG
		}
	}
	end = jiffies;
	proc_time = (end > start) ? jiffies_to_usecs(end - start): 0;
	if (unlikely(proc_time >= DISAGG_SLOW_ACK_REPORT_IN_USEC))
	{
		printk(KERN_ERR "ERROR: too much time to hold the lock: conn[%d] addr[0x%lx]\n",
			   conn_id, (unsigned long)addr);
	}
	PROFILE_LEAVE_PTR(NET_send_rdma_lock);

	if (!is_data)
	{
		// set up remote key for ack (to receive RDMA ack)
		rkey |= (ctx->qp[conn_id]->qp_num);
	}

	PROFILE_START(NET_send_rdma_fit);
	res = cn_send_message_write(
		ctx, conn_id, rkey,
		(uintptr_t)(addr), (uintptr_t)(buf), size);
	if (is_data)
		PROFILE_LEAVE_PTR(NET_send_rdma_fit);
	spin_unlock(&ctx->conn_lock[conn_id]);
	return res;
}
