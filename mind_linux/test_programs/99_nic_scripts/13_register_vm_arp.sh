#!/bin/bash

# switch controller
sudo arp -s -i $1 10.10.10.1 00:02:00:00:03:00
ping -c 3 10.10.10.1

# = VMs =
# computing node VMs
sudo arp -s -i $1 10.10.10.201 04:3f:72:a2:b4:a2
sudo arp -s -i $1 10.10.10.202 04:3f:72:a2:b4:a3
sudo arp -s -i $1 10.10.10.203 04:3f:72:a2:b5:f2
sudo arp -s -i $1 10.10.10.204 04:3f:72:a2:b5:f3
sudo arp -s -i $1 10.10.10.205 0c:42:a1:41:8b:5a
sudo arp -s -i $1 10.10.10.206 0c:42:a1:41:8b:5b
sudo arp -s -i $1 10.10.10.207 04:3f:72:a2:b0:12
sudo arp -s -i $1 10.10.10.208 04:3f:72:a2:b0:13
sudo arp -s -i $1 10.10.10.209 0c:42:a1:41:8a:92
sudo arp -s -i $1 10.10.10.210 0c:42:a1:41:8a:93
sudo arp -s -i $1 10.10.10.211 04:3f:72:a2:b4:3a
sudo arp -s -i $1 10.10.10.212 04:3f:72:a2:b4:3b

# memory node VMs
sudo arp -s -i $1 10.10.10.221 04:3f:72:a2:b7:3a
sudo arp -s -i $1 10.10.10.222 04:3f:72:a2:c5:32
