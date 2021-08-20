#!/bin/bash

APP_NAME=$1
SRC_DIR=$2
DST_DIR_1=$3
DST_DIR_2=$4
USER=$5
LOG_SERVER=$6
SERVER_ID=$7

echo cd ${DST_DIR_1}${APP_NAME}/;
cd ${DST_DIR_1}${APP_NAME}/;
mkdir -p ~/sftp_logs
for i in 0 1 2 3 4 5 6 7 8 9; do
  nohup sudo -b sftp ${USER}@${LOG_SERVER}:${SRC_DIR}${APP_NAME}_$(((SERVER_ID-1)*20+i))_0 > ~/sftp_logs/foo_${i}.out 2> ~/sftp_logs/foo_${i}.err < /dev/null;
  sleep 1
done;
echo cd ${DST_DIR_2}${APP_NAME}/;
cd ${DST_DIR_2}${APP_NAME}/;
for i in 10 11 12 13 14 15 16 17 18 19; do
  nohup sudo -b sftp ${USER}@${LOG_SERVER}:${LOG_DIR}/partitioned/${APP_NAME}_$(((SERVER_ID-1)*20+i))_0 > ~/sftp_logs/foo_${i}.out 2> ~/sftp_logs/foo_${i}.err < /dev/null;
  sleep 1
done;