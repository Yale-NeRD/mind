#!/bin/bash

APP_NAME=$1
SRC_DIR=$2
DST_DIR_1=$3
DST_DIR_2=$4
USER=$5
LOG_SERVER=$6
SERVER_ID=$7
SERVER_KEY=$8

PRE=""
POST=""
if [ ${LOG_SERVER} == "memcached_a" ]; then
  PRE="memcached_a_"
  POST="_0"
fi

echo cd ${DST_DIR_1}/;
cd ${DST_DIR_1}/;
mkdir -p ~/sftp_logs
for i in 0 1 2 3 4 5 6 7 8 9; do
  nohup sudo -b sftp -i ${SERVER_KEY} ${USER}@${LOG_SERVER}:${SRC_DIR}/partitioned/${PRE}$(((SERVER_ID-1)*20+i))${POST} > ~/sftp_logs/foo_${i}.out 2> ~/sftp_logs/foo_${i}.err < /dev/null;
  sleep 1
done;
echo cd ${DST_DIR_2}/;
cd ${DST_DIR_2}/;
for i in 10 11 12 13 14 15 16 17 18; do
  nohup sudo -b sftp -i ${SERVER_KEY} ${USER}@${LOG_SERVER}:${SRC_DIR}/partitioned/${PRE}$(((SERVER_ID-1)*20+i))${POST} > ~/sftp_logs/foo_${i}.out 2> ~/sftp_logs/foo_${i}.err < /dev/null;
  sleep 1
done;

# the last entry without nohup
i=19
sudo -b sftp -i ${SERVER_KEY} ${USER}@${LOG_SERVER}:${SRC_DIR}/partitioned/${PRE}$(((SERVER_ID-1)*20+i))${POST} > ~/sftp_logs/foo_${i}.out 2> ~/sftp_logs/foo_${i}.err < /dev/null;
