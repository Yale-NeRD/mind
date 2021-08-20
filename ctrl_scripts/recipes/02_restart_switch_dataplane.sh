#!/bin/bash
source ../bricks/h_set_env.sh
CMD="\"cd '$V_SCRIPT_BRICK_PATH' && source ./h_switch_env.sh && ./h_switch_reset.sh\"" bash -c 'ssh sslee@switch-onl -t \"$CMD\"'
