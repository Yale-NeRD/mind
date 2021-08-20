import sys

trace_file_name = sys.argv[1]
res_file_name = sys.argv[2]
ops = {}
addrs = {}
vals = {}
trace_len = 0
ops[trace_file_name] = []
ops[res_file_name] = []
addrs[trace_file_name] = []
addrs[res_file_name] = []
vals[trace_file_name] = []
vals[res_file_name] = []

ALLOC_SIZE = 4096 * 9999
magic_stuff = 1111111
res = [magic_stuff for _ in range(ALLOC_SIZE)]
gold_res = [magic_stuff for _ in range(ALLOC_SIZE)]


def load_trace(file_name):
	file = open(file_name, "r")
	str_traces = file.read().splitlines()
	file.close()
	for i,trace in enumerate(str_traces):
		if i == 0:
			trace_len = int(trace)
			continue
		tmp = trace.split(" ")
		ops[file_name].append(tmp[0])
		addrs[file_name].append(int(tmp[1]))
		vals[file_name].append(int(tmp[2]))


def load_res(file_name):
	file = open(file_name, "r")
	str_ress = file.read().splitlines()
	file.close()
	for trace in str_ress:
		tmp = trace.split(" ")
		ops[file_name].append(tmp[0])
		addrs[file_name].append(int(tmp[1]))
		vals[file_name].append(int(tmp[2]))


def set_gold_res_by_trace(file_name):
	_ops = ops[file_name]
	_addrs = addrs[file_name]
	_vals = vals[file_name]

	buf = [0 for _ in range(ALLOC_SIZE)]
	for i, (op, addr, val) in enumerate(zip(_ops, _addrs, _vals)):
		if op == "w":
			buf[addr] = val
		elif op == "r":
			_vals[i] = buf[addr]
		else:
                    print("wrong access code: ", op)


def verify_res(trace_file_name, res_file_name):
	t_ops = ops[trace_file_name]
	t_addrs = addrs[trace_file_name]
	t_vals = vals[trace_file_name]
	r_ops = ops[res_file_name]
	r_addrs = addrs[res_file_name]
	r_vals = vals[res_file_name]

	good = True
	for i, (t_op, t_addr, t_val, r_op, r_addr, r_val) in enumerate(zip(t_ops, t_addrs, t_vals, r_ops, r_addrs, r_vals)):
		if t_op != r_op or t_addr != r_addr:
                    print("wrong-op: ", r_op, ", addr: ", r_addr, "<->", t_addr)
		elif t_val != r_val:
                    print("%d: addr: %d, get: %d, but expect: %d" % (i, t_addr, r_val, t_val))
                    good = False
	if good:
		print("test passed")


load_trace(trace_file_name)
set_gold_res_by_trace(trace_file_name)
load_res(res_file_name)
verify_res(trace_file_name, res_file_name)
