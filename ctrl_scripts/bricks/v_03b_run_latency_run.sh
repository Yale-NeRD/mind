#!/bin/bash
# @1: NODE_ID
# @2: state from (shared or modified)
# @3: state to (shared or modified)
# @4: number of total nodes (only used for logging)

if [ $# -le 4 ]; then
    echo "Error: missing arguments: $@"
fi
LOG_DIR=~/Downloads/03b_latency
source g_set_env.sh
cd ${MIND_PATH}/mind_linux/test_programs/03b_state_transition
mkdir -p $LOG_DIR
echo "Run for Node: $1"
pwd

make_cmd="run_$3"
echo "Log file: ${LOG_DIR}/$2_to_$3_total_$4_blades"
echo "Make cmd: ${make_cmd}"
sleep 120
make $make_cmd NODE_ID=$1 > /dev/null 2>&1 &
sleep 60
cd ${MIND_PATH}/mind_linux/util_modules
make
sudo rmmod pprint.ko
sudo insmod pprint.ko
sleep 5
tail -n 20 /var/log/kern.log >> ${LOG_DIR}/$2_to_$3_total_$4_blades.log
