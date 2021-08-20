#!/bin/bash
./update_vms.sh
echo "Wait for 15 sec to update hosts"
sleep 15
source ../bricks/g_set_env.sh
CMD="\"cd '$H_SCRIPT_RECIPE_PATH' && pwd && ./01a_vmhost_init_vms.sh 101 102\"" bash -c 'ssh sslee@vmhost1.cloud.cs.yale.internal -t \"$CMD\"' &
CMD="\"cd '$H_SCRIPT_RECIPE_PATH' && pwd && ./01a_vmhost_init_vms.sh 103 104\"" bash -c 'ssh sslee@vmhost2.cloud.cs.yale.internal -t \"$CMD\"' &
CMD="\"cd '$H_SCRIPT_RECIPE_PATH' && pwd && ./01a_vmhost_init_vms.sh 105 106\"" bash -c 'ssh sslee@vmhost3.cloud.cs.yale.internal -t \"$CMD\"' &
CMD="\"cd '$H_SCRIPT_RECIPE_PATH' && pwd && ./01a_vmhost_init_vms.sh 107 108\"" bash -c 'ssh sslee@vmhost5.cloud.cs.yale.internal -t \"$CMD\"' &