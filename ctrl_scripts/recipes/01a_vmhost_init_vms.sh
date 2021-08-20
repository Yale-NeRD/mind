#!/bin/bash
# $1: start index of VM
# $2: end index of VM
# $3: script to run
source ../bricks/h_set_env.sh
c_id=$1
while [ $c_id -ge $1 -a $c_id -le $2 ]
do
    echo "VM: ${c_id}"
    node_id=$(($c_id - 101))
    c_id="${c_id}" node_id="${node_id}" CMD="\"export V_SCRIPT_BRICK_PATH=${V_SCRIPT_BRICK_PATH} && cd '$V_SCRIPT_BRICK_PATH' && pwd && ./v_run.sh ./v_init.sh $node_id\"" bash -c 'ssh sslee@192.168.122.$c_id -i ~/.ssh/id_rsa_for_vm -tt \"$CMD\"'
    sleep 5
	(( c_id++ ))
done
