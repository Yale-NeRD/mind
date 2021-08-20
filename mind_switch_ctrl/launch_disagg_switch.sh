#!/bin/sh
PATH=/usr/local/bin:/usr/bin:/bin:/usr/games:${SDE_INSTALL}/bin
P4_PROGRAM_NAME=tna_disagg_switch
P4_PROGRAM_DIR=tna_disagg_switch
P4_PROGRAM_CONF=${SDE_INSTALL}/share/p4/targets/tofino/${P4_PROGRAM_NAME}.conf
echo "P4 program target: ${P4_PROGRAM_NAME}"
sudo env LD_LIBRARY_PATH=/usr/local/lib/ ${P4_PROGRAM_NAME} --install-dir ${SDE_INSTALL} --conf-file ${P4_PROGRAM_CONF}
