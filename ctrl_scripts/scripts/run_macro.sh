#!/bin/bash
PROFILE_DIR=~/profile_cdf
EVAL_DIR=~/eval_result
# for num_node in 1 2 4 8
for num_node in 1
do
    # for sharing_ratio in {100..100..25}
    # do
    # echo "Sh: $sharing_ratio, R/W: $rw_ratio"
    cd ../scripts
    ./reset_vms.sh
    echo "Wait for 1 min for reset VMs"
    sleep 60
    cd ../recipes
    # ./02_restart_switch_dataplane.sh
    # ./03_restart_mem_vms.sh
    # ./03a_init_mem_vms.sh
    # ./03b_init_comp_vms.sh
    # sleep 250
    ./09a_get_cdf.sh "${PROFILE_DIR}/ma/t${num_node}0_tso.r1" $num_node
    ./09b_get_res.sh "${EVAL_DIR}/ma/t${num_node}0_prof.r1" "progress" $num_node    # dest folder, file name, num blades
    exit
    # done
done
