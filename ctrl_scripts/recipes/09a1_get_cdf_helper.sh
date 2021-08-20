#!/bin/bash
source ../bricks/h_set_env.sh
mkdir -p $3
cd $3
pwd
# set up list of files
c_id=$1
c_id_start=$1
c_id_end=$2
user_id="sslee"
while [ $c_id -ge $c_id_start -a $c_id -le $c_id_end ]
do
    echo "VM: ${c_id}"
    node_id=$(($c_id - 101))
    for thread_id in {0..9}
    do
        source $V_SCRIPT_BRICK_PATH/h_download_files.sh "${user_id}@192.168.122.${c_id}" "/tmp_test/cdf_C0${node_id}_T0${thread_id}.txt"
    done
	(( c_id++ ))
done
