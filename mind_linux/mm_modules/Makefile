OFA_KDIR := /usr/src/ofa_kernel/$(shell uname -r)

ccflags-y := -I$(OFA_KDIR)/include

obj-m += ns.o

ns-objs := ./network_server/network_server.o ./network_server/request_handler.o\
		 ./network_server/network_rdma.o ./network_server/network_rdma_core.o\
		 ./network_server/memory_management.o\
		 ./network_server/request_handler_mem.o

KBUILD_EXTRA_SYMBOLS := /usr/src/linux-headers-$(shell uname -r)/Module.symvers\
			/usr/src/ofa_kernel/$(shell uname -r)/Module.symvers

all:
	 make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
clean:
	 make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
