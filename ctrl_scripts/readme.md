# Script to run programs on *MIND* cluster
This directory contains script to send commands to the servers consising MIND cluster.

## Summary
This readme.md contains the following three benchmarks:

- ![#c5f015](https://via.placeholder.com/15/c5f015/000000?text=+) Performance evaluation with memory traces (Fig. 6 and 8-left)
- ![#c5f015](https://via.placeholder.com/15/c5f015/000000?text=+) Latency measurements for state transision cases (Fig. 7-left)
- ![#c5f015](https://via.placeholder.com/15/c5f015/000000?text=+) Benchmark with various sharing and read/write ratios (Fig. 7-right)

## Cluster setup
![cluster_setup](https://raw.githubusercontent.com/shsym/mind/main/ctrl_scripts/exp_cluster.png)
We prepared experiment profiles (.yaml format) so that you can easily change configuration of the evaluations. Please refer the details below.

## Dependencies
- `python3` (tested with v3.6.9)
- `yaml` and `argparse` package (```pip3 install argparse pyyaml```)
- QEMU / virsh tools and kvm hypervisor

## Prerequisites / assumptions
*If you are using our VMs on CloudLab, you can simply skip this section but please make sure that you have agree with [End-User agreement of Mellanox NIC driver](https://github.com/shsym/mind/tree/main/artifacts#nda-requirements).*
- Control server (i.e., the machine running this script) has ssh keys to access any compute/memory servers in *MIND* cluster.
  - In addition to compute and memory servers, we introduce storage servers to store and fetch memory access traces, if compute servers do not have enough local storage (around 1TB per VM).
- Each compute/memory server has (own) ssh key to access any of its VMs.
- User can run `sudo` wihtout password (can be set up by using `visudo`).
  - For compute and memory servers, user can run `sudo virsh` without password.
  - For compute and memory blade VMs, user can run any command via `sudo` without password.
- All servers and VMs have this repository at `~/mind` (please clone this repository).

## Usage / cluster setup
- Please update cluster configuration located at `scripts/config.yaml`.
  - Cluster setup for CloudLab is pre-configured, so please skip this step.
  - Example setup is based on 4 compute server (2 compute blade VMs per server), 1 memory server (1 memory blade VM per server), and 1 programmable switch.
  - If storage server is needed, location of memory traces can be also configured in this configuration file.

- Run the main script `run_commands.py` with the profile corresponding to the test.

- When each task of the profile is finished (see below for more details), output from servers/VMs will be updated at `scripts/run.log`

## ![#c5f015](https://via.placeholder.com/15/c5f015/000000?text=+) Performance evaluation with memory traces
***We assume that you are in this directory `mind/ctrl_scripts/` on the experiment management server***

### Prepare traces
```
python3 scripts/run_commands.py --profile profiles/05_load_trace.yaml
```
- First, run the script with the profile `05_load_trace.yaml` to load memory access traces from the storage server.
  - You can choose which application traces to load in the profile—please modify both lines: [here](https://github.com/shsym/mind/blob/5da9130db51f4da10fd4b84d64ae1f01dc008fb9/ctrl_scripts/scripts/profiles/05_load_trace.yaml#L38) and [here](https://github.com/shsym/mind/blob/5da9130db51f4da10fd4b84d64ae1f01dc008fb9/ctrl_scripts/scripts/profiles/05_load_trace.yaml#L45)
    - tf: TensorFlow, gc: GraphChi, ma: Memcached w/ YCSB workloadA, mc: Memcached w/ YCSB workloadC

### How to run the benchmark
- ***Each experiment would take a few hours to run (usually 2 to 5 hours per data point). Please modify `04_macro_bench.yaml` to reduce the running time by limiting number of steps***
  - Update [this line](https://github.com/shsym/mind/blob/153c0d1fe2ed089e7f6b984dafadb8de507c7cd9/ctrl_scripts/scripts/profiles/04_macro_profile.yaml#L33) to a smaller value than that we used.
    - The values we used for complete memory traces are specified in the next lines
- (Fig. 6 and 8-left, default setup: Memcached with YCSB workloada)
  - Please setup `04_macro_bench.yaml` to have the same application you loaded: [here](https://github.com/shsym/mind/blob/0a5911fb939b15f3b9975f89bf23f08d756c26cb/ctrl_scripts/scripts/profiles/04_macro_bench.yaml#L30), [here](https://github.com/shsym/mind/blob/0a5911fb939b15f3b9975f89bf23f08d756c26cb/ctrl_scripts/scripts/profiles/04_macro_bench.yaml#L47), and [here](https://github.com/shsym/mind/blob/0a5911fb939b15f3b9975f89bf23f08d756c26cb/ctrl_scripts/scripts/profiles/04_macro_bench.yaml#L55)
```
python3 scripts/run_commands.py --profile profiles/04_macro_bench.yaml
```
- Result from compute blade VMs will be placed in `~/Downloads/04_macro_bench_[APP]` (see `04_macro_bench.yaml` for details)
  - Please run [this script](https://github.com/shsym/mind/blob/main/ctrl_scripts/scripts/post_processing/04macro_bench_res.sh) for calculating final numbers from the logs
    ```bash
    scripts/post_processing/04macro_bench_res.sh
    ```
  - Result representation: how final numbers are calculated
    - Inside the files `progress.[APP]_[BLADE ID]_of_[NUM OF BLADES]_[#THREADS PER BLADE].log`, `Time [1234566789]: ...` at the beginning of each line shows the highest value among threads. We used the highest value among blades (e.g., the slowest thread among 80 threads).
    - We calculated the inverse of the time as a performance (i.e., 1 / [the highest time value]), then the performance values are normalized by comparing agains MIND's result with 1 and 10 threads for Fig. 6 left and right, respectively (Fig. 6 itself also shows which data point is the base of the normalization).
- Result from switch will be placed at `~/Download/latest.log`
  - A new result will override any previous result having the same filename.
  - Inside the log file, each line `23:07:02:512201, 7473, 1` represents `[TIMESTAMP], [#FREE DIRECTORY ENTRIES], [SPLIT/MERGE THRESHOLD]`

### Comparison with other systems
- For GAM, please check [this repository](https://github.com/charles-typ/mind_ae_gam)
- For FastSwap, please check [this repository](https://github.com/yyppyy/cfm)

### Measure CDF for PSO/PSO+ estimation
- Assuming that the memory access trace files are already loaded, please run
```
python3 scripts/run_commands.py --profile=profiles/04_macro_profile.yaml
```
- Result CDF files will be placed at `~/Downloads/04_macro_profile_[APP]/`
  - Each sub directory `cdf.[APP]_[BLADE ID]_of_[NUM OF BLADES]_[#THREADS PER BLADE]` represents CDF collected from each compute blade 

## ![#c5f015](https://via.placeholder.com/15/c5f015/000000?text=+) Latency measurements for state transision cases
- (Fig. 7-left, default setup: shared state to modified state, 8 compute blades)
  - Update [the corresponding profile `03b_latency.yaml`](https://github.com/shsym/mind/blob/153c0d1fe2ed089e7f6b984dafadb8de507c7cd9/ctrl_scripts/scripts/profiles/03b_latency.yaml#L35-L36) to test various state transition scenarios (shared (S) to S, modified (M) to S, S to M, and M to M, with 2, 4 and 8 blades.)
    - Idle to S/M can be tested by setting the number of blade as 1 (i.e., profile setup with S to S/M and 1 blade = transition from idle to S/M)
    - Since only one blade can be in M (exclusive write permission), we used only 2 blades for the transition from M to S/M
```
python3 scripts/run_commands.py --profile profiles/03b_latency.yaml
```
- Result will be placed in `~/Downloads/03b_latency`
  - The name `shared_to_modified_total_8_blades.log` represent the transition from `Shared` to `Modified` states for `8` blades (7 sharer to 1 writer).
  - Inside the file, `FH_fetch_remote_tot` shows network latency
  - Inside the file, `FH_ack_waiting_node` shows latency for waiting ACK/invalidation

## ![#c5f015](https://via.placeholder.com/15/c5f015/000000?text=+) Benchmark with various sharing and read/write ratios
- (Fig. 7-right, default setup: sharing ratio = 50%, read ratio = 50%)
  - Modify [the profile `03_sharing_ratio.yaml`](https://github.com/shsym/mind/blob/153c0d1fe2ed089e7f6b984dafadb8de507c7cd9/ctrl_scripts/scripts/profiles/03_sharing_ratio.yaml#L30-L31) to test various sharing and read ratios
```
python3 scripts/run_commands.py --profile profiles/03_sharing_ratio.yaml
```
- Result will be placed in `~/Downloads/03a_sharing_ratio`
  - The name `res_2_sr050_rw050.log` presents it was from the 3rd blade (id=`2`), and sharing ratio was `50%` and read ratio was `50%`.
  - Inside the file, the last line shows 4KB IOPS. We used the sum over 8 blades.

## (Re-)Build MIND kernel
```
python3 scripts/run_commands.py --profile profiles/01_restart_vms.yaml
```
- This profile will clone this repo (which will print warnings because our VMs already have the repo), build kernel, and restart VMs.

## Test programs (source code)
- Test programs used by this script is located in [here](https://github.com/shsym/mind/tree/main/mind_linux/test_programs) with detailed information. 

## Details of experiment profile files
Experiment profiles are located in `scripts/profiles` and each profile consists of task blocks. The main script `run_commands.py` loads and parses the given profile then runs corresponding tasks in order.
- Example: sharing ratio experiment block in `03_sharing_ratio.yaml`
```yaml
- name: run sharing ratio
  job: sharing_ratio
  per_command_delay: 10
  post_delay: 0
  job_args:
    sharing ratio: 50
    rw ratio: 50
    node num: 8
# target_vms:
# - 0
```
- By default, this command will be sent to all compute blade VMs. Please use `target_vms` tag to limit the target VMs that will run the test (commented in this example).
- `job` is the name of task that the main script recognizes.
- `per_command_delay` is a delay between sequential commands in the same task block and for different VMs (e.g., delay between sending a command to VM1 and VM2).
- `post_delay` is a wait time before moving on to the next task block
- `job_args` is a task-specific arguments block. In this example, we can set `sharing ratio` (=0 to 100), `rw ratio` (=read ratio, 0 to 100), and `node num` (=number of total compute blade VMs).
