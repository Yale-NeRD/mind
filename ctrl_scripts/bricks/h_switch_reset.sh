SCREEN_WORSPACE_NAME=run_switch_ctrl_prog
screen -X -S $SCREEN_WORSPACE_NAME quit
# If we want to send signal directly
# screen -S $SCREEN_WORSPACE_NAME -X stuff ^C
sudo killall tna_disagg_switch
# build p4 switch
# cd /home/sslee/sde/bf-sde-9.2.0/pkgsrc/bf-drivers/bf_switchd/bfrt_examples
# make_p4_mind_switch.sh
#
screen -S $SCREEN_WORSPACE_NAME -dm bash -c 'unbuffer ~/launch_disagg_switch.sh > ~/Downloads/experiment.log 2>&1'
echo "Wait for 30 sec for switch program initialization"
sleep 30