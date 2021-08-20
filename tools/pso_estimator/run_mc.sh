#!/bin/bash
# PSO
python3 main.py --dir=$PWD/input/mc/cdf_profile_t10/ --pso_dir=$PWD/input/mc/pso/ --rw_dir=$PWD/input/mc/rwcnt/ --tar=20000 --ext=20000 --remote_adjust=0.59
python3 main.py --dir=$PWD/input/mc/cdf_profile_t20/ --pso_dir=$PWD/input/mc/pso/ --rw_dir=$PWD/input/mc/rwcnt/ --tar=20000 --ext=20000 --remote_adjust=0.76
python3 main.py --dir=$PWD/input/mc/cdf_profile_t40/ --pso_dir=$PWD/input/mc/pso/ --rw_dir=$PWD/input/mc/rwcnt/ --tar=20000 --ext=20000 --remote_adjust=0.79
python3 main.py --dir=$PWD/input/mc/cdf_profile_t80/ --pso_dir=$PWD/input/mc/pso/ --rw_dir=$PWD/input/mc/rwcnt/ --tar=20000 --ext=20000 --remote_adjust=0.82

# # PSO+
python3 main.py --dir=$PWD/input/mc/cdf_profile_t10/ --pso_dir=$PWD/input/mc/pso/ --rw_dir=$PWD/input/mc/rwcnt/ --tar=20000 --ext=20000 --remote_adjust=0.59 \
--unlimited_dir_sim=True --unlimit_cdf_dir=$PWD/input/mc/cdf_profile_unlimit.5k/ \
--limit_cdf_dir=$PWD/input/mc/cdf_profile_limit/ --unlimit_tar=5000
python3 main.py --dir=$PWD/input/mc/cdf_profile_t20/ --pso_dir=$PWD/input/mc/pso/ --rw_dir=$PWD/input/mc/rwcnt/ --tar=20000 --ext=20000 --remote_adjust=0.76 \
--unlimited_dir_sim=True --unlimit_cdf_dir=$PWD/input/mc/cdf_profile_unlimit.5k/ \
--limit_cdf_dir=$PWD/input/mc/cdf_profile_limit/ --unlimit_tar=5000
python3 main.py --dir=$PWD/input/mc/cdf_profile_t40/ --pso_dir=$PWD/input/mc/pso/ --rw_dir=$PWD/input/mc/rwcnt/ --tar=20000 --ext=20000 --remote_adjust=0.79 \
--unlimited_dir_sim=True --unlimit_cdf_dir=$PWD/input/mc/cdf_profile_unlimit.5k/ \
--limit_cdf_dir=$PWD/input/mc/cdf_profile_limit/ --unlimit_tar=5000
python3 main.py --dir=$PWD/input/mc/cdf_profile_t80/ --pso_dir=$PWD/input/mc/pso/ --rw_dir=$PWD/input/mc/rwcnt/ --tar=20000 --ext=20000 --remote_adjust=0.82 \
--unlimited_dir_sim=True --unlimit_cdf_dir=$PWD/input/mc/cdf_profile_unlimit.5k/ \
--limit_cdf_dir=$PWD/input/mc/cdf_profile_limit/ --unlimit_tar=5000
