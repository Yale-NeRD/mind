IDIR = .
CC=gcc
CFLAGS=-I$(IDIR) -lpthread -g -D __TEST__ -O2 -D_GNU_SOURCE

obj-y += test_protocol.c test_utils.c

default: test_protocol

test_protocol: $(obj-y)
	@mkdir -p logs_03a_sharing_ratio
	@make clean
	@mkdir -p bin
	$(CC) -o bin/$@ $^ $(CFLAGS) $(LIBS)

clean:
	@rm -f *.o
	@rm -f bin/*

debug: $(obj-y)
	@make clean
	@mkdir -p bin
	$(CC) -o bin/debug_test $^ $(CFLAGS) $(LIBS) -D DEBUG
	
run_debug: debug
	@bin/debug_test 1 1 $(LOC) 1 0 1 $(SRATIO) $(RRATIO)

# ** presets **
# make cases: run_shared_(shared ratio)_(read ratio)
# params: [is_local/debug] [#local thread] [shared data in #pages] [#compute blades] [node_id] [#memory blades] [shared_ratio] [read_ratio] [locality] [local data in #pages] [dummies...]
# * Currently unused args: #memory blades, locality

run_shared_0_0: test_protocol
	@bin/test_protocol 0 1 204800 $(NUM_NODE) $(NODE_ID) 1 0 0 50 204800 0 shared_$(NODE_ID)_0

run_shared_0_25: test_protocol
	@bin/test_protocol 0 1 204800 $(NUM_NODE) $(NODE_ID) 1 0 25 50 204800 0 shared_$(NODE_ID)_0

run_shared_0_50: test_protocol
	@bin/test_protocol 0 1 204800 $(NUM_NODE) $(NODE_ID) 1 0 50 50 204800 0 shared_$(NODE_ID)_0

run_shared_0_75: test_protocol
	@bin/test_protocol 0 1 204800 $(NUM_NODE) $(NODE_ID) 1 0 75 50 204800 0 shared_$(NODE_ID)_0

run_shared_0_100: test_protocol
	@bin/test_protocol 0 1 204800 $(NUM_NODE) $(NODE_ID) 1 0 100 50 204800 0 shared_$(NODE_ID)_0


run_shared_25_0: test_protocol
	@bin/test_protocol 0 1 204800 $(NUM_NODE) $(NODE_ID) 1 25 0 50 204800 0 shared_$(NODE_ID)_0

run_shared_25_25: test_protocol
	@bin/test_protocol 0 1 204800 $(NUM_NODE) $(NODE_ID) 1 25 25 50 204800 0 shared_$(NODE_ID)_0

run_shared_25_50: test_protocol
	@bin/test_protocol 0 1 204800 $(NUM_NODE) $(NODE_ID) 1 25 50 50 204800 0 shared_$(NODE_ID)_0

run_shared_25_75: test_protocol
	@bin/test_protocol 0 1 204800 $(NUM_NODE) $(NODE_ID) 1 25 75 50 204800 0 shared_$(NODE_ID)_0

run_shared_25_100: test_protocol
	@bin/test_protocol 0 1 204800 $(NUM_NODE) $(NODE_ID) 1 25 100 50 204800 0 shared_$(NODE_ID)_0



run_shared_50_0: test_protocol
	@bin/test_protocol 0 1 204800 $(NUM_NODE) $(NODE_ID) 1 50 0 50 204800 0 shared_$(NODE_ID)_0

run_shared_50_25: test_protocol
	@bin/test_protocol 0 1 204800 $(NUM_NODE) $(NODE_ID) 1 50 25 50 204800 0 shared_$(NODE_ID)_0

run_shared_50_50: test_protocol
	@bin/test_protocol 0 1 204800 $(NUM_NODE) $(NODE_ID) 1 50 50 50 204800 0 shared_$(NODE_ID)_0

run_shared_50_75: test_protocol
	@bin/test_protocol 0 1 204800 $(NUM_NODE) $(NODE_ID) 1 50 75 50 204800 0 shared_$(NODE_ID)_0

run_shared_50_100: test_protocol
	@bin/test_protocol 0 1 204800 $(NUM_NODE) $(NODE_ID) 1 50 100 50 204800 0 shared_$(NODE_ID)_0



run_shared_75_0: test_protocol
	@bin/test_protocol 0 1 204800 $(NUM_NODE) $(NODE_ID) 1 75 0 50 204800 0 shared_$(NODE_ID)_0

run_shared_75_25: test_protocol
	@bin/test_protocol 0 1 204800 $(NUM_NODE) $(NODE_ID) 1 75 25 50 204800 0 shared_$(NODE_ID)_0

run_shared_75_50: test_protocol
	@bin/test_protocol 0 1 204800 $(NUM_NODE) $(NODE_ID) 1 75 50 50 204800 0 shared_$(NODE_ID)_0

run_shared_75_75: test_protocol
	@bin/test_protocol 0 1 204800 $(NUM_NODE) $(NODE_ID) 1 75 75 50 204800 0 shared_$(NODE_ID)_0

run_shared_75_100: test_protocol
	@bin/test_protocol 0 1 204800 $(NUM_NODE) $(NODE_ID) 1 75 100 50 204800 0 shared_$(NODE_ID)_0


run_shared_100_0: test_protocol
	@bin/test_protocol 0 1 204800 $(NUM_NODE) $(NODE_ID) 1 100 0 50 204800 0 shared_$(NODE_ID)_0

run_shared_100_25: test_protocol
	@bin/test_protocol 0 1 204800 $(NUM_NODE) $(NODE_ID) 1 100 25 50 204800 0 shared_$(NODE_ID)_0

run_shared_100_50: test_protocol
	@bin/test_protocol 0 1 204800 $(NUM_NODE) $(NODE_ID) 1 100 50 50 204800 0 shared_$(NODE_ID)_0

run_shared_100_75: test_protocol
	@bin/test_protocol 0 1 204800 $(NUM_NODE) $(NODE_ID) 1 100 75 50 204800 0 shared_$(NODE_ID)_0

run_shared_100_90: test_protocol
	@bin/test_protocol 0 1 204800 $(NUM_NODE) $(NODE_ID) 1 100 90 50 204800 0 shared_$(NODE_ID)_0

run_shared_100_100: test_protocol
	@bin/test_protocol 0 1 204800 $(NUM_NODE) $(NODE_ID) 1 100 100 50 204800 0 shared_$(NODE_ID)_0

