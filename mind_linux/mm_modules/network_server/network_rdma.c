#include "network_server.h"
#include "network_rdma.h"
#include "memory_management.h"
#include <linux/io.h>

static struct rdma_context *mind_roce_ctx;
static struct ib_device *ibapi_dev;
static struct dest_info node_infos[DISAGG_MAX_NODE];
static int connected_nodes = 0;

/* 
 * IB related functions
 */
static void add_device(struct ib_device *dev)
{
	printk(KERN_DEFAULT "RDMA_API: add new RDMA device\n");
	ibapi_dev = dev;
	kthread_run((void *)rdma_init, NULL, "rdma_initializer");
}

static void remove_device(struct ib_device *dev, void *client_data)
{
	printk(KERN_DEFAULT "RDMA_API: Remove RDMA device: %u\n", dev->index);
}

static struct ib_client ibv_client = {
	.name = "ibv_server",
	.add = add_device,
	.remove = remove_device};

int rdma_library_init(void)
{
	int ret = 0;
	int i = 0;

	// initialize variables
	for (i = 0; i < DISAGG_MAX_NODE; i++)
	{
		node_infos[i].node_id = -1;
	}
	connected_nodes = 0;

	// setup client
	ret = ib_register_client(&ibv_client);
	if (ret)
	{
		printk(KERN_ERR "RDMA_API: Couldn't register IB client\n");
		return ret;
	}
	else
	{
		printk(KERN_DEFAULT "RDMA_API: IB client is registered\n");
	}
	return 0;
}

/* 
 * Infomation of the other nodes and TCP message handler
 */

static void gid_to_wire_gid(const union ib_gid *gid, char *wgid)
{
	char tmp_gid[sizeof(DISAGG_RDMA_GID_FORMAT)];
	int i;

	memcpy(tmp_gid, gid, sizeof(union ib_gid));
	for (i = 0; i < sizeof(union ib_gid); ++i)
		sprintf(&wgid[i * 2], "%02x", tmp_gid[i] & 0xff);
	wgid[32] = '\0';
}

unsigned int get_node_global_lid(unsigned int nid)
{
	return node_infos[nid].lid;
}

unsigned int get_node_first_qpn(unsigned int nid)
{
	return node_infos[nid].qpn[0];
}

struct dest_info *set_node_info(unsigned int nid, int lid, u8 *mac, unsigned int *qpn, int psn, union ib_gid *gid)
{
	int i;
	node_infos[nid].node_id = nid;
	node_infos[nid].lid = lid;
	memcpy(node_infos[nid].mac, mac, sizeof(u8) * ETH_ALEN);
	for (i = 0; i < NUM_PARALLEL_CONNECTION; i++)
		node_infos[nid].qpn[i] = qpn[i];
	node_infos[nid].psn = psn;
	memcpy((void *)&node_infos[nid].gid, (void *)gid, sizeof(*gid));
	return get_node_info(nid);
}

struct dest_info *get_node_info(unsigned int nid)
{
	return &node_infos[nid];
}

void print_node_info(unsigned int nid)
{
	char gid_char[sizeof(DISAGG_RDMA_GID_FORMAT) + 1] = {0};
	gid_to_wire_gid(&node_infos[nid].gid, gid_char);
	pr_debug("DMA_FIT_API - node info[%d]: lid: %d, qpn[0]: %d, psn: %d, gid: %s\n",
			  nid, node_infos[nid].lid, node_infos[nid].qpn[0], node_infos[nid].psn, gid_char);
}

// It will be called when the meta data is sent from computing nodes
int handle_rdma_init(struct mem_header *hdr, void *payload, struct socket *sk, int id)
{
	struct rdma_msg_struct *rdma_req = (struct rdma_msg_struct *)payload;
	struct rdma_msg_struct *reply;
	struct dest_info local_info;
	int ret = -1;
	int nid = rdma_req->node_id;
	char gid_char[sizeof(DISAGG_RDMA_GID_FORMAT) + 1] = {0};
	int i;

	if (nid < DISAGG_MAX_NODE && nid >= 0)
	{
		if (node_infos[nid].node_id >= 0)
		{
			pr_info("RDMA_INIT: reuse the existing node id: %d\n", nid);
		}
		set_node_info(nid, rdma_req->lid, rdma_req->mac, rdma_req->qpn, rdma_req->psn,
					  (union ib_gid *)(void *)rdma_req->gid); // copy the gid anyway assuming same endian
		ret = 0;											  // No error
	}

	// create reply
	reply = kmalloc(sizeof(*reply), GFP_KERNEL);
	if (!reply)
	{
		ret = -1;
		goto rdma_init_release;
	}
	rdma_get_handshake_data(nid, &local_info);
	reply->ret = ret;
	reply->node_id = local_info.node_id;
	reply->lid = local_info.lid;
	reply->psn = local_info.psn;
	for (i = 0; i < NUM_PARALLEL_CONNECTION; i++)
		reply->qpn[i] = local_info.qpn[i];
	reply->lkey = 0;
	reply->rkey = 0;
	memset(reply->mac, 0, sizeof(u8) * ETH_ALEN);
	memset(reply->gid, 0, sizeof(reply->gid));
	memcpy(reply->gid, (void *)&local_info.gid, min(sizeof(reply->gid), sizeof(local_info.gid)));
	gid_to_wire_gid(&local_info.gid, gid_char);
	barrier();

	pr_info("RDMA_INIT: handshake with node[%d] local - lid: %d, psn: %d, qpn[0]: %d, gid: %s\n",
			nid, local_info.lid, local_info.psn, local_info.qpn[0], gid_char);

	tcp_server_send(sk, id, (const char *)reply, sizeof(*reply), MSG_DONTWAIT);
	ret = 0; // must return 0 if it sent reply
	connected_nodes++;

rdma_init_release:
	if (reply)
		kfree(reply);
	return ret;
}

/* 
 * Initialization (local context and connect to other nodes)
 */

static struct rdma_context *cn_init_context(struct rdma_context *ctx, int size, int rx_depth, int port,
						 struct ib_device *ib_dev, int mynodeid)
{
	int i;
	int num_total_connections = MAX_CONNECTION;
	int com_node_id;
	struct ib_cq_init_attr cq_attr = {};

	ctx->node_id = mynodeid;
	ctx->send_flags = IB_SEND_SIGNALED;
	ctx->rx_depth = rx_depth;
	ctx->num_connections = num_total_connections;
	ctx->num_node = DISAGG_MAX_NODE;
	ctx->num_parallel_connection = NUM_PARALLEL_CONNECTION;
	ctx->context = (struct ib_context *)ib_dev;

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

	ctx->num_alive_connection = kmalloc(ctx->num_node * sizeof(atomic_t), GFP_KERNEL);
	atomic_set(&ctx->num_alive_nodes, 1);
	memset(ctx->num_alive_connection, 0, ctx->num_node * sizeof(atomic_t));
	for (i = 0; i < ctx->num_node; i++)
		atomic_set(&ctx->num_alive_connection[i], 0);

	atomic_set(&ctx->num_completed_threads, 0);

	ctx->cq = kmalloc(NUM_POLLING_THREADS * sizeof(struct ib_cq *), GFP_KERNEL);
	if (!ctx->cq)
	{
		pr_err("RDMA_FIT_API: OOM - couldn't allocate CQ\n");
		return NULL;
	}

	for (i = 0; i < NUM_POLLING_THREADS; i++)
	{
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

#ifdef CONFIG_SOCKET_O_IB
	ctx->sock_send_cq = (struct ib_cq **)kmalloc(DISAGG_MAX_NODE * sizeof(struct ib_cq *), GFP_KERNEL);
	ctx->sock_qp = (struct ib_qp **)kmalloc(DISAGG_MAX_NODE * sizeof(struct ib_qp *), GFP_KERNEL);
	ctx->sock_recv_cq = ib_create_cq((struct ib_device *)ctx->context, NULL, NULL, NULL, rx_depth + 1, 0);
	BUG_ON(!ctx->sock_send_cq || !ctx->sock_recv_cq || !ctx->sock_qp);
#endif

	for (i = 0; i < num_total_connections; i++)
	{
		struct ib_qp_attr attr;

		memset(&attr, 0, sizeof(attr));
		com_node_id = i / NUM_PARALLEL_CONNECTION;
		pr_debug("RDMA_FIT_API: mynodeid %d i %d connecting node %d\n", ctx->node_id, i, com_node_id);
		if (com_node_id == ctx->node_id)
			continue;

		/*
		 * XXX
		 * why rx_depth+1 ???
		 */
		cq_attr.cqe = rx_depth + 1;
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
			init_attr.cap.max_send_wr = MAX_OUTSTANDING_SEND;
			init_attr.cap.max_recv_wr = rx_depth;
			init_attr.cap.max_inline_data = MAX_INLINED_DATA;
			init_attr.cap.max_send_sge = 16;
			init_attr.cap.max_recv_sge = 16;
			init_attr.qp_type = IB_QPT_RC;
			init_attr.sq_sig_type = IB_SIGNAL_REQ_WR;

			//align_first_qpn(ctx->pd, &init_attr);
			ctx->qp[i] = ib_create_qp(ctx->pd, &init_attr);
			if (IS_ERR_OR_NULL(ctx->qp[i]))
			{
				printk(KERN_ERR "RDMA_FIT_API: Fail to create qp[%d]. Error: %d",
					   i, PTR_ERR_OR_ZERO(ctx->qp[i]));
				return NULL;
			}

			ib_query_qp(ctx->qp[i], &attr, IB_QP_CAP, &init_attr);
			if (init_attr.cap.max_inline_data >= size)
				ctx->send_flags |= IB_SEND_INLINE;
		}

		{
			struct ib_qp_attr attr1 = {
				.qp_state = IB_QPS_INIT,
				.pkey_index = 0,
				.port_num = port,
				.qp_access_flags = IB_ACCESS_REMOTE_WRITE | IB_ACCESS_REMOTE_READ |
								   IB_ACCESS_LOCAL_WRITE | IB_ACCESS_REMOTE_ATOMIC,
				.path_mtu = IB_MTU_4096,
				.retry_cnt = 7,
				.rnr_retry = 7};

			if (ib_modify_qp(ctx->qp[i], &attr1,
							 IB_QP_STATE |
							 IB_QP_PKEY_INDEX |
							 IB_QP_PORT |
							 IB_QP_ACCESS_FLAGS))
			{
				printk(KERN_ALERT "RDMA_FIT_API: Fail to modify qp[%d]\n", i);
				ib_destroy_qp(ctx->qp[i]);
				return NULL;
			}
		}
		printk(KERN_DEFAULT "RDMA_FIT_API: modified qp[%d]\n", i);
	}
	return ctx;
}

enum ib_mtu mtu;
static int sl;

void *_alloc_memory(unsigned int length)
{
	void *tempptr;
	tempptr = kmalloc(length, GFP_KERNEL); //Modify from kzalloc to kmalloc
	if (!tempptr)
		printk(KERN_CRIT "%s: alloc error\n", __func__);
	return tempptr;
}

static inline struct cn_ib_mr *
cn_reg_mr(struct rdma_context *ctx, void *addr, size_t length, enum ib_access_flags access, int isPhy)
{
	struct cn_ib_mr *ret;
	struct ib_mr *proc;

	proc = ctx->proc;
	ret = (struct cn_ib_mr *)kmalloc(sizeof(struct cn_ib_mr), GFP_KERNEL);

	if (isPhy)
	{
		struct ib_device *ibd = (struct ib_device *)ctx->context;
		ret->dma_addr = (void *)phys_to_dma(ibd->dma_device, (phys_addr_t)addr);
	}
	else
	{
		// DMA address
		ret->dma_addr = (void *)ib_dma_map_single((struct ib_device *)ctx->context, addr, length, DMA_BIDIRECTIONAL);		
	}
	// Original PA / VA address mapped to DMA address
	ret->addr = addr;

	if (ib_dma_mapping_error((struct ib_device *)ctx->context, (u64)ret->dma_addr))
		return NULL;

	pr_info("Assigned addr for ib_reg: 0x%lx\n", (unsigned long)ret->addr);

	ret->length = length;
	ret->lkey = proc->lkey;
	ret->rkey = proc->rkey;
	ret->node_id = ctx->node_id;
	return ret;
}

struct rdma_context *_initalize_struct(struct rdma_context *ctx, struct ib_device *ib_dev, int ib_port, int mynodeid)
{
	int size = 8192;
	int rx_depth = RECV_DEPTH;
	int ret;

retry_qport:
	ret = ib_query_port(ib_dev, ib_port, &ctx->portinfo);
	pr_info("RDMA_FIT_API: %s() after query CPU%d port: %d LID: %d state: %d\n",
			__func__, smp_processor_id(), ib_port,
			ctx->portinfo.lid, ctx->portinfo.state);

	if (ret < 0)
	{
		pr_err("RDMA_FIT_API: Fail to query port\n");
		return NULL;
	}

	if (ctx->portinfo.state != IB_PORT_ACTIVE)
	{
		mdelay(1000);
		pr_info("RDMA_FIT_API: %s() CPU%d port: %d LID: %d state: %d\n",
				__func__, smp_processor_id(), ib_port,
				ctx->portinfo.lid, ctx->portinfo.state);
		goto retry_qport;
	}
	pr_info("RDMA_FIT_API: Query returned LID: %d\n", ctx->portinfo.lid);

	/* This function will create a lot stuff including CQ, QP */
	ctx = cn_init_context(ctx, size, rx_depth, ib_port, ib_dev, mynodeid);
	if (!ctx)
	{
		pr_err("RDMA_FIT_API: Fail to init ctx\n");
		return NULL;
	}

	// ctx->mn_mapped_mem = ioremap(MEMORY_BASE_LOCATION, MEMORY_TOTAL_SIZE);
	// ctx->mn_mapped_mem = memremap(MEMORY_BASE_LOCATION, MEMORY_TOTAL_SIZE, MEMREMAP_WB);
	ctx->mn_mapped_mem = (void *)MEMORY_BASE_LOCATION;
	set_base_addr((unsigned long)ctx->mn_mapped_mem, MEMORY_TOTAL_SIZE);
	printk(KERN_DEFAULT "Mapped area: 0x%lx\n", (unsigned long)ctx->mn_mapped_mem);

	return ctx;
}

static int _cn_connect_qp(struct rdma_context *ctx, int connection_id, int port, int psn,
						   enum ib_mtu mtu, int sl, int destlid, int destqpn, int destpsn,
						   union ib_gid destgid)
{
	struct ib_qp_attr attr;
	int flags = IB_QP_STATE;

	memset(&attr, 0, sizeof(attr));
	attr.qp_state = IB_QPS_RTR;
	attr.ah_attr.ah_flags = IB_AH_GRH;
	attr.ah_attr.port_num = port;
	attr.ah_attr.sl = sl;
	attr.path_mtu = mtu;
	attr.dest_qp_num = destqpn;
	attr.rq_psn = destpsn;
	attr.max_dest_rd_atomic = MAX_OUTSTANDING_READ; //MAX_OUTSTANDING_SEND
	attr.min_rnr_timer = 12;
	attr.ah_attr.grh.sgid_index = 3; 	// IPv4 & RoCEv2
	attr.ah_attr.grh.dgid = destgid;
	attr.ah_attr.grh.hop_limit = 0xFF;
	attr.ah_attr.type = RDMA_AH_ATTR_TYPE_ROCE;

	flags |= (IB_QP_AV | IB_QP_PATH_MTU | IB_QP_DEST_QPN | IB_QP_RQ_PSN |
			  IB_QP_MAX_DEST_RD_ATOMIC | IB_QP_MIN_RNR_TIMER); // RC

	pr_debug("Try Modify QP (1/2) conn: %d, port: %d, dpsn: %d, dqpn: %d\n",
			  connection_id, port, destpsn, destqpn);

	if (ib_modify_qp(ctx->qp[connection_id], &attr, flags))
	{
		printk(KERN_ALERT "Fail to modify QP to RTR at connection %d\n", connection_id);
		return 1;
	}

	attr.qp_state = IB_QPS_RTS;
	attr.timeout = 21;
	attr.retry_cnt = 7;
	attr.rnr_retry = 7;
	attr.sq_psn = psn;
	attr.max_rd_atomic = MAX_OUTSTANDING_READ; //was 1

	flags = IB_QP_STATE;
	flags |= (IB_QP_TIMEOUT | IB_QP_RETRY_CNT | IB_QP_RNR_RETRY |
			  IB_QP_SQ_PSN | IB_QP_MAX_QP_RD_ATOMIC);

	if (ib_modify_qp(ctx->qp[connection_id], &attr, flags))
	{
		printk(KERN_ALERT "Fail to modify QP to RTS at connection %d\n", connection_id);
		return 2;
	}

	pr_debug("connected conn %d destqpn %d\n", connection_id, destqpn);
	return 0;
}

static int cn_connect_qp(struct rdma_context *ctx, int rem_node_id, int ib_port, int mynodeid)
{
	int i;
	int ret;
	int cur_connection;
	int psn = ctx->psn;
	int per_node_conn_idx;
	struct dest_info *rem_info = get_node_info(rem_node_id);

	for (i = 0; i < NUM_PARALLEL_CONNECTION; i++)
	{
		per_node_conn_idx = atomic_read(&ctx->num_alive_connection[rem_node_id]);
		cur_connection = (rem_node_id * ctx->num_parallel_connection) + per_node_conn_idx;
		pr_debug("cur connection %d mynode %d myqpn[%d] %d remnode %d remotelid %d remoteqpn %d\n",
				cur_connection, ctx->node_id, cur_connection, ctx->qp[cur_connection]->qp_num,
				rem_node_id, rem_info->lid, rem_info->qpn[per_node_conn_idx]);

	retry:
		ret = _cn_connect_qp(ctx, cur_connection, ib_port, psn, mtu, sl,
				rem_info->lid, rem_info->qpn[per_node_conn_idx],
				rem_info->psn, rem_info->gid);
		if (ret)
		{
			printk("fail to connect to node %d conn %d\n", rem_node_id, i);
			msleep(1000);
			goto retry;
		}

		atomic_inc(&ctx->num_alive_connection[rem_node_id]);
		if (atomic_read(&ctx->num_alive_connection[rem_node_id]) == NUM_PARALLEL_CONNECTION)
		{
			atomic_inc(&ctx->num_alive_nodes);
		}
	}

	pr_info("***  Successfully built QP for node %2d [LID: %d QPN[0]: %d]\n",
			rem_node_id, get_node_global_lid(rem_node_id),
			get_node_first_qpn(rem_node_id));

	return 0;
}

static int mn_send_rdma_meta(unsigned int lid, unsigned int psn, unsigned int* qpn, char *gid,
							 unsigned int lkey, unsigned int rkey,
							 struct dest_info *dest, u64 mapped_mem, u64 mapped_size)
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
	payload.node_type = DISAGG_RDMA_MEMORY_TYPE;
	payload.addr = mapped_mem;
	payload.size = mapped_size;
	payload.lid = lid;
	payload.psn = psn;
	for (i = 0; i < NUM_PARALLEL_CONNECTION; i++)
		payload.qpn[i] = qpn[i];
	payload.lkey = lkey;
	payload.rkey = rkey;
	memset(payload.mac, 0, sizeof(u8) * ETH_ALEN);
	memcpy(payload.gid, gid, gid_size);

	ret = send_one_msg_to_ctrl(DISAGG_RDMA, &payload, sizeof(payload),
							   reply, sizeof(struct rdma_msg_struct));
	if (ret < 0 || ret < (int)sizeof(payload))
		ret = -EINTR;
	else if (reply->ret)
	{					  // only 0 is success
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
			memcpy(dest->mac, reply->mac, sizeof(u8) * ETH_ALEN);
			dest->psn = reply->psn;
			for (i = 0; i < NUM_PARALLEL_CONNECTION; i++)
				dest->qpn[i] = reply->qpn[i];
			memset((void *)&(dest->gid), 0, sizeof(dest->gid));
			memcpy((void *)&(dest->gid), reply->gid, min(sizeof(dest->gid), sizeof(reply->gid)));
		}
		// set up local data about controller
		set_node_info(reply->node_id, reply->lid, reply->mac, reply->qpn, reply->psn,
						(union ib_gid *)(void *)reply->gid); // copy the gid anyway assuming same endian
		set_local_node_id(reply->node_id_res);
	}
	kfree(reply);
	return ret;
}

// same name but different implementation for the memory side
void rdma_init(void)
{
	int n_port = 1;
	struct cn_ib_mr *ret_mr;
	struct rdma_context *ret_ctx;
	int ret;
	int i;
	struct dest_info local_info;
	//TODO: we assume that id of controller is 0
	//      and send all metadata to the controller
	int pair_node_id = 0;
	char gid_str[sizeof(DISAGG_RDMA_GID_FORMAT)];
	unsigned long mmap_test_size = 0x4000;

	msleep(DISAGG_RDMA_INIT_SLEEP_TIME_IN_MS);

	mtu = IB_MTU_4096;
	sl = 0;

	mind_roce_ctx = kzalloc(sizeof(struct rdma_context), GFP_KERNEL);
	if (!mind_roce_ctx)
		return;

	ret_ctx = _initalize_struct(mind_roce_ctx, ibapi_dev, n_port, get_local_node_id());
	if (!ret_ctx)
	{
		if (mind_roce_ctx)
			kfree(mind_roce_ctx);
		return;
	}
	mind_roce_ctx = ret_ctx;

	// psn
	get_random_bytes(&mind_roce_ctx->psn, sizeof(mind_roce_ctx->psn));
	mind_roce_ctx->psn &= 0xffffff;
	pr_info("RDMA_FIT_API: PSN: %d\n", mind_roce_ctx->psn);

	// gid
	rdma_query_gid(ibapi_dev, n_port, 3, &mind_roce_ctx->gid);	//IPv4 & RoCEv2
	gid_to_wire_gid(&mind_roce_ctx->gid, gid_str);
	pr_info("RDMA_FIT_API: gid at %lx [%s]\n",
			(unsigned long)&mind_roce_ctx->gid, gid_str);

	// Report its meta-data to the controller
	rdma_get_handshake_data(pair_node_id, &local_info);
	barrier();

	if (mind_roce_ctx->mn_mapped_mem) // should be initialized in _initalize_struct()
	{
		mmap_test_size = MEMORY_TOTAL_SIZE;

		ret_mr = cn_reg_mr(mind_roce_ctx, mind_roce_ctx->mn_mapped_mem, mmap_test_size,
							   IB_ACCESS_LOCAL_WRITE | IB_ACCESS_REMOTE_WRITE | IB_ACCESS_REMOTE_READ,
							   1);

		pr_info("cn_reg_mr assigned: 0x%lx\n", (unsigned long)ret_mr);
		if (!ret_mr)
		{
			pr_err("Cannot allocate memory region\n");
			BUG();
		}
		memcpy(&mind_roce_ctx->mn_mapped_mem_mr, ret_mr, sizeof(struct cn_ib_mr));
		printk(KERN_DEFAULT "Memory mapped to the NIC at 0x%lx [original addr: 0x%lx]\n",
			   (unsigned long)mind_roce_ctx->mn_mapped_mem_mr.dma_addr,
			   (unsigned long)mind_roce_ctx->mn_mapped_mem_mr.addr);
	}

retry_send_meta:
	ret = mn_send_rdma_meta(mind_roce_ctx->portinfo.lid, mind_roce_ctx->psn,
							local_info.qpn,
							(char *)&local_info.gid, mind_roce_ctx->proc->lkey, mind_roce_ctx->proc->rkey,
							NULL,
							// (u64)(mind_roce_ctx->mn_mapped_mem_mr.addr),
							(u64)(mind_roce_ctx->mn_mapped_mem_mr.dma_addr),
							(u64)mmap_test_size);
	if (ret)
	{
		msleep(5000);
		pr_info("RDMA_FIT_API: retry connecting to the controller, err: %d\n", ret);
		goto retry_send_meta;
	}
	pr_info("Sent data to controller\n");

	/*
	 * Allocate and register local RDMA-IMM rings for all nodes
	 */
	mind_roce_ctx->local_rdma_recv_rings = kmalloc(DISAGG_MAX_NODE * sizeof(void *), GFP_KERNEL);
	mind_roce_ctx->local_rdma_ring_mrs = kmalloc(DISAGG_MAX_NODE * sizeof(struct cn_ib_mr), GFP_KERNEL);
	for (i = 0; i < DISAGG_MAX_NODE; i++)
	{
		mind_roce_ctx->local_rdma_recv_rings[i] = _alloc_memory(IMM_PORT_CACHE_SIZE);
		ret_mr = cn_reg_mr(mind_roce_ctx, mind_roce_ctx->local_rdma_recv_rings[i], IMM_RING_SIZE,
							   IB_ACCESS_LOCAL_WRITE | IB_ACCESS_REMOTE_WRITE | IB_ACCESS_REMOTE_READ, 0);
		memcpy(&mind_roce_ctx->local_rdma_ring_mrs[i], ret_mr, sizeof(struct cn_ib_mr));
	}

	// We do not need to wait, we can only connect to the controller
	pr_debug("RDMA_FIT_API: Try to initialize QPs for other nodes\n");
	mind_roce_ctx->node_id = get_local_node_id();
	for (i = 0; i < DISAGG_MAX_NODE; i++)
	{
		if (i == DISAGG_MEMORY_NODE_ID)	// itself or controller (no other nodes in its scope)
			continue;
		cn_connect_qp(mind_roce_ctx, i, n_port, get_local_node_id());
	}
	pr_debug("RDMA initialized\n");
	return;
}

void free_rdma(void)
{
	if (mind_roce_ctx)
		kfree(mind_roce_ctx);
	mind_roce_ctx = NULL;
	// TODO: need to free other resources in ctx (e.g., QPs, DMAs)
}

void rdma_get_handshake_data(int pair_node_id, struct dest_info *local_info)
{
	int i;
	int pair_conn_id = pair_node_id * NUM_PARALLEL_CONNECTION;
	if (local_info && mind_roce_ctx)
	{
		local_info->node_id = get_local_node_id();
		local_info->lid = mind_roce_ctx->portinfo.lid;
		local_info->psn = mind_roce_ctx->psn;
		for (i = 0; i < NUM_PARALLEL_CONNECTION; i++)
			local_info->qpn[i] = mind_roce_ctx->qp[pair_conn_id + i]->qp_num;
		memcpy((void *)&local_info->gid, (void *)&mind_roce_ctx->gid, sizeof(local_info->gid));
	}
}
