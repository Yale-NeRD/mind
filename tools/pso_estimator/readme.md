# PSO/PSO+ estimator
## Overall estimation flow chart
![cluster_setup](https://github.com/shsym/mind/blob/main/tools/pso_estimator/pso_ordering.png?raw=true)

## Run
  - Use the following scripts in this directory for running PSO/PSO+ estimation: `run_tf.sh`, `run_gc.sh`, `run_ma.sh`, `run_mc.sh`
  - The script will print out Estimated time for PSO and SC
    - Time for SC is used for obtaining the input argument `--remote_adjust` (details in Usage section below)
      - *Script contains the values that we found from our experiments.*
      - Someone try to find a value of `--remote_adjust` where
        ```
        [running time without profiling] - [running time without memory accesses] = [estimated SC time]
        ```
        - The first two values are measured on MIND (not this emulator or simulator)
        - `[running time without memory accesses]` can be measured by simply comment out memory access in [this file](https://github.com/shsym/mind/blob/main/mind_linux/test_programs/04_macro_benchmark/test_program.cpp)
    - Time for PSO is used for PSO/PSO+ estimation 

## Usage
- PSO
```
python3 main.py \
--dir=[Path to CDF log files collected from profiling in MIND] \
--pso_dir=[Path to pso logs from the simulation with limited directory entries] \
--rw_dir=[Path to rwcnt logs from the simulation with limited directory entries] \
--tar=[Maximum number of steps/passes] \
--ext=[NOT USED, for extrapolation] \
--remote_adjust=[CDF adjustment factor: difference between w/ and w/o profiling overhead on MIND] \
```

- PSO+ (unlimited directory entries)
  - Use the same input arguement as PSO. Additional arguments only for PSO+ are
```
--unlimited_dir_sim=[Should be 'True' for indicating we are running PSO+ not PSO] \
--unlimit_cdf_dir=[Path to CDF logs from simulation with unlimited directory entries] \
--limit_cdf_dir=[Path to CDF logs from simulation with limited directory entries] \
--unlimit_tar=[Maximum number of steps/passes for simulation with unlimited directory entries] \
```
  - The last arguement is used when we could not fully run the simulator for unlimited directory entries due to the time constraint.
