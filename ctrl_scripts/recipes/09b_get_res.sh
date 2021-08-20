#!/bin/bash
source ../bricks/g_set_env.sh
if [ $# -lt 2 ]; then
    echo "Please provide destination directory"
    exit
fi
echo "Store result file to $1/$2"
if [ $# -lt 3 ]; then
    echo "Please provide number of blades"
    exit
fi
echo "Number of compute blades: $3"
num_node=$3
user_id="sslee"
default_path="/tmp_test/progress.txt"
if [ $num_node -ge 1 ]; then
    CMD="\"mkdir -p $1 && cd $1 && pwd && sftp ${user_id}@192.168.122.101:${default_path} $2_C0\"" bash -c 'ssh sslee@vmhost1.cloud.cs.yale.internal -t \"$CMD\"'
fi
if [ $num_node -ge 2 ]; then
    CMD="\"mkdir -p $1 && cd $1 && pwd && sftp ${user_id}@192.168.122.102:${default_path} $2_C1\"" bash -c 'ssh sslee@vmhost1.cloud.cs.yale.internal -t \"$CMD\"'
fi
if [ $num_node -ge 4 ]; then
    CMD="\"mkdir -p $1 && cd $1 && pwd && sftp ${user_id}@192.168.122.103:${default_path} $2_C2\"" bash -c 'ssh sslee@vmhost2.cloud.cs.yale.internal -t \"$CMD\"'
    CMD="\"mkdir -p $1 && cd $1 && pwd && sftp ${user_id}@192.168.122.104:${default_path} $2_C3\"" bash -c 'ssh sslee@vmhost2.cloud.cs.yale.internal -t \"$CMD\"'
fi
if [ $num_node -ge 8 ]; then
    CMD="\"mkdir -p $1 && cd $1 && pwd && sftp ${user_id}@192.168.122.105:${default_path} $2_C4\"" bash -c 'ssh sslee@vmhost3.cloud.cs.yale.internal -t \"$CMD\"'
    CMD="\"mkdir -p $1 && cd $1 && pwd && sftp ${user_id}@192.168.122.106:${default_path} $2_C5\"" bash -c 'ssh sslee@vmhost3.cloud.cs.yale.internal -t \"$CMD\"'
    CMD="\"mkdir -p $1 && cd $1 && pwd && sftp ${user_id}@192.168.122.107:${default_path} $2_C6\"" bash -c 'ssh sslee@vmhost5.cloud.cs.yale.internal -t \"$CMD\"'
    CMD="\"mkdir -p $1 && cd $1 && pwd && sftp ${user_id}@192.168.122.108:${default_path} $2_C7\"" bash -c 'ssh sslee@vmhost5.cloud.cs.yale.internal -t \"$CMD\"'
fi
