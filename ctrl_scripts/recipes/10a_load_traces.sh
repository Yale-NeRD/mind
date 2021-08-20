#!/bin/bash
source ../bricks/g_set_env.sh
if [ $# -lt 1 ]; then
    echo "Please provide trace directory"
    exit
fi
if [ $# -lt 2 ]; then
    echo "Please provide trace name"
    exit
fi

dir_main_disk="/media/data_ssds/"
dir_sub_disk="/media/data_raid0/"
echo "Traces in remote: $1"
echo "Download traces of: $2"
# delete existing traces here
CMD="\"cd '$H_SCRIPT_RECIPE_PATH' && pwd && ./10a1_load_traces_helper.sh 0 9 $1 $dir_main_disk/$2\"" bash -c 'ssh sslee@vmhost1.cloud.cs.yale.internal -t \"$CMD\"'
CMD="\"cd '$H_SCRIPT_RECIPE_PATH' && pwd && ./10a1_load_traces_helper.sh 10 19 $1 $dir_sub_disk/$2\"" bash -c 'ssh sslee@vmhost1.cloud.cs.yale.internal -t \"$CMD\"'
CMD="\"cd '$H_SCRIPT_RECIPE_PATH' && pwd && ./10a1_load_traces_helper.sh 20 29 $1 $dir_main_disk/$2\"" bash -c 'ssh sslee@vmhost2.cloud.cs.yale.internal -t \"$CMD\"'
CMD="\"cd '$H_SCRIPT_RECIPE_PATH' && pwd && ./10a1_load_traces_helper.sh 30 39 $1 $dir_sub_disk/$2\"" bash -c 'ssh sslee@vmhost2.cloud.cs.yale.internal -t \"$CMD\"'
CMD="\"cd '$H_SCRIPT_RECIPE_PATH' && pwd && ./10a1_load_traces_helper.sh 40 49 $1 $dir_main_disk/$2\"" bash -c 'ssh sslee@vmhost3.cloud.cs.yale.internal -t \"$CMD\"'
CMD="\"cd '$H_SCRIPT_RECIPE_PATH' && pwd && ./10a1_load_traces_helper.sh 50 59 $1 $dir_sub_disk/$2\"" bash -c 'ssh sslee@vmhost3.cloud.cs.yale.internal -t \"$CMD\"'
CMD="\"cd '$H_SCRIPT_RECIPE_PATH' && pwd && ./10a1_load_traces_helper.sh 06 69 $1 $dir_main_disk/$2\"" bash -c 'ssh sslee@vmhost5.cloud.cs.yale.internal -t \"$CMD\"'
CMD="\"cd '$H_SCRIPT_RECIPE_PATH' && pwd && ./10a1_load_traces_helper.sh 70 79 $1 $dir_sub_disk/$2\"" bash -c 'ssh sslee@vmhost5.cloud.cs.yale.internal -t \"$CMD\"'
