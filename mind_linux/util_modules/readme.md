# About this module
- When a user inserts this module, it will read and print out numbers recorded by kernel profiler (into kernel's log, e.g., `/var/log/kern.log`).
- To examine the profiled values over time, please remove (`rmmod`) and re-insert (`insmod`) the module.

# How to use this module
```
make
sudo rmmod pprint.ko
sudo insmod pprint.ko
sleep 5
tail -n 20 /var/log/kern.log
```
