# Datasets
PRE_TF := /test_program_log/tf/
PRE_GC := /test_program_log/gc/
PRE_MA := /test_program_log/ma/memcached_a_
PRE_MC := /test_program_log/mc/
# MAX_PASS_TF := 50000
# MAX_PASS_GC := 50000
# MAX_PASS_MA := 35000
# MAX_PASS_MC := 20000
MAX_PASS_TF := $(MAX_PASS)
MAX_PASS_GC := $(MAX_PASS)
MAX_PASS_MA := $(MAX_PASS)
MAX_PASS_MC := $(MAX_PASS)


# System env
SUF := _0
USER_PRE := sudo -u user
USER_MID := user

DIR := $(shell pwd)
TMP_DIR := /tmp_test
SHELL=/bin/bash
NFS_IP := 192.168.122.1

build:
	mkdir -p bin
	g++ test_program.cpp -O2 -g -o bin/test_protocol -lpthread -D __TEST__
	sudo mkdir -p $(TMP_DIR)
	sudo mv bin/test_protocol $(TMP_DIR)/.
	sudo chown -R $(USER_MID) $(TMP_DIR)

build_debug:
	mkdir -p bin
	g++ test_program.cpp -O2 -g -o bin/test_protocol_debug -lpthread -D __TEST__ -D __NORMAL_MAP__
	$(eval NODE_ID := 0)
	$(eval NUM_NODE := 1)
	sudo mkdir -p $(TMP_DIR)
	sudo mv bin/test_protocol_debug $(TMP_DIR)/.
	sudo chown -R $(USER_MID) $(TMP_DIR)

build_profile:
	mkdir -p bin
	g++ test_program.cpp -O2 -g -o bin/test_protocol -lpthread -D __TEST__ -D __TEST_TIME_MEASUREMENT__ -Wno-unused-result
	sudo mkdir -p $(TMP_DIR)
	sudo mv bin/test_protocol $(TMP_DIR)/.
	sudo chown -R $(USER_MID) $(TMP_DIR)
	sudo rm -f $(TMP_DIR)/cdf_*

build_profile_debug:
	mkdir -p bin
	g++ test_program.cpp -O2 -g -o bin/test_protocol_debug -lpthread -D __TEST__  -D __NORMAL_MAP__ -D __TEST_TIME_MEASUREMENT__ -Wno-unused-result
	$(eval NODE_ID := 0)
	$(eval NUM_NODE := 1)
	sudo mkdir -p $(TMP_DIR)
	sudo mv bin/test_protocol_debug $(TMP_DIR)/.
	sudo chown -R $(USER_MID) $(TMP_DIR)
	sudo rm -f $(TMP_DIR)/cdf_*

clean:
	@rm -rf bin
	@sudo rm -r $(TMP_DIR)

# tensorflow
set_tf:
	sudo mkdir -p /test_program_log
	sudo mkdir -p /test_program_log/tf
	sudo mount $(NFS_IP):/media/data_ssds/tensorflow_logs /test_program_log/tf
	sudo mkdir -p /media/data_raid0/
	sudo mkdir -p /media/data_raid0/tensorflow_logs
	sudo mount $(NFS_IP):/media/data_raid0/tensorflow_logs /media/data_raid0/tensorflow_logs

set_tf_debug:
	sudo mkdir -p /test_program_log
	sudo mkdir -p /test_program_log/tf
	sudo mount $(NFS_IP):/home/yanpeng/2020_11_25_tensorflow/partitioned /test_program_log/tf
	# sudo mkdir -p /media/data_raid0/
	# sudo mount $(NFS_IP):/media/data_raid0/ /media/data_raid0/

# graphchi
set_gc:
	sudo mkdir -p /test_program_log
	sudo mkdir -p /test_program_log/gc
	sudo mount $(NFS_IP):/media/data_ssds/graphchi_logs /test_program_log/gc
	sudo mkdir -p /media/data_raid0/
	sudo mkdir -p /media/data_raid0/graphchi_logs
	sudo mount $(NFS_IP):/media/data_raid0/graphchi_logs /media/data_raid0/graphchi_logs

# memcached_a
set_ma:
	sudo mkdir -p /test_program_log
	sudo mkdir -p /test_program_log/ma
	sudo mount $(NFS_IP):/media/data_ssds/memcached_a_logs /test_program_log/ma
	sudo mkdir -p /media/data_raid0/
	sudo mkdir -p /media/data_raid0/memcached_a_logs
	sudo mount $(NFS_IP):/media/data_raid0/memcached_a_logs /media/data_raid0/memcached_a_logs

# memcached_c
set_mc:
	sudo mkdir -p /test_program_log
	sudo mkdir -p /test_program_log/mc
	sudo mount $(NFS_IP):/media/data_ssds/memcached_c_logs /test_program_log/mc
	sudo mkdir -p /media/data_raid0/
	sudo mkdir -p /media/data_raid0/memcached_c_logs
	sudo mount $(NFS_IP):/media/data_raid0/memcached_c_logs /media/data_raid0/memcached_c_logs

# === TENSORFLOW ===
# 8 nodes x 1 threads
run_tf_1t: set_tf build
	$(USER_PRE) $(TMP_DIR)/test_protocol $(NUM_NODE) $(NODE_ID) 1 $(MAX_PASS_TF) $(TMP_DIR) $(PRE_TF)$$((10 * $(NODE_ID)))

run_tf_2t: set_tf build
	$(USER_PRE) $(TMP_DIR)/test_protocol $(NUM_NODE) $(NODE_ID) 2 $(MAX_PASS_TF) $(TMP_DIR) $(PRE_TF)$$((10 * $(NODE_ID))) $(PRE_TF)$$((10 * $(NODE_ID) + 1))

run_tf_4t: set_tf build
	$(USER_PRE) $(TMP_DIR)/test_protocol $(NUM_NODE) $(NODE_ID) 4 $(MAX_PASS_TF) $(TMP_DIR) $(PRE_TF)$$((10 * $(NODE_ID))) $(PRE_TF)$$((10 * $(NODE_ID) + 1)) $(PRE_TF)$$((10 * $(NODE_ID) + 2)) $(PRE_TF)$$((10 * $(NODE_ID) + 3))

run_tf_8t: set_tf build
	$(USER_PRE) $(TMP_DIR)/test_protocol $(NUM_NODE) $(NODE_ID) 8 $(MAX_PASS_TF) $(TMP_DIR) $(PRE_TF)$$((10 * $(NODE_ID))) $(PRE_TF)$$((10 * $(NODE_ID) + 1)) $(PRE_TF)$$((10 * $(NODE_ID) + 2)) $(PRE_TF)$$((10 * $(NODE_ID) + 3)) $(PRE_TF)$$((10 * $(NODE_ID) + 4)) $(PRE_TF)$$((10 * $(NODE_ID) + 5)) $(PRE_TF)$$((10 * $(NODE_ID) + 6)) $(PRE_TF)$$((10 * $(NODE_ID) + 7))

run_tf_10t: set_tf build
	$(USER_PRE) $(TMP_DIR)/test_protocol $(NUM_NODE) $(NODE_ID) 10 $(MAX_PASS_TF) $(TMP_DIR) $(PRE_TF)$$((10 * $(NODE_ID))) $(PRE_TF)$$((10 * $(NODE_ID) + 1)) $(PRE_TF)$$((10 * $(NODE_ID) + 2)) $(PRE_TF)$$((10 * $(NODE_ID) + 3)) $(PRE_TF)$$((10 * $(NODE_ID) + 4)) $(PRE_TF)$$((10 * $(NODE_ID) + 5)) $(PRE_TF)$$((10 * $(NODE_ID) + 6)) $(PRE_TF)$$((10 * $(NODE_ID) + 7)) $(PRE_TF)$$((10 * $(NODE_ID) + 8)) $(PRE_TF)$$((10 * $(NODE_ID) + 9))

run_tf_1t_profile: set_tf build_profile
	$(USER_PRE) $(TMP_DIR)/test_protocol $(NUM_NODE) $(NODE_ID) 1 $(MAX_PASS_TF) $(TMP_DIR) $(PRE_TF)$$((10 * $(NODE_ID)))

run_tf_2t_profile: set_tf build_profile
	$(USER_PRE) $(TMP_DIR)/test_protocol $(NUM_NODE) $(NODE_ID) 2 $(MAX_PASS_TF) $(TMP_DIR) $(PRE_TF)$$((10 * $(NODE_ID))) $(PRE_TF)$$((10 * $(NODE_ID) + 1))

run_tf_4t_profile: set_tf build_profile
	$(USER_PRE) $(TMP_DIR)/test_protocol $(NUM_NODE) $(NODE_ID) 4 $(MAX_PASS_TF) $(TMP_DIR) $(PRE_TF)$$((10 * $(NODE_ID))) $(PRE_TF)$$((10 * $(NODE_ID) + 1)) $(PRE_TF)$$((10 * $(NODE_ID) + 2)) $(PRE_TF)$$((10 * $(NODE_ID) + 3))

run_tf_8t_profile: set_tf build_profile
	$(USER_PRE) $(TMP_DIR)/test_protocol $(NUM_NODE) $(NODE_ID) 8 $(MAX_PASS_TF) $(TMP_DIR) $(PRE_TF)$$((10 * $(NODE_ID))) $(PRE_TF)$$((10 * $(NODE_ID) + 1)) $(PRE_TF)$$((10 * $(NODE_ID) + 2)) $(PRE_TF)$$((10 * $(NODE_ID) + 3)) $(PRE_TF)$$((10 * $(NODE_ID) + 4)) $(PRE_TF)$$((10 * $(NODE_ID) + 5)) $(PRE_TF)$$((10 * $(NODE_ID) + 6)) $(PRE_TF)$$((10 * $(NODE_ID) + 7))

run_tf_10t_profile: set_tf build_profile
	$(USER_PRE) $(TMP_DIR)/test_protocol $(NUM_NODE) $(NODE_ID) 10 $(MAX_PASS_TF) $(TMP_DIR) $(PRE_TF)$$((10 * $(NODE_ID))) $(PRE_TF)$$((10 * $(NODE_ID) + 1)) $(PRE_TF)$$((10 * $(NODE_ID) + 2)) $(PRE_TF)$$((10 * $(NODE_ID) + 3)) $(PRE_TF)$$((10 * $(NODE_ID) + 4)) $(PRE_TF)$$((10 * $(NODE_ID) + 5)) $(PRE_TF)$$((10 * $(NODE_ID) + 6)) $(PRE_TF)$$((10 * $(NODE_ID) + 7)) $(PRE_TF)$$((10 * $(NODE_ID) + 8)) $(PRE_TF)$$((10 * $(NODE_ID) + 9))

# === debuging on unmodified Linux === #
run_tf_debug_1t: set_tf_debug build_debug
	$(USER_PRE) $(TMP_DIR)/test_protocol_debug 1 0 1 $(MAX_PASS_TF) $(TMP_DIR) $(PRE_TF)$$((10 * $(NODE_ID)))

run_tf_debug_10t: set_tf_debug build_debug
	$(USER_PRE) $(TMP_DIR)/test_protocol_debug 1 0 10 $(MAX_PASS_TF) $(TMP_DIR) $(PRE_TF)$$((10 * $(NODE_ID))) $(PRE_TF)$$((10 * $(NODE_ID) + 1)) $(PRE_TF)$$((10 * $(NODE_ID) + 2)) $(PRE_TF)$$((10 * $(NODE_ID) + 3)) $(PRE_TF)$$((10 * $(NODE_ID) + 4)) $(PRE_TF)$$((10 * $(NODE_ID) + 5)) $(PRE_TF)$$((10 * $(NODE_ID) + 6)) $(PRE_TF)$$((10 * $(NODE_ID) + 7)) $(PRE_TF)$$((10 * $(NODE_ID) + 8)) $(PRE_TF)$$((10 * $(NODE_ID) + 9))

# === GRAPHCHI === #
run_gc_1t: set_gc build
	$(USER_PRE) $(TMP_DIR)/test_protocol $(NUM_NODE) $(NODE_ID) 1 $(MAX_PASS_GC) $(TMP_DIR) $(PRE_GC)$$((10 * $(NODE_ID)))

run_gc_2t: set_gc build
	$(USER_PRE) $(TMP_DIR)/test_protocol $(NUM_NODE) $(NODE_ID) 2 $(MAX_PASS_GC) $(TMP_DIR) $(PRE_GC)$$((10 * $(NODE_ID))) $(PRE_GC)$$((10 * $(NODE_ID) + 1))

run_gc_4t: set_gc build
	$(USER_PRE) $(TMP_DIR)/test_protocol $(NUM_NODE) $(NODE_ID) 4 $(MAX_PASS_GC) $(TMP_DIR) $(PRE_GC)$$((10 * $(NODE_ID))) $(PRE_GC)$$((10 * $(NODE_ID) + 1)) $(PRE_GC)$$((10 * $(NODE_ID) + 2)) $(PRE_GC)$$((10 * $(NODE_ID) + 3))

run_gc_8t: set_gc build
	$(USER_PRE) $(TMP_DIR)/test_protocol $(NUM_NODE) $(NODE_ID) 8 $(MAX_PASS_GC) $(TMP_DIR) $(PRE_GC)$$((10 * $(NODE_ID))) $(PRE_GC)$$((10 * $(NODE_ID) + 1)) $(PRE_GC)$$((10 * $(NODE_ID) + 2)) $(PRE_GC)$$((10 * $(NODE_ID) + 3)) $(PRE_GC)$$((10 * $(NODE_ID) + 4)) $(PRE_GC)$$((10 * $(NODE_ID) + 5)) $(PRE_GC)$$((10 * $(NODE_ID) + 6)) $(PRE_GC)$$((10 * $(NODE_ID) + 7))

run_gc_10t: set_gc build
	$(USER_PRE) $(TMP_DIR)/test_protocol $(NUM_NODE) $(NODE_ID) 10 $(MAX_PASS_GC) $(TMP_DIR) $(PRE_GC)$$((10 * $(NODE_ID))) $(PRE_GC)$$((10 * $(NODE_ID) + 1)) $(PRE_GC)$$((10 * $(NODE_ID) + 2)) $(PRE_GC)$$((10 * $(NODE_ID) + 3)) $(PRE_GC)$$((10 * $(NODE_ID) + 4)) $(PRE_GC)$$((10 * $(NODE_ID) + 5)) $(PRE_GC)$$((10 * $(NODE_ID) + 6)) $(PRE_GC)$$((10 * $(NODE_ID) + 7)) $(PRE_GC)$$((10 * $(NODE_ID) + 8)) $(PRE_GC)$$((10 * $(NODE_ID) + 9))

run_gc_1t_profile: set_gc build_profile
	$(USER_PRE) $(TMP_DIR)/test_protocol $(NUM_NODE) $(NODE_ID) 1 $(MAX_PASS_GC) $(TMP_DIR) $(PRE_GC)$$((10 * $(NODE_ID)))

run_gc_2t_profile: set_gc build_profile
	$(USER_PRE) $(TMP_DIR)/test_protocol $(NUM_NODE) $(NODE_ID) 2 $(MAX_PASS_GC) $(TMP_DIR) $(PRE_GC)$$((10 * $(NODE_ID))) $(PRE_GC)$$((10 * $(NODE_ID) + 1))

run_gc_4t_profile: set_gc build_profile
	$(USER_PRE) $(TMP_DIR)/test_protocol $(NUM_NODE) $(NODE_ID) 4 $(MAX_PASS_GC) $(TMP_DIR) $(PRE_GC)$$((10 * $(NODE_ID))) $(PRE_GC)$$((10 * $(NODE_ID) + 1)) $(PRE_GC)$$((10 * $(NODE_ID) + 2)) $(PRE_GC)$$((10 * $(NODE_ID) + 3))

run_gc_8t_profile: set_gc build_profile
	$(USER_PRE) $(TMP_DIR)/test_protocol $(NUM_NODE) $(NODE_ID) 8 $(MAX_PASS_GC) $(TMP_DIR) $(PRE_GC)$$((10 * $(NODE_ID))) $(PRE_GC)$$((10 * $(NODE_ID) + 1)) $(PRE_GC)$$((10 * $(NODE_ID) + 2)) $(PRE_GC)$$((10 * $(NODE_ID) + 3)) $(PRE_GC)$$((10 * $(NODE_ID) + 4)) $(PRE_GC)$$((10 * $(NODE_ID) + 5)) $(PRE_GC)$$((10 * $(NODE_ID) + 6)) $(PRE_GC)$$((10 * $(NODE_ID) + 7))

run_gc_10t_profile: set_gc build_profile
	$(USER_PRE) $(TMP_DIR)/test_protocol $(NUM_NODE) $(NODE_ID) 10 $(MAX_PASS_GC) $(TMP_DIR) $(PRE_GC)$$((10 * $(NODE_ID))) $(PRE_GC)$$((10 * $(NODE_ID) + 1)) $(PRE_GC)$$((10 * $(NODE_ID) + 2)) $(PRE_GC)$$((10 * $(NODE_ID) + 3)) $(PRE_GC)$$((10 * $(NODE_ID) + 4)) $(PRE_GC)$$((10 * $(NODE_ID) + 5)) $(PRE_GC)$$((10 * $(NODE_ID) + 6)) $(PRE_GC)$$((10 * $(NODE_ID) + 7)) $(PRE_GC)$$((10 * $(NODE_ID) + 8)) $(PRE_GC)$$((10 * $(NODE_ID) + 9))

# === MEMCACHED_A === #
run_ma_1t: set_ma build
	$(USER_PRE) $(TMP_DIR)/test_protocol $(NUM_NODE) $(NODE_ID) 1 $(MAX_PASS_MA) $(TMP_DIR) $(PRE_MA)$$((10 * $(NODE_ID)))$(SUF)

run_ma_2t: set_ma build
	$(USER_PRE) $(TMP_DIR)/test_protocol $(NUM_NODE) $(NODE_ID) 2 $(MAX_PASS_MA) $(TMP_DIR) $(PRE_MA)$$((10 * $(NODE_ID)))$(SUF) $(PRE_MA)$$((10 * $(NODE_ID) + 1))$(SUF)

run_ma_4t: set_ma build
	$(USER_PRE) $(TMP_DIR)/test_protocol $(NUM_NODE) $(NODE_ID) 4 $(MAX_PASS_MA) $(TMP_DIR) $(PRE_MA)$$((10 * $(NODE_ID)))$(SUF) $(PRE_MA)$$((10 * $(NODE_ID) + 1))$(SUF) $(PRE_MA)$$((10 * $(NODE_ID) + 2))$(SUF) $(PRE_MA)$$((10 * $(NODE_ID) + 3))$(SUF)

run_ma_8t: set_ma build
	$(USER_PRE) $(TMP_DIR)/test_protocol $(NUM_NODE) $(NODE_ID) 8 $(MAX_PASS_MA) $(TMP_DIR) $(PRE_MA)$$((10 * $(NODE_ID)))$(SUF) $(PRE_MA)$$((10 * $(NODE_ID) + 1))$(SUF) $(PRE_MA)$$((10 * $(NODE_ID) + 2))$(SUF) $(PRE_MA)$$((10 * $(NODE_ID) + 3))$(SUF) $(PRE_MA)$$((10 * $(NODE_ID) + 4))$(SUF) $(PRE_MA)$$((10 * $(NODE_ID) + 5))$(SUF) $(PRE_MA)$$((10 * $(NODE_ID) + 6))$(SUF) $(PRE_MA)$$((10 * $(NODE_ID) + 7))$(SUF)

run_ma_10t: set_ma build
	$(USER_PRE) $(TMP_DIR)/test_protocol $(NUM_NODE) $(NODE_ID) 10 $(MAX_PASS_MA) $(TMP_DIR) $(PRE_MA)$$((10 * $(NODE_ID)))$(SUF) $(PRE_MA)$$((10 * $(NODE_ID) + 1))$(SUF) $(PRE_MA)$$((10 * $(NODE_ID) + 2))$(SUF) $(PRE_MA)$$((10 * $(NODE_ID) + 3))$(SUF) $(PRE_MA)$$((10 * $(NODE_ID) + 4))$(SUF) $(PRE_MA)$$((10 * $(NODE_ID) + 5))$(SUF) $(PRE_MA)$$((10 * $(NODE_ID) + 6))$(SUF) $(PRE_MA)$$((10 * $(NODE_ID) + 7))$(SUF) $(PRE_MA)$$((10 * $(NODE_ID) + 8))$(SUF) $(PRE_MA)$$((10 * $(NODE_ID) + 9))$(SUF)

run_ma_1t_profile: set_ma build_profile
	$(USER_PRE) $(TMP_DIR)/test_protocol $(NUM_NODE) $(NODE_ID) 1 $(MAX_PASS_MA) $(TMP_DIR) $(PRE_MA)$$((10 * $(NODE_ID)))$(SUF)

run_ma_2t_profile: set_ma build_profile
	$(USER_PRE) $(TMP_DIR)/test_protocol $(NUM_NODE) $(NODE_ID) 2 $(MAX_PASS_MA) $(TMP_DIR) $(PRE_MA)$$((10 * $(NODE_ID)))$(SUF) $(PRE_MA)$$((10 * $(NODE_ID) + 1))$(SUF)

run_ma_4t_profile: set_ma build_profile
	$(USER_PRE) $(TMP_DIR)/test_protocol $(NUM_NODE) $(NODE_ID) 4 $(MAX_PASS_MA) $(TMP_DIR) $(PRE_MA)$$((10 * $(NODE_ID)))$(SUF) $(PRE_MA)$$((10 * $(NODE_ID) + 1))$(SUF) $(PRE_MA)$$((10 * $(NODE_ID) + 2))$(SUF) $(PRE_MA)$$((10 * $(NODE_ID) + 3))$(SUF)

run_ma_8t_profile: set_ma build_profile
	$(USER_PRE) $(TMP_DIR)/test_protocol $(NUM_NODE) $(NODE_ID) 8 $(MAX_PASS_MA) $(TMP_DIR) $(PRE_MA)$$((10 * $(NODE_ID)))$(SUF) $(PRE_MA)$$((10 * $(NODE_ID) + 1))$(SUF) $(PRE_MA)$$((10 * $(NODE_ID) + 2))$(SUF) $(PRE_MA)$$((10 * $(NODE_ID) + 3))$(SUF) $(PRE_MA)$$((10 * $(NODE_ID) + 4))$(SUF) $(PRE_MA)$$((10 * $(NODE_ID) + 5))$(SUF) $(PRE_MA)$$((10 * $(NODE_ID) + 6))$(SUF) $(PRE_MA)$$((10 * $(NODE_ID) + 7))$(SUF)

run_ma_10t_profile: set_ma build_profile
	$(USER_PRE) $(TMP_DIR)/test_protocol $(NUM_NODE) $(NODE_ID) 10 $(MAX_PASS_MA) $(TMP_DIR) $(PRE_MA)$$((10 * $(NODE_ID)))$(SUF) $(PRE_MA)$$((10 * $(NODE_ID) + 1))$(SUF) $(PRE_MA)$$((10 * $(NODE_ID) + 2))$(SUF) $(PRE_MA)$$((10 * $(NODE_ID) + 3))$(SUF) $(PRE_MA)$$((10 * $(NODE_ID) + 4))$(SUF) $(PRE_MA)$$((10 * $(NODE_ID) + 5))$(SUF) $(PRE_MA)$$((10 * $(NODE_ID) + 6))$(SUF) $(PRE_MA)$$((10 * $(NODE_ID) + 7))$(SUF) $(PRE_MA)$$((10 * $(NODE_ID) + 8))$(SUF) $(PRE_MA)$$((10 * $(NODE_ID) + 9))$(SUF)

# === MEMCACHED_C === #
run_mc_1t: set_mc build
	$(USER_PRE) $(TMP_DIR)/test_protocol $(NUM_NODE) $(NODE_ID) 1 $(MAX_PASS_MC) $(TMP_DIR) $(PRE_MC)$$((10 * $(NODE_ID)))

run_mc_2t: set_mc build
	$(USER_PRE) $(TMP_DIR)/test_protocol $(NUM_NODE) $(NODE_ID) 2 $(MAX_PASS_MC) $(TMP_DIR) $(PRE_MC)$$((10 * $(NODE_ID))) $(PRE_MC)$$((10 * $(NODE_ID) + 1))

run_mc_4t: set_mc build
	$(USER_PRE) $(TMP_DIR)/test_protocol $(NUM_NODE) $(NODE_ID) 4 $(MAX_PASS_MC) $(TMP_DIR) $(PRE_MC)$$((10 * $(NODE_ID))) $(PRE_MC)$$((10 * $(NODE_ID) + 1)) $(PRE_MC)$$((10 * $(NODE_ID) + 2)) $(PRE_MC)$$((10 * $(NODE_ID) + 3))

run_mc_8t: set_mc build
	$(USER_PRE) $(TMP_DIR)/test_protocol $(NUM_NODE) $(NODE_ID) 8 $(MAX_PASS_MC) $(TMP_DIR) $(PRE_MC)$$((10 * $(NODE_ID))) $(PRE_MC)$$((10 * $(NODE_ID) + 1)) $(PRE_MC)$$((10 * $(NODE_ID) + 2)) $(PRE_MC)$$((10 * $(NODE_ID) + 3)) $(PRE_MC)$$((10 * $(NODE_ID) + 4)) $(PRE_MC)$$((10 * $(NODE_ID) + 5)) $(PRE_MC)$$((10 * $(NODE_ID) + 6)) $(PRE_MC)$$((10 * $(NODE_ID) + 7))

run_mc_10t: set_mc build
	$(USER_PRE) $(TMP_DIR)/test_protocol $(NUM_NODE) $(NODE_ID) 10 $(MAX_PASS_MC) $(TMP_DIR) $(PRE_MC)$$((10 * $(NODE_ID))) $(PRE_MC)$$((10 * $(NODE_ID) + 1)) $(PRE_MC)$$((10 * $(NODE_ID) + 2)) $(PRE_MC)$$((10 * $(NODE_ID) + 3)) $(PRE_MC)$$((10 * $(NODE_ID) + 4)) $(PRE_MC)$$((10 * $(NODE_ID) + 5)) $(PRE_MC)$$((10 * $(NODE_ID) + 6)) $(PRE_MC)$$((10 * $(NODE_ID) + 7)) $(PRE_MC)$$((10 * $(NODE_ID) + 8)) $(PRE_MC)$$((10 * $(NODE_ID) + 9))

run_mc_1t_profile: set_mc build_profile
	$(USER_PRE) $(TMP_DIR)/test_protocol $(NUM_NODE) $(NODE_ID) 1 $(MAX_PASS_MC) $(TMP_DIR) $(PRE_MC)$$((10 * $(NODE_ID)))

run_mc_2t_profile: set_mc build_profile
	$(USER_PRE) $(TMP_DIR)/test_protocol $(NUM_NODE) $(NODE_ID) 2 $(MAX_PASS_MC) $(TMP_DIR) $(PRE_MC)$$((10 * $(NODE_ID))) $(PRE_MC)$$((10 * $(NODE_ID) + 1))

run_mc_4t_profile: set_mc build_profile
	$(USER_PRE) $(TMP_DIR)/test_protocol $(NUM_NODE) $(NODE_ID) 4 $(MAX_PASS_MC) $(TMP_DIR) $(PRE_MC)$$((10 * $(NODE_ID))) $(PRE_MC)$$((10 * $(NODE_ID) + 1)) $(PRE_MC)$$((10 * $(NODE_ID) + 2)) $(PRE_MC)$$((10 * $(NODE_ID) + 3))

run_mc_8t_profile: set_mc build_profile
	$(USER_PRE) $(TMP_DIR)/test_protocol $(NUM_NODE) $(NODE_ID) 8 $(MAX_PASS_MC) $(TMP_DIR) $(PRE_MC)$$((10 * $(NODE_ID))) $(PRE_MC)$$((10 * $(NODE_ID) + 1)) $(PRE_MC)$$((10 * $(NODE_ID) + 2)) $(PRE_MC)$$((10 * $(NODE_ID) + 3)) $(PRE_MC)$$((10 * $(NODE_ID) + 4)) $(PRE_MC)$$((10 * $(NODE_ID) + 5)) $(PRE_MC)$$((10 * $(NODE_ID) + 6)) $(PRE_MC)$$((10 * $(NODE_ID) + 7))

run_mc_10t_profile: set_mc build_profile
	$(USER_PRE) $(TMP_DIR)/test_protocol $(NUM_NODE) $(NODE_ID) 10 $(MAX_PASS_MC) $(TMP_DIR) $(PRE_MC)$$((10 * $(NODE_ID))) $(PRE_MC)$$((10 * $(NODE_ID) + 1)) $(PRE_MC)$$((10 * $(NODE_ID) + 2)) $(PRE_MC)$$((10 * $(NODE_ID) + 3)) $(PRE_MC)$$((10 * $(NODE_ID) + 4)) $(PRE_MC)$$((10 * $(NODE_ID) + 5)) $(PRE_MC)$$((10 * $(NODE_ID) + 6)) $(PRE_MC)$$((10 * $(NODE_ID) + 7)) $(PRE_MC)$$((10 * $(NODE_ID) + 8)) $(PRE_MC)$$((10 * $(NODE_ID) + 9))
