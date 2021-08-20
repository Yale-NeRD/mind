#!/bin/bash
source ../bricks/g_set_env.sh
if [ $# -lt 1 ]; then
    echo "Please provide destination directory"
    exit
fi
echo "Store cdf to $1"
if [ $# -lt 2 ]; then
    echo "Please provide number of blades"
    exit
fi
echo "Number of compute blades: $2"
num_node=$2
if [ $num_node -ge 1 ]; then
    CMD="\"cd '$H_SCRIPT_RECIPE_PATH' && pwd && ./09a1_get_cdf_helper.sh 101 101 $1\"" bash -c 'ssh sslee@vmhost1.cloud.cs.yale.internal -t \"$CMD\"'
fi
if [ $num_node -ge 2 ]; then
    CMD="\"cd '$H_SCRIPT_RECIPE_PATH' && pwd && ./09a1_get_cdf_helper.sh 102 102 $1\"" bash -c 'ssh sslee@vmhost1.cloud.cs.yale.internal -t \"$CMD\"'
fi
if [ $num_node -ge 4 ]; then
    CMD="\"cd '$H_SCRIPT_RECIPE_PATH' && pwd && ./09a1_get_cdf_helper.sh 103 104 $1\"" bash -c 'ssh sslee@vmhost2.cloud.cs.yale.internal -t \"$CMD\"'
fi
if [ $num_node -ge 8 ]; then
    CMD="\"cd '$H_SCRIPT_RECIPE_PATH' && pwd && ./09a1_get_cdf_helper.sh 105 106 $1\"" bash -c 'ssh sslee@vmhost3.cloud.cs.yale.internal -t \"$CMD\"'
    CMD="\"cd '$H_SCRIPT_RECIPE_PATH' && pwd && ./09a1_get_cdf_helper.sh 107 108 $1\"" bash -c 'ssh sslee@vmhost5.cloud.cs.yale.internal -t \"$CMD\"'
fi
