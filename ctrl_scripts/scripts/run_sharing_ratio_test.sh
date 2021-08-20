#!/bin/bash
# sharing_ratio=0
# rw_ratio=0
for num_node in 1 2 4 8
do
    for sharing_ratio in {100..100..25}
    # for sharing_ratio in {100..100..1}
    do
        for rw_ratio in {0..0..50}
        # for rw_ratio in {0..100..25}
        # for rw_ratio in {50..50..1}
        do
            echo "Sh: $sharing_ratio, R/W: $rw_ratio"
            cd ../scripts
            ./reset_vms.sh
            echo "Wait for 1 min for reset VMs"
            sleep 60
            cd ../recipes
            ./02_restart_switch_dataplane.sh
            ./03_restart_mem_vms.sh
            ./03a_init_mem_vms.sh
            # ./03b_init_comp_vms.sh
            # ./04a_run_sharing_ratio.sh $sharing_ratio $rw_ratio $num_node
            # sleep 250
            exit
        done
    done
done