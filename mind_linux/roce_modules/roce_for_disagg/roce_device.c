#include "roce_disagg.h"

/* IB related functions */

static void add_device(struct ib_device *dev)
{
    printk(KERN_DEFAULT "RDMA_API: add new RDMA device\n");
    // rdma_ctx.dev = dev;
    set_ib_dev(dev);
    kthread_run((void *)rdma_init, NULL, "rdma_initializer");
}

static void remove_device(struct ib_device *dev, void *client_data)
{
    printk(KERN_DEFAULT "RDMA_API: Remove RDMA device: %u\n", dev->index);
}

static struct ib_client ibv_client = {
    .name = "ibv_server",
    .add = add_device,
    .remove = remove_device
};

int rdma_device_init(void)
{
    int ret = 0;

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
