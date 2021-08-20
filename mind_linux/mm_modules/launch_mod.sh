#!/bin/bash
sudo insmod ns.ko
cd ~/Downloads
sudo ethtool --set-priv-flags ens10 sniffer on
sudo tcpdump -i ens10 -s 65535 -w rdma_MN_test.pcap
