/*
 * Copyright 2002-2020 Intel Corporation.
 * 
 * This software is provided to you as Sample Source Code as defined in the accompanying
 * End User License Agreement for the Intel(R) Software Development Products ("Agreement")
 * section 1.L.
 * 
 * This software and the related documents are provided as is, with no express or implied
 * warranties, other than those that are expressly stated in the License.
 */

/*
 *  This file contains an ISA-portable PIN tool for tracing memory accesses.
 */
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <algorithm>
#include "pin.H"
#include <sys/syscall.h>
#include <pthread.h>
#include <sys/stat.h>
#include "pinatrace.hpp"


const std::string trace_dir = "../../../../traces/";

unsigned long pad1[PAD];
char *buf_tail[MAX_NUM_THREADS] = {};

unsigned long pad2[PAD];
char *buf[MAX_NUM_THREADS] = {};

unsigned long pad4[PAD];
unsigned long prev_timestamp[MAX_NUM_THREADS] = {};

unsigned long pad5[PAD];
unsigned long ins_cnt[MAX_NUM_THREADS] = {};

unsigned long pad7[PAD];
int trace[MAX_NUM_THREADS] = {};

unsigned long pad8[PAD];
PIN_MUTEX mutexs[MAX_NUM_THREADS];

unsigned long pad3[PAD];
std::string mm_syscalls[NUM_MM_SYSCALLS];

void flush_buf(THREADID threadid) {
    if (buf_tail[threadid] != buf[threadid]) {
        long size = write(trace[threadid], buf[threadid], buf_tail[threadid] - buf[threadid]);
        if (size != buf_tail[threadid] - buf[threadid]) {
	        printf("fail to flush file[%d] errno[%d], expect to write %ld bytes, but write %ld bytes\n",
			    threadid, errno, buf_tail[threadid] - buf[threadid], size);
                return;
        }
        buf_tail[threadid] = buf[threadid];
    }
    return;
}

// Print a memory read record
VOID RecordMemRead(VOID * ip, VOID * addr, THREADID threadid)
{
	unsigned int cpu;
	syscall(SYS_getcpu, &cpu, NULL, NULL);
	threadid = cpu;

    struct timeval ts;
    gettimeofday(&ts, NULL);
    prev_timestamp[threadid] = ts.tv_sec * 1000000 + ts.tv_usec;

    PIN_MutexLock(&mutexs[cpu]);

    char *p = buf_tail[threadid];
    char *limit = buf[threadid] + BUF_SIZE_PER_THREAD;
    if ((unsigned long)(p + sizeof(struct RWlog)) > (unsigned long)limit - 1)
        flush_buf(threadid);

    struct RWlog *log = (struct RWlog *)(buf_tail[threadid]);
    log->addr = (unsigned long)addr & MMAP_ADDR_MASK;
    log->op = 'R';
    log->usec = prev_timestamp[threadid];

    if ((unsigned long)addr > MMAP_ADDR_MASK)
        printf("read addr: %p larger than 6B\n", addr);

    buf_tail[threadid] += sizeof(struct RWlog);

    PIN_MutexUnlock(&mutexs[cpu]);

    ++(ins_cnt[threadid]);
    if (ins_cnt[threadid] % 100000000 == 0)
	    printf("core[%d] instr[%lu]\n", threadid, ins_cnt[threadid]);
}

// Print a memory write record
VOID RecordMemWrite(VOID * ip, VOID * addr, THREADID threadid)
{
	unsigned int cpu;
    syscall(SYS_getcpu, &cpu, NULL, NULL);
    threadid = cpu;

    struct timeval ts;
    gettimeofday(&ts, NULL);
    prev_timestamp[threadid] = ts.tv_sec * 1000000 + ts.tv_usec;

    PIN_MutexLock(&mutexs[cpu]);

    char *p = buf_tail[threadid];
    char *limit = buf[threadid] + BUF_SIZE_PER_THREAD;
    if ((unsigned long)(p + sizeof(struct RWlog)) > (unsigned long)limit - 1)
        flush_buf(threadid);

    struct RWlog *log = (struct RWlog *)(buf_tail[threadid]);
    log->addr = (unsigned long)addr & MMAP_ADDR_MASK;
    log->op = 'W';
    log->usec = prev_timestamp[threadid];

    if ((unsigned long)addr > MMAP_ADDR_MASK)
	    printf("write addr: %p larger than 6B\n", addr);

    buf_tail[threadid] += sizeof(struct RWlog);

    PIN_MutexUnlock(&mutexs[cpu]);

    ++(ins_cnt[threadid]);
    if (ins_cnt[threadid] % 100000000 == 0)
	    printf("core[%d] instr[%lu]\n", cpu, ins_cnt[threadid]);
}


VOID RecordMmap(THREADID threadid, void *start, size_t real_len, int prot, int flags, int fs, off_t offset) {
	unsigned int cpu;
	syscall(SYS_getcpu, &cpu, NULL, NULL);
	threadid = cpu;

    struct timeval ts;
    gettimeofday(&ts, NULL);
    unsigned long new_t = ts.tv_sec * 1000000 + ts.tv_usec;
    size_t len = real_len;
    char *p, *limit;
    struct Mlog *log;

mmap_again:

    PIN_MutexLock(&mutexs[cpu]);

    p = buf_tail[threadid];
    limit = buf[threadid] + BUF_SIZE_PER_THREAD;
    if ((unsigned long)(p + sizeof(struct Mlog)) > (unsigned long)limit - 1)
        flush_buf(threadid);

    log = (struct Mlog *)(buf_tail[threadid]);
    log->hdr.op = 'M';
    log->hdr.usec = new_t - prev_timestamp[threadid];
    prev_timestamp[threadid] = new_t;
    log->start = (unsigned long)start & MMAP_ADDR_MASK;
    log->len = len & MMAP_MAX_LEN;

    buf_tail[threadid] += sizeof(struct Mlog);

    PIN_MutexUnlock(&mutexs[cpu]);

    if ((unsigned long)start > MMAP_ADDR_MASK)
	    printf("mmap addr: %p larger than 6B\n", start);

    if (len > MMAP_MAX_LEN) {
	    len -= MMAP_MAX_LEN;
	    goto mmap_again;
    }
}

VOID RecordBrk(THREADID threadid, void *addr) {
	unsigned int cpu;
	syscall(SYS_getcpu, &cpu, NULL, NULL);
	threadid = cpu;

    struct timeval ts;
    gettimeofday(&ts, NULL);
    prev_timestamp[threadid] = ts.tv_sec * 1000000 + ts.tv_usec;

    PIN_MutexLock(&mutexs[cpu]);

    char *p = buf_tail[threadid];
    char *limit = buf[threadid] + BUF_SIZE_PER_THREAD;
    if ((unsigned long)(p + sizeof(struct Blog)) > (unsigned long)limit - 1)
        flush_buf(threadid);

    struct Blog *log = (struct Blog *)(buf_tail[threadid]);
    log->addr = (unsigned long)addr & MMAP_ADDR_MASK;
    log->op = 'B';
    log->usec = prev_timestamp[threadid];

    buf_tail[threadid] += sizeof(struct Blog);

    PIN_MutexUnlock(&mutexs[cpu]);

    if ((unsigned long)addr > MMAP_ADDR_MASK)
	    printf("brk addr: %p larger than 6B\n", addr);
}

VOID RecordMunmap(THREADID threadid, void *start, size_t real_len) {
	unsigned int cpu;
	syscall(SYS_getcpu, &cpu, NULL, NULL);
	threadid = cpu;

    struct timeval ts;
    gettimeofday(&ts, NULL);
    unsigned long new_t = ts.tv_sec * 1000000 + ts.tv_usec;
    size_t len = real_len;
    char *p, *limit;
    struct Ulog *log;

munmap_again:

    PIN_MutexLock(&mutexs[cpu]);

    p = buf_tail[threadid];
    limit = buf[threadid] + BUF_SIZE_PER_THREAD;
    if ((unsigned long)(p + sizeof(struct Ulog)) > (unsigned long)limit - 1)
        flush_buf(threadid);

    log = (struct Ulog *)(buf_tail[threadid]);
    log->hdr.op = 'U';
    log->hdr.usec = new_t - prev_timestamp[threadid];
    prev_timestamp[threadid] = new_t;
    log->start = (unsigned long)start & MMAP_ADDR_MASK;
    log->len = len & MMAP_MAX_LEN;

    buf_tail[threadid] += sizeof(struct Ulog);

    PIN_MutexUnlock(&mutexs[cpu]);

    if ((unsigned long)start > MMAP_ADDR_MASK)
	    printf("munmap addr: %p larger than 6B\n", start);

    if (len > MMAP_MAX_LEN) {
            len -= MMAP_MAX_LEN;
            goto munmap_again;
    }
}

VOID RecordMremap(THREADID threadid, void *start, size_t old_len, size_t new_len, int flags) {
	printf("mremap detected\n");
    return;
}


// Is called for every instruction and instruments reads and writes
VOID Instruction(INS ins, VOID *v)
{
    // Instruments memory accesses using a predicated call, i.e.
    // the instrumentation is called iff the instruction will actually be executed.
    //
    // On the IA-32 and Intel(R) 64 architectures conditional moves and REP 
    // prefixed instructions appear as predicated instructions in Pin.
    UINT32 memOperands = INS_MemoryOperandCount(ins);

    // Iterate over each memory operand of the instruction.
    for (UINT32 memOp = 0; memOp < memOperands; memOp++)
    {
        if (INS_MemoryOperandIsRead(ins, memOp))
        {
            INS_InsertPredicatedCall(
                ins, IPOINT_BEFORE, (AFUNPTR)RecordMemRead,
                IARG_INST_PTR,
                IARG_MEMORYOP_EA, memOp,
                IARG_THREAD_ID,
                IARG_END);
        }
        // Note that in some architectures a single memory operand can be 
        // both read and written (for instance incl (%eax) on IA-32)
        // In that case we instrument it once for read and once for write.
        if (INS_MemoryOperandIsWritten(ins, memOp))
        {
            INS_InsertPredicatedCall(
                ins, IPOINT_BEFORE, (AFUNPTR)RecordMemWrite,
                IARG_INST_PTR,
                IARG_MEMORYOP_EA, memOp,
		IARG_THREAD_ID,
                IARG_END);
        }
    }
}

// This routine is executed for each image.
VOID ImageLoad(IMG img, VOID *)
{
    for (int i = 0; i < NUM_MM_SYSCALLS; ++i) {
	RTN rtn = RTN_FindByName(img, mm_syscalls[i].c_str());
    
  	if ( RTN_Valid( rtn ))
    	{
	    //printf("found rtn %s\n", mm_syscalls[i].c_str());
	    RTN_Open(rtn);
	   
	    if (mm_syscalls[i] == "mmap") 
	    	RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(RecordMmap),
		       IARG_THREAD_ID,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 3,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 4,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 5,
                       IARG_END);
	    else if (mm_syscalls[i] == "brk") 
	    	RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(RecordBrk),
		       IARG_THREAD_ID,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_END);
	    else if (mm_syscalls[i] == "munmap") 
	    	RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(RecordMunmap),
		       IARG_THREAD_ID,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                       IARG_END);
	    else if (mm_syscalls[i] == "mremap") 
	    	RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(RecordMremap),
		       IARG_THREAD_ID,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 3,
                       IARG_END);
	    RTN_Close(rtn);
    	}
    }
}

VOID Fini(INT32 code, VOID *v)
{
    //fprintf(trace, "#eof\n");
    for (int threadIndex = 0; threadIndex < MAX_NUM_THREADS; ++threadIndex) {
		flush_buf(threadIndex);
        	write(trace[threadIndex], "#eof\n", strlen("#eof\n"));
		close(trace[threadIndex]);
		trace[threadIndex] = 0;
		free(buf[threadIndex]);
		buf[threadIndex] = NULL;
		buf_tail[threadIndex] = NULL;
	    PIN_MutexFini(&mutexs[threadIndex]);
    }
}


VOID ThreadStart(THREADID threadid, CONTEXT *ctxt, INT32 flags, VOID *v)
{
	printf("thread %d start\n", threadid);
}


// This function is called when the thread exits
VOID ThreadFini(THREADID threadIndex, const CONTEXT *ctxt, INT32 code, VOID *v)
{
	printf("thread %d finish\n", threadIndex);
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */
   
INT32 Usage()
{
    PIN_ERROR( "This Pintool prints a trace of memory addresses\n" 
              + KNOB_BASE::StringKnobSummary() + "\n");
    return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */


int main(int argc, char *argv[])
{
    if (PIN_Init(argc, argv)) return Usage();
    PIN_InitSymbols();

    //TODO make MAX_NUM_THREADS configurable
    for (int threadid = 0; threadid < MAX_NUM_THREADS; ++threadid) {
		char tname[32];
		sprintf(tname, "%u", threadid);
        //TODO configurabale dirpath
		trace[threadid] = open((trace_dir + std::string("original/") + std::string(tname)).c_str(),
            O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
        if (trace[threadid] < 0) {
            printf("fail to open trace file[%d] errno[%d]\n", threadid, errno);
            return -1;
        }

		buf[threadid] = (char *)malloc(BUF_SIZE_PER_THREAD);
        if (!buf[threadid]) {
            printf("fail to allocate buffer\n");
            return -1;
        }
		buf_tail[threadid] = buf[threadid];
        prev_timestamp[threadid] = 0;
		ins_cnt[threadid] = 0;
	    PIN_MutexInit(&mutexs[threadid]);
    }

    mm_syscalls[0] = "mmap";
    mm_syscalls[1] = "munmap";
    mm_syscalls[2] = "mremap";
    mm_syscalls[3] = "brk";

    // Register ThreadStart to be called when a thread starts.
    PIN_AddThreadStartFunction(ThreadStart, NULL);
    // Register Fini to be called when thread exits.
    PIN_AddThreadFiniFunction(ThreadFini, NULL);

    INS_AddInstrumentFunction(Instruction, 0);
    //RTN_AddInstrumentFunction(Routine, 0);
    IMG_AddInstrumentFunction(ImageLoad, 0);

    PIN_AddFiniFunction(Fini, 0);
    // Never returns
    PIN_StartProgram();
    
    return 0;
}
