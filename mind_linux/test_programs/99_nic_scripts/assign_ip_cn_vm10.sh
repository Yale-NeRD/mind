#!/bin/bash
sudo ip addr add 10.10.10.210/24 dev ens9
sudo ifconfig ens9 mtu 9000
sudo ifconfig ens9 up

# blueflame
export MLX5_POST_SEND_PREFER_BF=1
export MLX5_SHUT_UP_BF=0
unset MLX5_SHUT_UP_BF

# sudo export MLX5_POST_SEND_PREFER_BF=1
# sudo export MLX5_SHUT_UP_BF=0
# sudo unset MLX5_SHUT_UP_BF

# local host and vm
# ping -c 3 10.10.10.101
# sh 13_register_vm_arp.sh ens9
sudo arp -i ens9 -s 10.10.10.1 00:02:00:00:03:00
sudo ibv_devinfo
