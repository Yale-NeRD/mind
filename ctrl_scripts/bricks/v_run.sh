$V_SCRIPT_BRICK_PATH/g_update_this_repo.sh
echo "** Run script: $1"
echo "** Node ID: $2"
SCREEN_WORKSPACE_NAME=run_script
# clean previous session
if ( screen -ls | grep $SCREEN_WORKSPACE_NAME > /dev/null); then
    screen -X -S $SCREEN_WORKSPACE_NAME quit;
fi

# new sessions
args="$@"
SCREEN_COMMAND="cd ${V_SCRIPT_BRICK_PATH} && pwd && $args > ~/Downloads/experiment.log"
#  2>&1"
echo "** Cmd: $SCREEN_COMMAND"
screen -S $SCREEN_WORKSPACE_NAME -dm unbuffer bash -c "$SCREEN_COMMAND"
sleep 5
