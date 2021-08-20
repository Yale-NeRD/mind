# Makefile
# - Example:
#	make run_4_0_1_0 LOC=1 RRATIO=100 SRATIO=100

IDIR = .
CC=gcc
CFLAGS=-I$(IDIR) -lpthread -g -D __TEST__

obj-y += test_protocol.c
# test_utils.c

default: test_protocol

test_protocol: $(obj-y)
	@make clean
	@mkdir -p bin
	$(CC) -o bin/$@ $^ $(CFLAGS) $(LIBS)

clean:
	@rm -f *.o
	@rm -f bin/*

run_shared: test_protocol
	@bin/test_protocol 0 $(NODE_ID) 0

run_modified: test_protocol
	@bin/test_protocol 0 $(NODE_ID) 1

run_debug: test_protocol
	@bin/test_protocol 1 $(NODE_ID) 0
