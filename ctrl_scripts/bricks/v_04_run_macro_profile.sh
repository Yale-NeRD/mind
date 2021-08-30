#!/bin/bash
#$1: node id
#$2: number of nodes
#$3: number of thread per blade
#$4: trace type = [tf, gc, ma, mc]
#$5: maximum step (workload specific)
if [ $# -le 4 ]; then
    echo "Error: missing arguments: $@"
fi

LOG_DIR=~/Downloads/04_macro_profile_$4
mkdir -p $LOG_DIR
echo "Run for Node: $1" 
source g_set_env.sh

cd ${MIND_PATH}/mind_linux/test_programs/04_macro_benchmark
pwd
make_cmd="run_$4_$3t_profile"
echo "Log file: ${LOG_DIR}/$4_$1_of_$2_$3t"
echo "Make cmd: ${make_cmd}"
make $make_cmd NODE_ID=$1 NUM_NODE=$2 MAX_PASS=$5 > /dev/null 2>&1
sleep 30
# sudo cp /tmp_test/progress.txt ${LOG_DIR}/progress.$4_$1_of_$2_$3t.log
mkdir -p ${LOG_DIR}/cdf.$4_$1_of_$2_$3t
sudo cp /tmp_test/cdf* ${LOG_DIR}/cdf.$4_$1_of_$2_$3t/.
sudo chgrp -R ${USER} ${LOG_DIR}
