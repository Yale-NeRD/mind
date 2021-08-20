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

/* ===================================================================== */
/*! @file
 * Insert a call before a function in probe mode.
 */

/* ===================================================================== */
#include "pin.H"
#include <cstdlib>
#include <iostream>
#include "tool_macros.h"
using std::dec;
using std::flush;
using std::hex;
using std::cout;
using std::endl;


/* ===================================================================== */
/* Analysis routines  */
/* ===================================================================== */

VOID Before_Malloc( size_t size )
{
    cout << "Before_Malloc: Calling malloc() with size=" << size << endl << flush;
}

VOID After_Malloc( ADDRINT retval )
{
    cout << "After_Malloc: malloc() return value=" << hex << retval
         << dec << endl << flush;
}

VOID Before_Free( void * ptr )
{
    cout << "Before_Free: Calling free() with ptr=" << hex <<
        (ADDRINT)ptr << dec << endl << flush;
}

VOID After_Free()
{
    cout << "After_Free: returning from free()." << endl << flush;
}



/* ===================================================================== */
/* Instrumentation routines  */
/* ===================================================================== */

VOID Sanity(IMG img, RTN rtn)
{
    if ( PIN_IsProbeMode() && ! RTN_IsSafeForProbedInsertion( rtn ) )
    {
        cout << "Cannot insert calls around " << RTN_Name(rtn) <<
            "() in " << IMG_Name(img) << endl;
        exit(1);
    }
}

/* ===================================================================== */
VOID ImageLoad(IMG img, VOID *v)
{
    RTN rtn = RTN_FindByName(img, C_MANGLE("malloc"));
    if (RTN_Valid(rtn))
    {
        Sanity(img, rtn);
        
        cout << "Inserting calls before/after malloc in " << IMG_Name(img) << endl;

        PROTO proto_malloc = PROTO_Allocate( PIN_PARG(void *), CALLINGSTD_DEFAULT,
                                             "malloc", PIN_PARG(size_t), PIN_PARG_END() );
        
        RTN_InsertCallProbed(
            rtn, IPOINT_BEFORE, AFUNPTR( Before_Malloc ),
            /*IARG_PROTOTYPE, proto_malloc,*/
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
            IARG_END);

        RTN_InsertCallProbed(
            rtn, IPOINT_AFTER, AFUNPTR( After_Malloc ),
            IARG_PROTOTYPE, proto_malloc,
            IARG_REG_VALUE, REG_GAX,
            IARG_END);

        PROTO_Free( proto_malloc );
    }

    rtn = RTN_FindByName(img, C_MANGLE("free"));
    if (RTN_Valid(rtn))
    {
        Sanity(img, rtn);
        
        cout << "Inserting calls before/after free in " << IMG_Name(img) << endl;

        PROTO proto_free = PROTO_Allocate( PIN_PARG(void), CALLINGSTD_DEFAULT,
                                             "free", PIN_PARG(void *), PIN_PARG_END() );
        
        RTN_InsertCallProbed(
            rtn, IPOINT_BEFORE, AFUNPTR( Before_Free ),
            /*IARG_PROTOTYPE, proto_free,*/
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
            IARG_END);

        RTN_InsertCallProbed(
            rtn, IPOINT_AFTER, AFUNPTR( After_Free ),
            IARG_PROTOTYPE, proto_free,
            IARG_END);

        PROTO_Free( proto_free );
    }
}


/* ===================================================================== */

int main(INT32 argc, CHAR *argv[])
{
    PIN_InitSymbols();
    
    PIN_Init(argc, argv);
    
    IMG_AddInstrumentFunction(ImageLoad, 0);
    
    PIN_StartProgramProbed();
    
    return 0;
}



/* ===================================================================== */
/* eof */
/* ===================================================================== */
