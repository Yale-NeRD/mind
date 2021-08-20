#!/bin/bash
echo "Initialize VM's RoCE module"
source ~/.init_cn.sh
cd ~/mind_ae/mind_linux/roce_modules
make
sudo insmod roce4disagg.ko
exit
