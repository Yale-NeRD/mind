# Makefile
IDIR = .
CC=gcc
CFLAGS=-I$(IDIR) -lpthread -g -D __TEST__

obj-y += test_protocol.o

test_protocol: $(obj-y)
	@mkdir -p bin
	@$(CC) -o bin/$@ $^ $(CFLAGS) $(LIBS)

clean:
	@rm -f *.o
	@rm -f bin/*

run: test_protocol
	@bin/test_protocol trace/multiPage_align1024_tid0_r2w0.5 trace/multiPage_align1024_tid1_r2w0.5 res/res0.txt res/res1.txt 1 $(NODE_ID)

run_multi_node1: test_protocol
	@bin/test_protocol trace/multiPage_align1024_tid0_r2w0.5 trace/multiPage_align1024_tid1_r2w0.5 res/res0.txt res/res1.txt 2 1
	@python3 res/verify_res.py trace/multiPage_align1024_tid0_r2w0.5 res/res0.txt
	@python3 res/verify_res.py trace/multiPage_align1024_tid1_r2w0.5 res/res1.txt

run_multi_node2: test_protocol
	@bin/test_protocol trace/multiPage_align1024_tid2_r2w0.5 trace/multiPage_align1024_tid3_r2w0.5 res/res2.txt res/res3.txt 2 2
	@python3 res/verify_res.py trace/multiPage_align1024_tid2_r2w0.5 res/res2.txt
	@python3 res/verify_res.py trace/multiPage_align1024_tid3_r2w0.5 res/res3.txt

run_uni_node1: test_protocol
	@bin/test_protocol trace/uniPage_align4_tid0_r2w0.5 trace/uniPage_align4_tid1_r2w0.5 res/res0.txt res/res1.txt 2 1
	@python3 res/verify_res.py trace/uniPage_align4_tid0_r2w0.5 res/res0.txt
	@python3 res/verify_res.py trace/uniPage_align4_tid1_r2w0.5 res/res1.txt

run_uni_node2: test_protocol
	@bin/test_protocol trace/uniPage_align4_tid2_r2w0.5 trace/uniPage_align4_tid3_r2w0.5 res/res2.txt res/res3.txt 2 2
	@python3 res/verify_res.py trace/uniPage_align4_tid2_r2w0.5 res/res2.txt
	@python3 res/verify_res.py trace/uniPage_align4_tid3_r2w0.5 res/res3.txt

run_uni_99_node1: test_protocol
	@bin/test_protocol trace/uniPage_align4_tid0_r2w0.99 trace/uniPage_align4_tid1_r2w0.99 res/res0.txt res/res1.txt 2 1
	@python3 res/verify_res.py trace/uniPage_align4_tid0_r2w0.99 res/res0.txt
	@python3 res/verify_res.py trace/uniPage_align4_tid1_r2w0.99 res/res1.txt

run_uni_99_node2: test_protocol
	@bin/test_protocol trace/uniPage_align4_tid2_r2w0.99 trace/uniPage_align4_tid3_r2w0.99 res/res2.txt res/res3.txt 2 2
	@python3 res/verify_res.py trace/uniPage_align4_tid2_r2w0.99 res/res2.txt
	@python3 res/verify_res.py trace/uniPage_align4_tid3_r2w0.99 res/res3.txt

run_uni_01_node1: test_protocol
	@bin/test_protocol trace/uniPage_align4_tid0_r2w0.01 trace/uniPage_align4_tid1_r2w0.01 res/res0.txt res/res1.txt 2 1
	@python3 res/verify_res.py trace/uniPage_align4_tid0_r2w0.01 res/res0.txt
	@python3 res/verify_res.py trace/uniPage_align4_tid1_r2w0.01 res/res1.txt

run_uni_01_node2: test_protocol
	@bin/test_protocol trace/uniPage_align4_tid2_r2w0.01 trace/uniPage_align4_tid3_r2w0.01 res/res2.txt res/res3.txt 2 2
	@python3 res/verify_res.py trace/uniPage_align4_tid2_r2w0.01 res/res2.txt
	@python3 res/verify_res.py trace/uniPage_align4_tid3_r2w0.01 res/res3.txt

generate_trace: clean_traces
	@cd trace && python3 genTrace.py
	@ls -l trace

clean_traces:
	@rm -f trace/multiPage*
	@rm -f trace/uniPage*
