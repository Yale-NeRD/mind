#!/bin/bash
# PSO
python3 main.py --dir=$PWD/input/ma/cdf_profile_t10/ --pso_dir=$PWD/input/ma/pso/ --rw_dir=$PWD/input/ma/rwcnt/ --tar=35000 --ext=35000 --remote_adjust=0.7
python3 main.py --dir=$PWD/input/ma/cdf_profile_t20/ --pso_dir=$PWD/input/ma/pso/ --rw_dir=$PWD/input/ma/rwcnt/ --tar=35000 --ext=35000 --remote_adjust=0.83
python3 main.py --dir=$PWD/input/ma/cdf_profile_t40/ --pso_dir=$PWD/input/ma/pso/ --rw_dir=$PWD/input/ma/rwcnt/ --tar=35000 --ext=35000 --remote_adjust=0.95
python3 main.py --dir=$PWD/input/ma/cdf_profile_t80/ --pso_dir=$PWD/input/ma/pso/ --rw_dir=$PWD/input/ma/rwcnt/ --tar=35000 --ext=35000 --remote_adjust=0.96

# # # PSO+
python3 main.py --dir=$PWD/input/ma/cdf_profile_t10/ --pso_dir=$PWD/input/ma/pso/ --rw_dir=$PWD/input/ma/rwcnt/ --tar=35000 --ext=35000 --remote_adjust=0.7 \
--unlimited_dir_sim=True --unlimit_cdf_dir=$PWD/input/ma/cdf_profile_unlimit/ \
--limit_cdf_dir=$PWD/input/ma/cdf_profile_limit/ --unlimit_tar=5000
python3 main.py --dir=$PWD/input/ma/cdf_profile_t20/ --pso_dir=$PWD/input/ma/pso/ --rw_dir=$PWD/input/ma/rwcnt/ --tar=35000 --ext=35000 --remote_adjust=0.83 \
--unlimited_dir_sim=True --unlimit_cdf_dir=$PWD/input/ma/cdf_profile_unlimit/ \
--limit_cdf_dir=$PWD/input/ma/cdf_profile_limit/ --unlimit_tar=5000
python3 main.py --dir=$PWD/input/ma/cdf_profile_t40/ --pso_dir=$PWD/input/ma/pso/ --rw_dir=$PWD/input/ma/rwcnt/ --tar=35000 --ext=35000 --remote_adjust=0.95 \
--unlimited_dir_sim=True --unlimit_cdf_dir=$PWD/input/ma/cdf_profile_unlimit/ \
--limit_cdf_dir=$PWD/input/ma/cdf_profile_limit/ --unlimit_tar=5000
python3 main.py --dir=$PWD/input/ma/cdf_profile_t80/ --pso_dir=$PWD/input/ma/pso/ --rw_dir=$PWD/input/ma/rwcnt/ --tar=35000 --ext=35000 --remote_adjust=0.96 \
--unlimited_dir_sim=True --unlimit_cdf_dir=$PWD/input/ma/cdf_profile_unlimit/ \
--limit_cdf_dir=$PWD/input/ma/cdf_profile_limit/ --unlimit_tar=5000
