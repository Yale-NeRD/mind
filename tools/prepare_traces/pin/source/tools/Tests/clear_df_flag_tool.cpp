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

// This tool inserts the cld instruction before each instruction.

#include "pin.H"


extern "C"
{
    extern void cleardf();
}


VOID Instruction(INS ins, VOID *v)
{
    
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)cleardf, IARG_FAST_ANALYSIS_CALL, IARG_END);
}


// argc, argv are the entire command line, including pin -t <toolname> -- ...
int main(int argc, char * argv[])
{
    // Initialize pin
    PIN_Init(argc, argv);

    // Register Instruction to be called to instrument instructions
    INS_AddInstrumentFunction(Instruction, 0);
    
    // Start the program, never returns
    PIN_StartProgram();
    
    return 0;
}
