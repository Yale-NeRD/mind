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

#if defined(TARGET_WINDOWS)
#include "windows.h"
#define EXPORT_CSYM extern "C" __declspec( dllexport )
#else
#define EXPORT_CSYM extern "C"
#endif
#include <stdio.h>
EXPORT_CSYM int probed_func_asm(int x);
EXPORT_CSYM int probed_func(int x);

EXPORT_CSYM int main (int argc, char *argv[])
{
    int retVal;
    printf ("calling probed func\n");
    fflush (stdout);
    retVal = probed_func (0);
    printf ("probed func returned %d\n", retVal);
    fflush (stdout);
    printf ("calling probed func\n");
    fflush (stdout);
    retVal = probed_func (1);
    printf ("probed func returned %d\n", retVal);
    fflush (stdout);
}
