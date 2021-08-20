#include "network_server.h"
#include "network_rdma.h"
#include "memory_management.h"

#include <asm/byteorder.h>

/* Exposed array used by FIT code */
static int nr_joined_nodes = 0;

/*
 * If we can not get the CQE within 20 seconds
 * There should be something wrong.
 */
#define FIT_POLL_CQ_TIMEOUT_NS (20000000000L)

/* message related functions */
static inline void hlt(void)
{
	asm volatile(
		"1: hlt\n\t"
		"jmp 1b\n\t");
}

inline uintptr_t
cn_reg_mr_phys_addr(struct rdma_context *ctx, void *addr, size_t length)
{
	struct ib_device *ibd = (struct ib_device *)ctx->context;
	return (uintptr_t)phys_to_dma(ibd->dma_device, (phys_addr_t)addr);
}

inline uintptr_t
cn_reg_mr_addr(struct rdma_context *ctx, void *addr, size_t length)
{
	uintptr_t dma_addr =
		(uintptr_t)ib_dma_map_single((struct ib_device *)ctx->context,
									 addr, length, DMA_BIDIRECTIONAL);
	if (ib_dma_mapping_error((struct ib_device *)ctx->context, dma_addr))
		return (uintptr_t)NULL;

	return dma_addr;
}

inline void
cn_unmap_mr_addr(struct rdma_context *ctx, u64 addr, size_t length)
{
	ib_dma_unmap_single((struct ib_device *)ctx->context, addr, length, DMA_BIDIRECTIONAL);
}

/* Threading related functions */
int get_joined_nodes(void)
{
	return (nr_joined_nodes);
}
