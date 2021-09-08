#!/bin/bash
# PSO
python3 main.py --dir=$PWD/input/gc/cdf_profile_t10/ --pso_dir=$PWD/input/gc/pso/ --rw_dir=$PWD/input/gc/rwcnt/ --tar=50000 --ext=50000 --proc_time=42908274 --remote_adjust=0.13
python3 main.py --dir=$PWD/input/gc/cdf_profile_t20/ --pso_dir=$PWD/input/gc/pso/ --rw_dir=$PWD/input/gc/rwcnt/ --tar=50000 --ext=50000 --proc_time=42908274 --remote_adjust=0.2
python3 main.py --dir=$PWD/input/gc/cdf_profile_t40/ --pso_dir=$PWD/input/gc/pso/ --rw_dir=$PWD/input/gc/rwcnt/ --tar=50000 --ext=50000 --proc_time=42908274 --remote_adjust=0.5
python3 main.py --dir=$PWD/input/gc/cdf_profile_t80/ --pso_dir=$PWD/input/gc/pso/ --rw_dir=$PWD/input/gc/rwcnt/ --tar=50000 --ext=50000 --proc_time=42908274 --remote_adjust=0.56
