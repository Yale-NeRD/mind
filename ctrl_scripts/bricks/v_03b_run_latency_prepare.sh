#!/bin/bash
# @1: NODE_ID
# @2: state from (shared or modified)
# @3: state to (shared or modified)

if [ $# -le 3 ]; then
    echo "Error: missing arguments: $@"
fi
LOG_DIR=~/Downloads/03b_latency
source g_set_env.sh
cd ${MIND_PATH}/mind_linux/test_programs/03b_state_transition
mkdir -p $LOG_DIR
echo "Run for Node: $1"
pwd

make_cmd="run_$2"
echo "Log file: ${LOG_DIR}/$2_to_$3"
echo "Make cmd: ${make_cmd}"
make $make_cmd NODE_ID=$1 > ${LOG_DIR}/$2_$3 2&>1 &
sleep 240
