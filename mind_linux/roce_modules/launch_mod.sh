#!/bin/bash
sudo insmod roce4disagg.ko
cd ~/Downloads
sudo ethtool --set-priv-flags ens11 sniffer on
sudo tcpdump -i ens11 -s 65535 -w rdma_CN_test.pcap

