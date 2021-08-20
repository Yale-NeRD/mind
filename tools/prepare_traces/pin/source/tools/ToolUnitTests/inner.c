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


// The tool callapp9.cpp and application inner.c showed a bug with the way REG_INST_G0 
// is handled.  That register should have the value "1" when executing code
// from PIN_CallApplicationFunction() and the value "0" when executing other
// code. However, after execution returns from the replaced function, REG_INST_G0
// continued to have the value "1". The correct behavior is is for REG_INST_G0
// to have the value "0" when it returns from the replacement function.
//
// The correct output is:
// At Inner G0=0
// Calling replaced Replaced()
// REPLACE_Replaced: REG_INST_G0=1
// At Inner G0=1
// REPLACE_Replaced: REG_INST_G0=1
// Returning from replaced Replaced()
// i=2
// At Inner G0=0

#include <stdio.h>

#if defined (TARGET_WINDOWS)
#define EXPORT_SYM __declspec( dllexport ) 
#else
#define EXPORT_SYM extern
#endif

EXPORT_SYM int Replaced();
EXPORT_SYM void Inner();

int Glob = 0;

int main()
{
    int i;

    Inner();
    i = Replaced();
    printf("i=%d\n", i);
    Inner();

    return 0;
}

EXPORT_SYM int Replaced()
{
    Inner();
    return Glob;
}

EXPORT_SYM void Inner()
{
    Glob++;
}
