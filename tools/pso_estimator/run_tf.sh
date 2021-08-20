#!/bin/bash
# PSO
python3 main.py --dir=$PWD/input/tf/cdf_profile_t10/ --pso_dir=$PWD/input/tf/pso/ --rw_dir=$PWD/input/tf/rwcnt/ --tar=50000 --ext=50000 --remote_adjust=0.125
python3 main.py --dir=$PWD/input/tf/cdf_profile_t20/ --pso_dir=$PWD/input/tf/pso/ --rw_dir=$PWD/input/tf/rwcnt/ --tar=50000 --ext=50000 --remote_adjust=0.2
python3 main.py --dir=$PWD/input/tf/cdf_profile_t40/ --pso_dir=$PWD/input/tf/pso/ --rw_dir=$PWD/input/tf/rwcnt/ --tar=50000 --ext=50000 --remote_adjust=0.165
python3 main.py --dir=$PWD/input/tf/cdf_profile_t80/ --pso_dir=$PWD/input/tf/pso/ --rw_dir=$PWD/input/tf/rwcnt/ --tar=50000 --ext=50000 --remote_adjust=0.26
