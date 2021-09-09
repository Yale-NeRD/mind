# Guide to run fastswap experiments in Yale cluster

## Summary
We will run the following experiments:
- Fastswap performance evaluation with memory traces (Fig. 6 left)
The interface of the control script is very similar to MIND's.
---

<br></br>
## Setup evaluation for FastSwap
We assume you already have access to the control server.

Change to the directory of this repository (having scripts for FastSwap).
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
---

<br></br>
## ![#c5f015](https://via.placeholder.com/15/c5f015/000000?text=+) Fastswap Performance evaluation with memory traces (Fig. 6 left)

*Skip this step if you already loaded target traces for MIND*
- Every time you load traces for new application, it will automatically erase the previous ones
- Go inside the script directory and load memory access traces for Tensorflow

```bash
python3 run_commands.py --profile=profiles/05_load_trace_tf.yaml
```

After the script for loading traces is finished, we can run the following command to run an experiment with the TensorFlow memory traces we just loaded:
```bash
python3 run_commands.py --profile profiles/04_macro_bench_tf.yaml
```
- By default, it will take 10 ~ 20 minutes by running only 1/10 of the trace (i.e., 5k steps of total 50k steps).
  - Please modify the values in `profiles/04_macro_bench_tf.yaml` to change number of threads and steps.
  - **IMPORTANT** fastswap can not run on multiple compute servers, so it must be `node num: 1`. 
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

You can also test traces from other applications, for example:
```bash
python3 run_commands.py --profile=profiles/05_load_trace_gc.yaml
python3 run_commands.py --profile profiles/04_macro_bench_gc.yaml
```
