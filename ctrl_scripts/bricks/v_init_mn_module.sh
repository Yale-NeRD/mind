#!/bin/bash
#$1: id of compute blade starting from 0
#$2: network interface of RoCE NIC
echo "Initialize VM's RoCE module"
source g_set_env.sh

# Linux kernel module for small initrd
export INSTALL_MOD_STRIP=1
cd ${MIND_PATH}/mind_linux/
pwd
# index here is starting from 1 not 0
sh test_programs/99_nic_scripts/assign_ip_mn_vm$(($1+1)).sh
sudo mlnx_qos -i $2 --trust dscp
sudo mlnx_qos -i $2 --pfc 0,0,0,1,0,0,0,0
cd ${MIND_PATH}/mind_linux/mm_modules
make -j4
sudo insmod ns.ko
sudo ethtool --set-priv-flags $2 sniffer on