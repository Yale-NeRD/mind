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

struct log_header {
	char op;
        unsigned int usec;
}__attribute__((__packed__));

struct RWlog {
	struct log_header hdr;
	unsigned long addr;
}__attribute__((__packed__));

struct Mlog {
	struct log_header hdr;
	unsigned long start;
	size_t len;
	int prot;
	int flags;
	int fs;
	off_t offset;
}__attribute__((__packed__));

struct Blog {
        struct log_header hdr;
        unsigned long addr;
}__attribute__((__packed__));

struct Ulog {
        struct log_header hdr;
        unsigned long addr;
	size_t len;
}__attribute__((__packed__));

struct Elog {
        struct log_header hdr;
	unsigned long addr;
        size_t old_len;
	size_t new_len;
	int prot;
}__attribute__((__packed__));


#define MAX_NUM_THREADS 1024
#define BUF_SIZE_PER_THREAD (8 * 1024 * 1024)
#define MAX_LOG_LEN 64

const char *Rfmt = "R %lu %lx\n";
const char *Wfmt = "W %lu %lx\n";


int trace[MAX_NUM_THREADS] = {};
char *buf[MAX_NUM_THREADS] = {};
char *buf_tail[MAX_NUM_THREADS] = {};
unsigned long prev_timestamp[MAX_NUM_THREADS] = {};
unsigned long ins_cnt[MAX_NUM_THREADS] = {};

void flush_buf(THREADID threadid) {
    long size = write(trace[threadid], buf[threadid], buf_tail[threadid] - buf[threadid]);
    if (size != buf_tail[threadid] - buf[threadid])
	    printf("fail to flush file, expect to write %ld bytes, but write %ld bytes\n",
			    buf_tail[threadid] - buf[threadid], size);
    buf_tail[threadid] = buf[threadid];
}


// Print a memory read record
VOID RecordMemRead(VOID * ip, VOID * addr, THREADID threadid)
{
    struct timeval ts;
    gettimeofday(&ts, NULL);

    //printf("[%lu.%lu] t%d R %p\n",ts.tv_sec, ts.tv_usec, threadid, addr);
/*
    char *p = buf_tail[threadid];
    char *limit = buf[threadid] + BUF_SIZE_PER_THREAD;
    if ((unsigned long)(p + MAX_LOG_LEN) > (unsigned long)limit) {
        flush_buf(threadid);
    	p = buf_tail[threadid];
    }
*/
    char *p = buf_tail[threadid];
    char *limit = buf[threadid] + BUF_SIZE_PER_THREAD;
    if ((unsigned long)(p + sizeof(struct RWlog)) > (unsigned long)limit - 1)
        flush_buf(threadid);

    struct RWlog *log = (struct RWlog *)(buf_tail[threadid]);
    log->hdr.op = 'R';
    log->hdr.usec = ts.tv_sec * 1000000 + ts.tv_usec;
    log->addr = (unsigned long)addr;

    buf_tail[threadid] += sizeof(struct RWlog);
    /*
    unsigned long ct = ts.tv_sec * 1000000 + ts.tv_usec;
    unsigned long dt = ct - prev_timestamp[threadid];
    prev_timestamp[threadid] = ct;

    int ret = sprintf(p, Rfmt, dt, (unsigned long)addr);
    buf_tail[threadid] += ret;
*/
    ++(ins_cnt[threadid]);
    if (ins_cnt[threadid] % 100000000 == 0)
	    printf("%lu\n", ins_cnt[threadid]);
}

// Print a memory write record
VOID RecordMemWrite(VOID * ip, VOID * addr, THREADID threadid)
{
    struct timeval ts;
    gettimeofday(&ts, NULL);

    //printf("[%lu.%lu] t%d W %p\n",ts.tv_sec, ts.tv_usec, threadid, addr);
/*
    char *p = buf_tail[threadid];
    char *limit = buf[threadid] + BUF_SIZE_PER_THREAD;
    if ((unsigned long)(p + MAX_LOG_LEN) > (unsigned long)limit) {
        flush_buf(threadid);
    	p = buf_tail[threadid];
    }
*/
    char *p = buf_tail[threadid];
    char *limit = buf[threadid] + BUF_SIZE_PER_THREAD;
    if ((unsigned long)(p + sizeof(struct RWlog)) > (unsigned long)limit - 1)
        flush_buf(threadid);

    struct RWlog *log = (struct RWlog *)(buf_tail[threadid]);
    log->hdr.op = 'W';
    log->hdr.usec = ts.tv_sec * 1000000 + ts.tv_usec;
    log->addr = (unsigned long)addr;

    buf_tail[threadid] += sizeof(struct RWlog);
    /*
    unsigned long ct = ts.tv_sec * 1000000 + ts.tv_usec;
    unsigned long dt = ct - prev_timestamp[threadid];
    prev_timestamp[threadid] = ct;

    int ret = sprintf(p, Wfmt, dt, (unsigned long)addr);
    buf_tail[threadid] += ret;
*/
    ++(ins_cnt[threadid]);
    if (ins_cnt[threadid] % 100000000 == 0)
	    printf("%lu\n", ins_cnt[threadid]);
}


VOID RecordMmap(THREADID threadid, void *start, size_t len, int prot, int flags, int fs, off_t offset) {
    struct timeval ts;
    gettimeofday(&ts, NULL);
//    fprintf(trace[threadid],"[%lu] t%d M %p %lu %x %x %d %ld\n",ts.tv_usec, threadid,
//		    start, len, prot, flags, fs, offset);
}

VOID RecordBrk(THREADID threadid, void *addr) {
    struct timeval ts;
    gettimeofday(&ts, NULL);
//    fprintf(trace[threadid],"[%lu] t%d B %p\n",ts.tv_usec, threadid,
//		    addr);
}

VOID RecordMunmap(THREADID threadid, void *start, size_t len) {
    struct timeval ts;
    gettimeofday(&ts, NULL);
//    fprintf(trace[threadid],"[%lu] t%d U %p %lu\n",ts.tv_usec, threadid,
//		    start, len);
}

VOID RecordMremap(THREADID threadid, void *start, size_t old_len, size_t new_len, int flags) {
    struct timeval ts;
    gettimeofday(&ts, NULL);
//    fprintf(trace[threadid],"[%lu] t%d E %p %lu %lu %x\n", ts.tv_usec, threadid,
//		    start, old_len, new_len, flags);
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

const int num_mm_syscalls = 4;
std::string mm_syscalls[num_mm_syscalls];

// This routine is executed for each image.
VOID ImageLoad(IMG img, VOID *)
{
    for (int i = 0; i < num_mm_syscalls; ++i) {
	RTN rtn = RTN_FindByName(img, mm_syscalls[i].c_str());
    
  	if ( RTN_Valid( rtn ))
    	{
	    printf("found rtn %s\n", mm_syscalls[i].c_str());
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

/*
// Pin calls this function every time a new rtn is executed
VOID Routine(RTN rtn, VOID *v)
{
    RTN_Open(rtn);

    // Insert a call at the entry point of a routine to increment the call count
    RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)RecordRoutine, IARG_PTR, &(rc->_rtnCount)IARG_THREAD_ID, IARG_END);

    RTN_Close(rtn);
}
*/

VOID Fini(INT32 code, VOID *v)
{
    //fprintf(trace, "#eof\n");
    //fclose(trace);
}

VOID ThreadStart(THREADID threadid, CONTEXT *ctxt, INT32 flags, VOID *v)
{
/*
    numThreads++;
    thread_data_t* tdata = new thread_data_t;
    if (PIN_SetThreadData(tls_key, tdata, threadid) == FALSE)
    {
        cerr << "PIN_SetThreadData failed" << endl;
        PIN_ExitProcess(1);
    }
*/
	if (!trace[threadid]) {
		std::string tname = "0";
		tname[0] += threadid;
		trace[threadid] = open((std::string("/media/data/yanpeng/default/tf") + tname).c_str(), O_WRONLY|O_CREAT, 777);
		buf[threadid] = (char *)malloc(BUF_SIZE_PER_THREAD);
		buf_tail[threadid] = buf[threadid];
		struct timeval ts;
		gettimeofday(&ts, NULL);
		prev_timestamp[threadid] = ts.tv_sec * 1000000 + ts.tv_usec;
		//char t[32];
		//int res = sprintf(t, "%lu\n", prev_timestamp[threadid]);
		//write(trace[threadid], t, res);
		ins_cnt[threadid] = 0;
	}
	printf("thread %d start\n", threadid);
}


// This function is called when the thread exits
VOID ThreadFini(THREADID threadIndex, const CONTEXT *ctxt, INT32 code, VOID *v)
{
/*
    thread_data_t* tdata = static_cast<thread_data_t*>(PIN_GetThreadData(tls_key, threadIndex));
    *OutFile << "Count[" << decstr(threadIndex) << "] = " << tdata->_count << endl;
    delete tdata;
*/
	if (trace[threadIndex]) {
		flush_buf(threadIndex);
        	write(trace[threadIndex], "#eof\n", strlen("#eof\n"));
		close(trace[threadIndex]);
		trace[threadIndex] = 0;
		free(buf[threadIndex]);
		buf[threadIndex] = NULL;
		buf_tail[threadIndex] = NULL;
		prev_timestamp[threadIndex] = 0;
		ins_cnt[threadIndex] = 0;
    }
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

    //trace = fopen("pinatrace.out", "w");

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
    //IMG_AddInstrumentFunction(ImageLoad, 0);

    PIN_AddFiniFunction(Fini, 0);

    // Never returns
    PIN_StartProgram();
    
    return 0;
}
