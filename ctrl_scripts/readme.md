# Script to run programs on *MIND* cluster
This directory contains script to send commands to the servers consising MIND cluster.
![cluster_setup](https://raw.githubusercontent.com/shsym/mind/main/ctrl_scripts/exp_cluster.png)

## Dependencies
- `python3`
- `yaml` and `argparse` package (```pip3 install argparse pyyaml```)
- QEMU / virsh tools and kvm hypervisor

## Prerequisites / assumptions
- Control server (i.e., the machine running this script) has ssh keys to access any compute/memory servers in *MIND* cluster.
  - In addition to compute and memory servers, we introduce storage servers to store and fetch memory access traces, if compute servers do not have enough local storage (around 1TB per VM).
- Each compute/memory server has (own) ssh key to access any of its VMs.
- User can run `sudo` wihtout password (can be set up by using `visudo`).
  - For compute and memory servers, user can run `sudo virsh` without password.
  - For compute and memory blade VMs, user can run any command via `sudo` without password.
- All servers and VMs have this repository at `~/mind`.

## Usage
- Please update cluster configuration located at `scripts/config.yaml`.
  - Example setup is based on 4 compute server (2 compute blade VMs per server), 1 memory server (1 memory blade VM per server), and 1 programmable switch.
  - If storage server is needed, location of memory traces can be also configured in this configuration file.

- Run the main script `run_commands.py` with the profile corresponding to the test.

## Performance evaluation with memory traces
```
python3 scripts/run_commands.py --profile profiles/05_load_trace.yaml
```
- First run the script with the profile `05_load_trace.yaml` to load memory access traces from the storage server.

```
python3 scripts/run_commands.py --profile profiles/04_macro_bench.yaml
```
- (Fig. 6 and 8-right, default setup: Memcached with YCSB workloada)
  - Please setup `04_macro_bench.yaml` for other workloads and configurations (tf: TensorFlow, gc: GraphChi, ma and mc: Memcached with YCSB workload A and C)
- Result from compute blade VMs will be placed in `~/Downloads/04_macro_bench_[TRACE/APP]` (see `04_macro_bench.yaml` for details)
  - Inside the files `progress.ma_[BLADE ID]_of_[NUM OF BLADES]_[#THREADS PER BLADE].log`, `Time [1234566789]` shows the highest value among threads. We used the highest value among blades (e.g., the slowest thread among 80 threads).
- Result from switch will be placed at `~/Download/latest.log`
  - Inside the log file, each line `23:07:02:512201, 7473, 1` represents `[TIMESTAMP], [#FREE DIRECTORY ENTRIES], [SPLIT/MERGE THRESHOLD]`

### Comparison with other systems
- For GAM, please check [this repository](https://github.com/charles-typ/mind_ae_gam)
- For FastSwap, please check [this repository](https://github.com/yyppyy/cfm)

## Latency measurements for state transision cases
```
python3 scripts/run_commands.py --profile profiles/03b_latency.yaml
```
- (Fig. 7-left, default setup: shared state to modified state, 8 compute blades)
- Result will be placed in `~/Downloads/03b_latency`
  - The name `shared_to_modified_total_8_blades.log` represent it was transition from Shared to Modified states for 8 blades (7 sharer to 1 writer).
  - Inside the file, `FH_fetch_remote_tot` shows network latency
  - Inside the file, `FH_ack_waiting_node` shows latency for waiting ACK/invalidation

## Benchmark with various sharing and read/write ratios
```
python3 scripts/run_commands.py --profile profiles/03_sharing_ratio.yaml
```
- (Fig. 7-right, default setup: sharing ratio = 50%, read ratio = 50%)
- Result will be placed in `~/Downloads/03a_sharing_ratio`
  - The name `res_2_sr050_rw050.log` presents it was from the 3rd blade (id=2), and sharing ratio was 50% and read ratio was 50%.
  - Inside the file, the last line shows 4KB IOPS. We used sum over 8 blades.

## Test programs 
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
