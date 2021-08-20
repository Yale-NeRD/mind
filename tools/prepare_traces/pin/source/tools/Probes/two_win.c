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

#include <stdio.h>
#include <windows.h>

__declspec(dllexport) int one()
{
    // make the literal 2 be part of the code
    fprintf(stderr,"%d\n", 2);
    fflush(stderr);

    return 2;
}


BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason,
        LPVOID lpvReserved)
{
  return 1;
}
