#!/bin/bash
source ../bricks/h_set_env.sh
mkdir -p $4
cd $4
pwd
# set up list of files
for thread_id in {$1..$2}
do
    printf -v thread_id_str "%02d" $thread_id
    source $V_SCRIPT_BRICK_PATH/h_download_files.sh "${user_id}@10.10.10.212" "/tmp_test/cdf_C0${node_id}_T0${thread_id}.txt"
done
