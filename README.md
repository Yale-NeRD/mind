# Guide to run fastswap experiments in Yale cluster

## Summary
We will run the following experiments:
- Fastswap Performance evaluation with memory traces (Fig. 6 left)

Generating each data point will take 15 ~ 60 minutes with default configuration in this repository.

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

Once you log in, the default directory is set to `scripts` directory inside the [control scripts](https://github.com/shsym/mind/tree/main/ctrl_scripts/) of MIND repository:
```bash
$ pwd
/home/sosp_ae/mind/ctrl_scripts/scripts
```
**IMPORTANT** However, in order to run fastswap evaluations, please use the following command to change to the directory of this repository.

```bash
cd /home/sosp_ae/mind_ae_fastswap/ctrl_scripts/scripts
```

You can check the current status of git repository
```bash
$ git remote -v
origin	https://github.com/yyppyy/mind.git (fetch)
origin	https://github.com/yyppyy/mind.git (push)

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

Please tell switch that you are going to run normal switch program
```bash
python3 run_commands.py --profile=profiles/02_setup_normal_switch.yaml
```

## ![#c5f015](https://via.placeholder.com/15/c5f015/000000?text=+) Fastswap Performance evaluation with memory traces (Fig. 6 left)

Let's go inside the script directory and load memory access traces for Tensorflow
```bash
python3 run_commands.py --profile=profiles/05_load_trace_tf.yaml
```
The script will print out raw input (i.e., ssh commands to the servers) and standard output.
- Since we need to download more than 1TB of data, this will take some time up to one hour.

After the script for loading traces is finished, we can run the following command to run an experiment with the TensorFlow memory traces we just loaded:
```bash
python3 run_commands.py --profile profiles/04_macro_bench_tf.yaml
```
- By default, it will run the entire traces on a single compute blades; it will take 10 ~ 20 minutes.
  - Please modify the values in `profiles/04_macro_bench_tf.yaml` for test various setup.
  - **IMPORTANT** fastswap can not run on multiple compute servers. So the node_num should always be set to 1. 

    ```yaml
    - name: run macro benchmark
        job: macro_bench
        per_command_delay: 10
        post_delay: 0
        job_args:
          trace: tf         # Tensorflow workload
          node_num: 1       # <- fix to 1 for fastswap
          thread_num: 10    # <- modify this value to set the number of threads per blade [1, 2, 4, 10]
          step_num: 5000    # <- increase this value to run more portion of the traces
    ```
  - Tag for the application or [APP]
    - [APP]: `tf` for TensorFlow, `gc` for GraphChi, `ma` / `mc` for Memcached with YCSB workloadA/workloadC
  - Number of total steps we used in the paper are
    - `tf`: 50000,  `gc`: 50000, `ma`: 35000, `mc`: 20000

The result of the experiment will be downloaded at `~/Downloads/04_macro_bench_[APP]`.


To compute the final number of the result, please run
```bash
cd post_processing && ./04macro_bench_res.sh && cd ..
```
- This script will scan through the directories for all the applications and number of compute blades then calculate normalized computing speed.
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
