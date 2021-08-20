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

// This little application is used to test calling application functions.
//
#include <stdio.h>

#if defined (TARGET_WINDOWS)
#define EXPORT_SYM __declspec( dllexport ) 
#else
#define EXPORT_SYM extern
#endif

EXPORT_SYM char Bar12( int, int, unsigned int, signed char, signed char, unsigned char,
                       int, int, unsigned int, signed char, signed char, unsigned char );

int main()
{
    Bar12(6, -2, 4, 'z', 't', 'p', -7, 1, 3, 's', 'd', 'f');

    return 0;
}
