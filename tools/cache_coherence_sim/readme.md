## Introduction
This repo contains a MIND simulator that runs on memory access traces prepared [here](https://github.com/shsym/mind_ae/tree/master/tools/prepare_traces)

## Summary
- ![#c5f015](https://via.placeholder.com/15/c5f015/000000?text=+) We have the following example scripts: `run_tf.sh`, `run_gc.sh`, `run_ma.sh`, and `run_mc.sh`
- Since it may take too much time to run simulation (up to 2 days per application, espeically for memcached w/ YCSB workloadA), please also find our pre-computed output logs in [here](https://github.com/shsym/mind/tree/main/tools/cache_coherence_sim/bounded_split_eval)
  - Please refer the last line of the log. Each column means:
    - The cumulative number of false invalidations,
    - Falsely written backed pages,
    - Directory entries at the end the current resizing epoch, respectively.
    - Please refer our description for `stat.[#blades]n_[#threads per blade]t...log` file below.

## Get Started
To run the simulator, you need to have the trace files ready first, which means they need to be stored
under one directory altogether and named from 0 to (# cores -1), then please use:
```shell
$ ./simulate.sh your_trace_file_directory_path num_nodes num_cores cache_line_size dir_block_size cache_size tot_mem interval_of_cache_resizing performance_target_coefficient degree_of_split
```
For example, with trace files stored as `/home/me/remapped/0` ... `/home/me/remapped/79`, the following command:
```shell
$ ./simulate.sh /home/me/remapped/ 8 80 4096 1048576 536870912 4294967296 10 1 1 300000
```
simulates a cluster with `8` nodes that each has `10` cores (total `80`).
- The cache line size is 4KB (`4096`). 
- The initial directory entry size is 1MB (`1048576`).
- The local cache size for each node is 512MB (`536870912`)
- The maximum memory footprint size (highest address minus lowest address) of the trace is 4GB (`4294967296`), which is large enough for applications we studied.
- The following `10` indicates that the directory entry is resized every 10 timewindows with resizing enabled.
- The last `300000` represents the maximum number of directory entries (Note that if the initial directory entry size is set to too small value, the initial number of directory entries can be larger than this value. The bounded splitting algorithm will merge directory entries to make the number of entries less than given maximum number of entries).

## Understand the output
After the simulator terminates, inside a directory `logs`, you should see directories named `pso`, `cdf`, `rwcnt` and log files named `stat.*` and `progress.*`, which are used for MIND's PSO simulation and sensitivity investigation of the bounded splitting algorithm.
- `pso` stores a log file used for PSO simulation for each core. Each line is populated at each time a read happens.
The content of an entry starts with the address of the cache line, then followed with 4 numbers, describing difference between TSO and PSO, consisting of how many local reads, local writes, remote reads and remote writes has happened between the privous remote write to the same cache line and the current local read, resepctively.

- `cdf` stores CDF of read and write accesses. Logs in this directory will be used for PSO/PSO+ simulation

- `rwcnt` stores number of reads and writes ([local read], [local write], [remote read], [remote writes] per line). Logs in this directory will be used for PSO simulation.

- ![#c5f015](https://via.placeholder.com/15/c5f015/000000?text=+) Entries in `stat.[#blades]n_[#threads per blade]t...log` are generated at each time the resizing happens. The entries contain 3 numbers each, indicating the cumulative number of false invalidations, falsely written backed pages, and directory entries at the end the current resizing epoch.
This result is collected to examine to effectivity of MIND's bounded directory entry splitting algorithm.

- Entries in `progress.[#blades]n_[#threads per blade]t...log` contain Invalidation statistics (number of invalidations and invalidated pages) and number of read/write accesses.

## Configure the simulator
Besides the parameters passed to the simulator, Other reconfiguraions can also be done by editing `./simulator.hpp`.
For example, you can undefine `DYNAMIC_RESIZE` to diable adaptive directory entry resizing, or reset `TIMEWINDOW_US` to change the time window size.
