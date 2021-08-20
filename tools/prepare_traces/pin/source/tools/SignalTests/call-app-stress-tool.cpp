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
 * Stress test for PIN_CallApplicationFunction() while handling signals.
 */

#include <stdio.h>
#include "pin.H"

static VOID OnImage(IMG, VOID *);
static VOID OnFoo(CONTEXT *, THREADID);


BOOL FoundBar = FALSE;
ADDRINT BarAddr;


int main(int argc, char * argv[])
{
    PIN_Init(argc, argv);
    PIN_InitSymbols();

    IMG_AddInstrumentFunction(OnImage, 0);

    PIN_StartProgram();
    return 0;
}

static VOID OnImage(IMG img, VOID *)
{
    RTN rtn = RTN_FindByName(img, "PIN_TEST_FOO");
    if (RTN_Valid(rtn))
    {
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(OnFoo), IARG_CONTEXT, IARG_THREAD_ID, IARG_END);
        RTN_Close(rtn);
    }

    rtn = RTN_FindByName(img, "PIN_TEST_BAR");
    if (RTN_Valid(rtn))
    {
        BarAddr = RTN_Address(rtn);
        FoundBar = TRUE;
    }
}

static VOID OnFoo(CONTEXT *ctxt, THREADID tid)
{
    if (!FoundBar)
    {
        fprintf(stderr, "Unable to find PIN_TEST_BAR()\n");
        PIN_ExitProcess(1);
    }

    PIN_CallApplicationFunction(ctxt, tid, CALLINGSTD_DEFAULT, AFUNPTR(BarAddr), NULL, PIN_PARG_END());
    PIN_CallApplicationFunction(ctxt, tid, CALLINGSTD_DEFAULT, AFUNPTR(BarAddr), NULL, PIN_PARG_END());
}
