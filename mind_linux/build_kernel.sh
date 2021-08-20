#!/bin/bash
echo "Remove kernel logs"
sudo rm /var/log/kern.log
sudo rm /var/log/syslog
echo "Remove cache"
sudo rm .cache.mk
echo "Start"
taskset --cpu-list 0-11 make bzImage -j23 && sudo make install && sudo reboot
