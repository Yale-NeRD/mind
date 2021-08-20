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
#include "pinatrace.hpp"
#include <sys/syscall.h>
#include <pthread.h>
#include <sys/mman.h>

const std::string trace_dir = "../../../../traces/";
PIN_MUTEX m;
FILE *trace;
std::string mm_syscalls[NUM_MM_SYSCALLS];
unsigned long old_brk = 0;

VOID RecordMmap(THREADID threadid, void *start, size_t real_len, int prot, int flags, int fs, off_t offset,
        void *ret) {
    //struct timeval ts;
    //gettimeofday(&ts, NULL);
    //unsigned long new_t = ts.tv_sec * 1000000 + ts.tv_usec;
    PIN_MutexLock(&m);
    //fprintf(trace, "M ts[%lu] addr[%lx] len[%lu] prot[%x] flags[%x] fd[%d] offset[%ld]\n",
    //        new_t, (unsigned long)start, real_len, prot, flags, fs, offset);
	fprintf(trace, "mmap %lu %d %lx %c%c %lx\n", real_len, fs == -1 ? 0 : 1, (unsigned long)start, 
			prot & PROT_READ ? 'r' : '-', prot & PROT_WRITE ? 'w' : '-', (unsigned long)ret);
	fflush(trace);
    PIN_MutexUnlock(&m);
}
/*
VOID MallocAfter(ADDRINT ret)
{
    PIN_MutexLock(&m);
    fprintf(trace, " %lx
    PIN_MutexUnlock(&m);
}
*/
VOID RecordBrk(THREADID threadid, void *addr) {
    //struct timeval ts;
    //gettimeofday(&ts, NULL);
    //unsigned long new_t = ts.tv_sec * 1000000 + ts.tv_usec;
    PIN_MutexLock(&m);
    //fprintf(trace, "B ts[%lu] addr[%lx]\n",
    //        new_t, (unsigned long)addr);
    unsigned long new_brk = (unsigned long)addr, len;
	if (!old_brk) {
		old_brk = new_brk;
	} else {
		if (old_brk <= new_brk) {
			len = new_brk - old_brk;
			old_brk = new_brk;
			fprintf(trace, "brk %lx %lu rw\n", (unsigned long)addr, len);
			fflush(trace);
		}
	}
    PIN_MutexUnlock(&m);
}

VOID RecordMunmap(THREADID threadid, void *start, size_t real_len) {
/*
    struct timeval ts;
    gettimeofday(&ts, NULL);
    unsigned long new_t = ts.tv_sec * 1000000 + ts.tv_usec;
    PIN_MutexLock(&m);
    fprintf(trace, "U ts[%lu] addr[%lx] len[%lu]\n",
            new_t, (unsigned long)start, real_len);
    PIN_MutexUnlock(&m);
*/
	PIN_MutexLock(&m);
	fprintf(trace, "munmap %lx %lu\n", (unsigned long)start, real_len);
	fflush(trace);
	PIN_MutexUnlock(&m);
}


// This routine is executed for each image.
VOID ImageLoad(IMG img, VOID *)
{
    for (int i = 0; i < NUM_MM_SYSCALLS; ++i) {
	    RTN rtn = RTN_FindByName(img, mm_syscalls[i].c_str());
  	    if ( RTN_Valid( rtn )) {
	        RTN_Open(rtn);
	        if (mm_syscalls[i] == "mmap") {
                /*
	    	    RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(RecordMmap),
		                IARG_THREAD_ID,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 3,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 4,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 5,
                       IARG_END);
                */
                RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)(RecordMmap),
                		IARG_THREAD_ID,
                        IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 3,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 4,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 5,
                       IARG_FUNCRET_EXITPOINT_VALUE, IARG_END);
            } else if (mm_syscalls[i] == "brk") { 
	    	    RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(RecordBrk),
		                IARG_THREAD_ID,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_END);
            } else if (mm_syscalls[i] == "munmap") { 
	    	    RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(RecordMunmap),
		                IARG_THREAD_ID,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                       IARG_END);
            }
	        RTN_Close(rtn);
    	}
    }
}


VOID Fini(INT32 code, VOID *v)
{
	fclose(trace);
	PIN_MutexFini(&m);
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
    PIN_ERROR( "This Pintool prints a trace of memory syscalls\n" 
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

	trace = fopen((trace_dir + std::string("syscall")).c_str(), "w");
	if (!trace) {
		printf("fail to open trace file\n");
		return -1;
	}
    PIN_MutexInit(&m);

    mm_syscalls[0] = "mmap";
    mm_syscalls[1] = "munmap";
    mm_syscalls[2] = "brk";

    // Register ThreadStart to be called when a thread starts.
    PIN_AddThreadStartFunction(ThreadStart, NULL);
    // Register Fini to be called when thread exits.
    PIN_AddThreadFiniFunction(ThreadFini, NULL);

    //INS_AddInstrumentFunction(Instruction, 0);
    //RTN_AddInstrumentFunction(Routine, 0);
    IMG_AddInstrumentFunction(ImageLoad, 0);

    PIN_AddFiniFunction(Fini, 0);
    // Never returns
    PIN_StartProgram();
    
    return 0;
}
