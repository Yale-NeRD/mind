#!/bin/bash
./update_hosts.sh
echo "Wait for 15 sec to update hosts"
sleep 15
source ../bricks/g_set_env.sh
CMD="\"cd '$H_SCRIPT_RECIPE_PATH' && pwd && ./00_vmhost_update_vms.sh 101 102\"" bash -c 'ssh sslee@vmhost1.cloud.cs.yale.internal -t \"$CMD\"' &
CMD="\"cd '$H_SCRIPT_RECIPE_PATH' && pwd && ./00_vmhost_update_vms.sh 103 104\"" bash -c 'ssh sslee@vmhost2.cloud.cs.yale.internal -t \"$CMD\"' &
CMD="\"cd '$H_SCRIPT_RECIPE_PATH' && pwd && ./00_vmhost_update_vms.sh 105 106\"" bash -c 'ssh sslee@vmhost3.cloud.cs.yale.internal -t \"$CMD\"' &
CMD="\"cd '$H_SCRIPT_RECIPE_PATH' && pwd && ./00_vmhost_update_vms.sh 107 108\"" bash -c 'ssh sslee@vmhost5.cloud.cs.yale.internal -t \"$CMD\"' &
sleep 15
