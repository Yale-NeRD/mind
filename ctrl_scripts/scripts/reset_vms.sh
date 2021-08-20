#!/bin/bash
source ../bricks/g_set_env.sh
CMD="\"cd '$H_SCRIPT_RECIPE_PATH' && pwd && ./01a_vmhost_reset_vms.sh\"" bash -c 'ssh sslee@vmhost1.cloud.cs.yale.internal -t \"$CMD\"' &
CMD="\"cd '$H_SCRIPT_RECIPE_PATH' && pwd && ./01a_vmhost_reset_vms.sh\"" bash -c 'ssh sslee@vmhost2.cloud.cs.yale.internal -t \"$CMD\"' &
CMD="\"cd '$H_SCRIPT_RECIPE_PATH' && pwd && ./01a_vmhost_reset_vms.sh\"" bash -c 'ssh sslee@vmhost3.cloud.cs.yale.internal -t \"$CMD\"' &
CMD="\"cd '$H_SCRIPT_RECIPE_PATH' && pwd && ./01a_vmhost_reset_vms.sh\"" bash -c 'ssh sslee@vmhost5.cloud.cs.yale.internal -t \"$CMD\"' &
