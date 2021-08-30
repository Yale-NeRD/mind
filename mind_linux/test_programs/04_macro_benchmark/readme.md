# Benchmark program for practical workloads
## This is a memory trace replayer which takes memory access log files as an input and replay them

# Setup
- Please set `$NFS_IP` inside `Makefile` to the IP address of the NFS server.
- Inside a VM, we *recommand* users install NFS in the host machine and use it as the NFS server (which is a default setup that we used--with QEMU and its default IP address, `192.168.122.1`)

# Usage
- `make run_[application]_[thread_per_blade] NUM_NODE=[total_blade] NODE_ID=[current_node]`

## Parameters
`application`
- `TF`: TensorFlow, `GC`: GracphChi, `MA`: Memcached w/ YCSB workloada, `MC`: Memcached w/ YCSB workloadc

`thread_per_blade`
- Number of threads per blade, can be `1`, `2`, `4`, or `10`

`total_blade`
- Total number of compute blades

`current_node`
- Id of the current blade, starting from 0 and must be less than `total_blade`

## Example
`make run_tf_10t NUM_NODE=8 NODE_ID=1` represents:
- User tries to run Tensorflow workload
- Each blade will run 10 threads (80 threads in total)
- The current blade is the 2nd compute blades among total 8 blades
