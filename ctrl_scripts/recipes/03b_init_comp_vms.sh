#!/bin/bash
source ../bricks/g_set_env.sh
CMD="\"cd '$H_SCRIPT_RECIPE_PATH' && pwd && ./01b_vmhost_run_vms.sh 101 102 ./v_init_module.sh\"" bash -c 'ssh sslee@vmhost1.cloud.cs.yale.internal -t \"$CMD\"'
sleep 10
CMD="\"cd '$H_SCRIPT_RECIPE_PATH' && pwd && ./01b_vmhost_run_vms.sh 103 104 ./v_init_module.sh\"" bash -c 'ssh sslee@vmhost2.cloud.cs.yale.internal -t \"$CMD\"'
sleep 10
CMD="\"cd '$H_SCRIPT_RECIPE_PATH' && pwd && ./01b_vmhost_run_vms.sh 105 106 ./v_init_module.sh\"" bash -c 'ssh sslee@vmhost3.cloud.cs.yale.internal -t \"$CMD\"'
sleep 10
CMD="\"cd '$H_SCRIPT_RECIPE_PATH' && pwd && ./01b_vmhost_run_vms.sh 107 108 ./v_init_module.sh\"" bash -c 'ssh sslee@vmhost5.cloud.cs.yale.internal -t \"$CMD\"'
sleep 10
echo "Wait for 30 sec for connecting all blades..."
sleep 30