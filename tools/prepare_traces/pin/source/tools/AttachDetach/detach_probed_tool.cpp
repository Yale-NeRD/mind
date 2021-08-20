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

/*! @file
 */

#include "pin.H"
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <assert.h>
#include "tool_macros.h"

/* ===================================================================== */
/* Commandline Switches */
/* ===================================================================== */

unsigned long *updateWhenReadyPtr = 0;

KNOB<BOOL> KnobCallReplaceSignatureProbed(KNOB_MODE_WRITEONCE, "pintool", "replace_signature_probed", "0",
        "Use ReplaceSignatureProbed() API instead of ReplaceProbed()");

// Replacement function for RTN_ReplaceSignatureProbed() without alignment constrains
VOID DetachPinFromMTApplication_WithoutAlignment(unsigned long *updateWhenReady)
{
    updateWhenReadyPtr = updateWhenReady;
    fprintf(stderr, "Pin tool: sending detach request\n");
    PIN_DetachProbed();
}

// Replacement function for RTN_ReplaceProbed() with alignment constrains in Linux 32-bit
#if defined(TARGET_LINUX) && defined(TARGET_IA32)
// Request the compiler to align the stack to 16 bytes boundary, since this
// function might called with application stack which might not be aligned properly
__attribute__((force_align_arg_pointer))
#endif
VOID DetachPinFromMTApplication_WithAlignment(unsigned long *updateWhenReady)
{
    updateWhenReadyPtr = updateWhenReady;
    fprintf(stderr, "Pin tool: sending detach request\n");
    PIN_DetachProbed();
}

VOID DetachCompleted(VOID *v)
{
    fprintf(stderr, "Pin tool: detach is completed\n");
    *updateWhenReadyPtr = 1;
}


VOID ImageLoad(IMG img, void *v)
{
    RTN rtn = RTN_FindByName(img, C_MANGLE("TellPinToDetach"));
    if (RTN_Valid(rtn))
    {
        if (KnobCallReplaceSignatureProbed)
        {
            PROTO proto_func = PROTO_Allocate( PIN_PARG(void), CALLINGSTD_DEFAULT,
                                       "TellPinToDetach", PIN_PARG(unsigned long*), PIN_PARG_END());

            RTN_ReplaceSignatureProbed(rtn, AFUNPTR(DetachPinFromMTApplication_WithoutAlignment),
                                                   IARG_PROTOTYPE, proto_func,
                                                   IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                                                   IARG_END);
        }
        else
        {
            RTN_ReplaceProbed(rtn, AFUNPTR(DetachPinFromMTApplication_WithAlignment));
        }
    }
}
/* ===================================================================== */

int main(int argc, CHAR *argv[])
{
    PIN_InitSymbols();

    PIN_Init(argc,argv);

    IMG_AddInstrumentFunction(ImageLoad, 0);
    PIN_AddDetachFunctionProbed(DetachCompleted, 0);
    PIN_StartProgramProbed();
    
    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
