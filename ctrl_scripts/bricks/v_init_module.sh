#!/bin/bash
#$1: id of compute blade starting from 0
#$2: network interface of RoCE NIC
echo "Initialize VM's RoCE module"
source g_set_env.sh

# Linux kernel module for small initrd
export INSTALL_MOD_STRIP=1

cd ${MIND_PATH}/mind_linux/test_programs/99_nic_scripts
# index here is starting from 1 not 0
sh assign_ip_cn_vm$(($1+1)).sh
cd ${MIND_PATH}/mind_linux/roce_modules
sudo mlnx_qos -i $2 --trust dscp
sudo mlnx_qos -i $2 --pfc 0,0,0,1,0,0,0,0
echo 70 | sudo tee /proc/sys/net/core/busy_read
echo 70 | sudo tee /proc/sys/net/core/busy_poll
echo 32768 | sudo tee /proc/sys/net/core/rps_sock_flow_entries
# sudo ip link set dev ens3 mtu 9000
sudo ethtool -C $2 rx-usecs 0
make
sudo insmod roce4disagg.ko
sudo ethtool --set-priv-flags $2 sniffer on

# sudo tcpdump -i ens9 -s 65535 -w ~/Downloads/rdma_CN1_test.pcap
# cd ~/mind/mind_linux/roce_modules
# make
# sudo insmod roce4disagg.ko
exit
