#!/bin/bash
source ../bricks/g_set_env.sh
num_node=8
if [ $# -ge 3 ]; then
    num_node=$3
fi
echo "Run script for $num_node compute blades"
if [ $num_node -ge 1 ]; then
    CMD="\"cd '$H_SCRIPT_RECIPE_PATH' && pwd && ./01b_vmhost_run_vms.sh 101 101 ./v_03a_run_sharing_ratio.sh $1 $2 $num_node\"" bash -c 'ssh sslee@vmhost1.cloud.cs.yale.internal -t \"$CMD\"'
fi
if [ $num_node -ge 2 ]; then
    CMD="\"cd '$H_SCRIPT_RECIPE_PATH' && pwd && ./01b_vmhost_run_vms.sh 102 102 ./v_03a_run_sharing_ratio.sh $1 $2 $num_node\"" bash -c 'ssh sslee@vmhost1.cloud.cs.yale.internal -t \"$CMD\"'
fi
if [ $num_node -ge 4 ]; then
    CMD="\"cd '$H_SCRIPT_RECIPE_PATH' && pwd && ./01b_vmhost_run_vms.sh 103 104 ./v_03a_run_sharing_ratio.sh $1 $2 $num_node\"" bash -c 'ssh sslee@vmhost2.cloud.cs.yale.internal -t \"$CMD\"'
fi
if [ $num_node -ge 8 ]; then
    CMD="\"cd '$H_SCRIPT_RECIPE_PATH' && pwd && ./01b_vmhost_run_vms.sh 105 106 ./v_03a_run_sharing_ratio.sh $1 $2 $num_node\"" bash -c 'ssh sslee@vmhost3.cloud.cs.yale.internal -t \"$CMD\"'
    CMD="\"cd '$H_SCRIPT_RECIPE_PATH' && pwd && ./01b_vmhost_run_vms.sh 107 108 ./v_03a_run_sharing_ratio.sh $1 $2 $num_node\"" bash -c 'ssh sslee@vmhost5.cloud.cs.yale.internal -t \"$CMD\"'
fi
