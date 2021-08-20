#!/bin/bash
if [ $# -le 3 ]; then
    echo "Error: missing arguments: $@"
fi
LOG_DIR=~/Downloads/03a_sharing_ratio
source g_set_env.sh
cd ${MIND_PATH}/mind_linux/test_programs/03a_micro_benchmark_shared
mkdir -p $LOG_DIR
echo "Run for Node: $1"
pwd

make_cmd="run_shared_$2_$3"
# echo "Log file: ${LOG_DIR}/$2_$3"
echo "Make cmd: ${make_cmd}"
make $make_cmd NODE_ID=$1 NUM_NODE=$4 > /dev/null 2>&1 &
sleep 240
cp logs_03a_sharing_ratio/* ${LOG_DIR}/.
cd ${MIND_PATH}/mind_linux/util_modules
make
sudo rmmod pprint.ko
sudo insmod pprint.ko
sleep 5
tail -n 100 /var/log/kern.log >> ${LOG_DIR}/kern.node_$1_of_$4_sr_$2_rw_$3.log
