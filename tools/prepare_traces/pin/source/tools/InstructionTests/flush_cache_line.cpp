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

#include <iostream>
#include "tool_macros.h"
#include "pin.H"
using std::cout;
using std::cerr;
using std::string;
using std::endl;

ADDRINT FlushParm;
const UINT32 CacheLineSize = 64;
ADDRINT LowAddress = 0;
ADDRINT HighAddress = 0;

/*!
 * Print out the error message and exit the process.
 */
 VOID AbortProcess(const string & msg, unsigned long code)
{
    cerr << "Test aborted: " << msg << " with code " << code << endl;
    PIN_WriteErrorMessage(msg.c_str(), 1002, PIN_ERR_FATAL, 0);
    PIN_ExitProcess(1);
}

VOID FlushInstruction(ADDRINT add, UINT32 size)
{
    if (CacheLineSize != size)
    {
        AbortProcess("Cache flush line instruction with size!=64 !", 0);
    }
    if (FlushParm != add)
    {
        cout << std::hex << FlushParm << "    " << add << endl;
        AbortProcess("Cache flush line instruction with unexpected parm!", 0);
    }
}

VOID GetFlushParm(ADDRINT flushParm)
{
    FlushParm = flushParm;
}


/* ================================================================== */


VOID Instruction(INS ins, VOID *v)
{
    if ((INS_Address(ins) >= LowAddress) && (INS_Address(ins) <= HighAddress)
            && INS_IsCacheLineFlush(ins) && INS_IsMemoryRead(ins))
    {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)FlushInstruction,
               IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE, IARG_END);
    }
}

VOID ImageLoad(IMG img, VOID *v)
{
    if (IMG_IsMainExecutable(img))
    {
        LowAddress = IMG_LowAddress(img);
        HighAddress = IMG_HighAddress(img);

        RTN rtn = RTN_FindByName(img, C_MANGLE("TellPinFlushParm"));
        if (RTN_Valid(rtn))
        {
            RTN_Open(rtn);
            RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(GetFlushParm),
                IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_END);
            RTN_Close(rtn);
        }
    }
}


/* ================================================================== */
/*
 Initialize and begin program execution under the control of Pin
*/
int main(INT32 argc, CHAR **argv)
{
    PIN_Init(argc, argv);

    PIN_InitSymbols();

    INS_AddInstrumentFunction(Instruction, NULL);

    IMG_AddInstrumentFunction(ImageLoad, NULL);

    PIN_StartProgram();

    return 0;
}
