## Introduction
This repo contain tools that prepare memory access traces for MIND's evaluation.
The traces includes normal read/write as well as memory related syscalls(brk, mmap and munmap).
Each trace is compacted into 15 Bytes, whose bit-structure is shown here:
```
-----------------------------------------------------------------------------------------
| bit         | 0          | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 |
-----------------------------------------------------------------------------------------
| read/write  | opcode(1B) |      address(6B)      |           timestamp(8B)            |
-----------------------------------------------------------------------------------------
| mmap |      | opcode(1B) |delta time(4B) |   start address(6B)    |    length(4B)     |
-----------------------------------------------------------------------------------------
| brk |       | opcode(1B) |      new brk(6B)      |           timestamp(8B)            |
-----------------------------------------------------------------------------------------
| munmap |    | opcode(1B) |delta time(4B) |   start address(6B)    |    length(4B)     |
-----------------------------------------------------------------------------------------
```
Their corresponding data structures in C are defined under `./include/trace_def.hpp`.


## Get Started
To prepare MIND's evaluation traces for your target application runned on N cores, please use:
```shell
$ cd ./script
$ ./prepare_trace.sh N your_app args_for_your_app
```
After the script finishes, the default location of original and remapped per core traces are
`./traces/original/` and `./traces/remapped/`.
If you would like to change the defualt location of traces, please edit `trace_dir` in `./script/prepare_trace.sh` and `./pin/source/tools/ManualExamples/pinatrace.cpp`. And if you would like to change the maximum number of cores which is default to 192, please edit `MAX_NUM_THREADS` in  `./pin/source/tools/ManualExamples/pinatrace.hpp`

To prepare more detailed and human-readable syscall traces for MIND's memory allocation evaluation, please use:
```shell
$ cd ./script
$ ./prepare_syscall_trace.sh your_app args_for_your_app
```
The syscall traces will be in `./traces/syscall` on default.
If you would like to change the defualt location of traces, please edit `trace_dir` in `./script/prepare_syscall_trace.sh` and `./pin/source/tools/ManualExamples/malloctrace.cpp`
