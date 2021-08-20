#!/bin/bash
# $1: start index of VM
# $2: end index of VM
# $3: script to run
source ../bricks/h_set_env.sh
c_id=$1
c_id_start=$1
c_id_end=$2
tar_script=$3
args=""
if [ $# -ge 4 ]; then
    shift 3
    args="$@"
fi
while [ $c_id -ge $c_id_start -a $c_id -le $c_id_end ]
do
    echo "VM: ${c_id}"
    node_id=$(($c_id - 101))
    echo "Args: $args"
	c_id="${c_id}" node_id="${node_id}" CMD="\"export V_SCRIPT_BRICK_PATH=${V_SCRIPT_BRICK_PATH} && cd '$V_SCRIPT_BRICK_PATH' && pwd && ./v_run.sh $tar_script $node_id $args\"" bash -c 'ssh sslee@192.168.122.$c_id -i ~/.ssh/id_rsa_for_vm -tt \"$CMD\"'&
    sleep 5
	(( c_id++ ))
done
