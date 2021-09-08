# Guide to run experiments in Yale cluster
This directory contains command lines with minimal description. Please check more details in [artifact evaluation guide](https://github.com/shsym/mind/tree/main/artifacts)

## Summary
We will run the following experiments:
- ![#c5f015](https://via.placeholder.com/15/c5f015/000000?text=+) Performance evaluation with memory traces (Fig. 6 and 8-left)
- ![#c5f015](https://via.placeholder.com/15/c5f015/000000?text=+) Latency measurements for state transision cases (Fig. 7-left)
- ![#c5f015](https://via.placeholder.com/15/c5f015/000000?text=+) Benchmark with various sharing and read/write ratios (Fig. 7-right)

Generating each data point will take 5~25 minutes.

## Access to the control server
We assume thet you already have Yale VPN following our instructions in hotcrp.
- **IMPORTANT** please reserve your time slot first using the link in hotcrp; since we have only one cluster with a programmable switch, each evaluator should have own dedicated time slot.
- *For your anonymity, please do not use your google account when you access any Google docs we provide*

After start Yale VPN session (i.e., login on Cisco Anyconnect)
```bash
ssh -i [PATH TO THE SSH KEY] sosp_ae@ecl-mem-01.cs.yale.internal
```
- Please find ssh key from hotcrp.
- Please do not remove repository in the control server; it will also remove any precomputed input/log files we set up on the servers.

Once you log in, the default directory is set to the [control script directory](https://github.com/shsym/mind/tree/main/mind_switch_ctrl) of MIND repository:
```bash
$ pwd
/home/sosp_ae/mind/ctrl_scripts
```

You can check the current status of git repository
```bash
$ git remote -v
origin	https://github.com/shsym/mind.git (fetch)
origin	https://github.com/shsym/mind.git (push)

$ git status
(some result here)

$ git log
(some result here)
```

If some files were modified by previous evaluator, please reset the repository by
```bash
git reset --hard HEAD
git pull
```

Please tell switch that you are going to run MIND
```bash
python3 run_commands.py --profile=profiles/02_setup_mind_switch.yaml
```

## ![#c5f015](https://via.placeholder.com/15/c5f015/000000?text=+) Performance evaluation with memory traces (Fig. 6 and 8-left)

Let's go inside the script directory and load memory access traces for Tensorflow
```bash
cd scripts
python3 run_commands.py --profile=profiles/05_load_trace_tf.yaml
```
The script will print out raw input (i.e., ssh commands to the servers) and standard output.
- Since we need to download more than 1TB of data, this will take some time up to one hour.

After the script for loading traces is finished, we can run the following command to run an experiment with the TensorFlow memory traces we just loaded:
```bash
python3 run_commands.py --profile profiles/04_macro_bench_tf.yaml
```
- By default, it will run only 1/10 of the total traces with 2 compute blades; it will take 10 ~ 20 minutes.
  - Please modify the values in `profiles/04_macro_bench_tf.yaml` for test various setup

    ```yaml
    - name: run macro benchmark
        job: macro_bench
        per_command_delay: 10
        post_delay: 0
        job_args:
        trace: tf         # Tensorflow workload
        node num: 2       # <- modify this value to set the number of compute blades [1, 2, 4, 8]
        thread_num: 10    # <- modify this value to set the number of threads per blade [1, 2, 4, 10]
        step_num: 5000    # <- increase this value to run more portion of the traces
    # step_num used in the paper ()
    # - Tensorflow or tf: 50000
    # - GraphChi or gc: 50000
    # - Memcached YCSB workloadA or ma: 35000
    # - Memcached YCSB workloadA or mc: 20000
    ```
The result of the experiment will be downloaded at `~/Downloads/04_macro_bench_[APP]`
- [APP]: `tf` for Tensorflow, `gc` for GraphChi, `ma` / `mc` for Memcached with YCSB workloadA/workloadC

To compute the final number of the result, please run
```bash
cd post_processing && ./04macro_bench_res.sh && cd ..
```
- This script scan will scan through the directories for all the applications and number of compute blades and calculate normalized computing speed.
  - The value is calculated by (total amount of task / processing time): [actual code](https://github.com/shsym/mind/blob/8cf7e8baa05bd2489ad3058437d06acd92c8aa43/ctrl_scripts/scripts/post_processing/04macro_bench.py#L54)

Result from the switch will be placed at `~/Download/latest.log`
  - A new result will override any previous result having the same filename.
  - Inside the log file, each line  `23:07:02:512201, 7473, 1`

    represents `[TIMESTAMP], [#FREE DIRECTORY ENTRIES], [SPLIT/MERGE THRESHOLD]`
    - We used/set total number of entires as 30,000

We can also test traces from other applications, for example
```bash
python3 run_commands.py --profile=profiles/05_load_trace_gc.yaml
python3 run_commands.py --profile profiles/04_macro_bench_gc.yaml
```

### Comparison with other systems
- For GAM, please check [this repository](https://github.com/charles-typ/mind_ae_gam)
- For FastSwap, please check [this repository](https://github.com/yyppyy/cfm)

---

## ![#c5f015](https://via.placeholder.com/15/c5f015/000000?text=+) Latency measurements for state transision cases (Fig. 7-left)
If you tested other systems, please ensure that switch knows about the type of the system—MIND in this case:
```bash
python3 run_commands.py --profile=profiles/02_setup_mind_switch.yaml
```

Now we run latency measurements from idle to modified (M; exclusive, write-able) state
```bash
python3 run_commands.py --profile profiles/03b_latency_I_to_M.yaml
```
- Result will be placed in `~/Downloads/03b_latency` (will replace any previous result)
  - The name `[shared]_to_[modified]_total_[1]_blades.log` represents the transition from `Shared` (it was actually `Idle` state, since we only used one blade) to `Modified` states for `1` blades.
    - Another example is `[shared]_to_[modified]_total_[8]_blades.log`: transition from `Shared` to `Modified` states for `8` blades (7 sharers -> 1 writer).
  - Please look into the file to check the values reported in the paper
    - Please look at the `Avg(ns)` column
    - `FH_fetch_remote_tot` shows network latency in ns
    - `FH_ack_waiting_node` shows latency for waiting ACK/invalidation in ns
    - (*We ported this useful profiling system from [LegoOS](https://github.com/WukLab/LegoOS)*)

We can test other transition cases (some examples):
- Modified to Modified
    ```bash
    python3 run_commands.py --profile profiles/03b_latency_M_to_M.yaml
    ```
- Shared to Modified with 4 blades (3 shrarers -> 1writer)
    ```bash
    python3 run_commands.py --profile profiles/03b_latency_S_to_M_4.yaml
    ```
- Shared to Shared with 8 blades (7 sharers -> 8 sharers)
    ```bash
    python3 run_commands.py --profile profiles/03b_latency_S_to_S_8.yaml
    ```

---

## ![#c5f015](https://via.placeholder.com/15/c5f015/000000?text=+) Benchmark with various sharing and read/write ratios (Fig. 7-right)
Now we run an experiment with various sharing and read(/write) ratios. Let's try an experiment for sharing ratio=50 and read ratio=50:
```bash
python3 run_commands.py --profile profiles/03_sharing_ratio_sr50_rw50.yaml
```
- Result will be placed in `~/Downloads/03a_sharing_ratio`
  - The name `res_2_sr050_rw050.log` represents that it was from the 3rd blade (id=`2`), and sharing ratio was `50%` and read ratio was `50%`.
  - To compute the final output value from the logs:
    ```bash
    cd post_processing && ./03sharing_ratio_res.sh && cd ..
    ```
    - Inside the file, the last line shows 4KB IOPS. We used the sum over 8 blades.

Please find other example profiles such as `03_sharing_ratio_sr0_rw0.yaml`, ..., `03_sharing_ratio_sr100_rw100.yaml`.
- Please modify `03_sharing_ratio.yaml` if you want to test your own sharing and read ratios:
  ```yaml
    - name: run sharing ratio
      job: sharing_ratio
      per_command_delay: 10
      post_delay: 0
      job_args:
          sharing ratio: 50     # modify this value for sharing ratio [0, 25, 50, 75, 100]
          rw ratio: 50          # modify this value for read ratio [0, 25, 50, 75, 100]
          node num: 2           # total number of compute blades
  ```

--- 
## (Re-)Build MIND kernel
If you want to pull the lastest repo inside blades (i.e., VMs) and then rebuild the kernel, please run
```
python3 run_commands.py --profile profiles/01_restart_vms.yaml
```
- This profile will clone this repo (which will print warnings because our VMs already have the repo), build kernel, and restart VMs.
