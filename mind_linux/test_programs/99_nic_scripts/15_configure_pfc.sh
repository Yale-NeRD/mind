#!/bin/bash
sudo mlnx_qos -i enp59s0 --trust dscp
sudo mlnx_qos -i enp59s0 --pfc 0,0,0,1,0,0,0,0
sudo mlnx_qos -i enp94s0f0 --trust dscp
sudo mlnx_qos -i enp94s0f0 --pfc 0,0,0,1,0,0,0,0
sudo mlnx_qos -i enp94s0f1 --trust dscp
sudo mlnx_qos -i enp94s0f1 --pfc 0,0,0,1,0,0,0,0
sudo mlnx_qos -i enp175s0 --trust dscp
sudo mlnx_qos -i enp175s0 --pfc 0,0,0,1,0,0,0,0
sudo mlnx_qos -i enp216s0f0 --trust dscp
sudo mlnx_qos -i enp216s0f0 --pfc 0,0,0,1,0,0,0,0
sudo mlnx_qos -i enp216s0f1 --trust dscp
sudo mlnx_qos -i enp216s0f1 --pfc 0,0,0,1,0,0,0,0
