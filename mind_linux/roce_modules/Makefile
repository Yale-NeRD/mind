OFA_KDIR := /usr/src/ofa_kernel/default
# OFED_KDIR := /usr/src/mlnx-ofed-kernel-5.0

# EXTRA_CFLAGS    := -DXX_KMOD_DEF
# EXTRA_CFLAGS    += -I$(OFA_KDIR)/include
# EXTRA_CFLAGS    += -I$(OFED_KDIR)/include
EXTRA_CFLAGS	+= -D__CN_ROCE__ #-D__CN_ROCE_TEST__

ccflags-y := -I$(OFA_KDIR)/include

obj-m += roce4disagg.o

roce4disagg-objs := ./roce_for_disagg/roce_disagg.o ./roce_for_disagg/roce_rdma.o\
					./roce_for_disagg/roce_device.o ./roce_for_disagg/network_fit_disagg.o\
					./roce_for_disagg/network_fit_core_disagg.o ./roce_for_disagg/roce_tests.o
			#  ./controller/network_rdma.o ./controller/network_rdma_core.o\
			#  ./controller/thpool.o\
			#  ./controller/memory_management.o ./controller/mmap_ftns.o\
			#  ./controller/rbtree_ftns.o ./controller/sys_x86_64.o\
			#  ./controller/fault.o ./controller/file_handler.o\
			#  ./controller/memory_node_connect.o
# KBUILD_EXTRA_SYMBOLS := /usr/src/linux-headers-$(shell uname -r)/Module.symvers
KBUILD_EXTRA_SYMBOLS := $(shell pwd)/Module.symvers\
			/usr/src/ofa_kernel/$(shell uname -r)/Module.symvers

# make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
# make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

all:
	@echo $(KBUILD_EXTRA_SYMBOLS)
	@echo $(EXTRA_CFLAGS)
	make -C ../. M=$(PWD) modules

clean:
	make -C ../. M=$(PWD) clean
