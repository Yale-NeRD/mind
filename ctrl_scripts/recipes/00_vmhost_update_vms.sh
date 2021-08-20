#!/bin/bash
# $1: start index of VM
# $2: end index of VM
# initialization
source ../bricks/h_set_env.sh
c_id=$1
MIND_DIR=~/mind
while [ $c_id -ge $1 -a $c_id -le $2 ]
do
    echo "VM: ${c_id}"
    c_id="${c_id}" CMD="\"cd '$V_SCRIPT_ROOT_PATH' && pwd && git pull && cd ${MIND_DIR} && git pull\"" bash -c 'ssh sslee@192.168.122.$c_id -i ~/.ssh/id_rsa_for_vm -t \"$CMD\"'
	(( c_id++ ))
done
