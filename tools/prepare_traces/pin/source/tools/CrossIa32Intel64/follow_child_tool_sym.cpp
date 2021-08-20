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

#include "pin.H"
#include <iostream>

using std::cout;
using std::endl;

BOOL FollowChild(CHILD_PROCESS childProcess, VOID * userData)
{

    cout << "At follow child callback" << endl;
    return TRUE;
}



int main(INT32 argc, CHAR **argv)
{
    // Implies use of external symbol server
    PIN_InitSymbols();

    PIN_Init(argc, argv);

    cout << "In parent_tool PinTool is probed " << decstr(PIN_IsProbeMode()) << endl;


    PIN_AddFollowChildProcessFunction(FollowChild, 0);

 
    // Never returns
    if ( PIN_IsProbeMode() )
    {
        PIN_StartProgramProbed();
    }
    else
    {
        PIN_StartProgram();
    }

    return 0;
}
