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
#include <pthread.h>
#include "tool_macros.h"

extern "C" void BareExitGroup() ASMNAME("BareExitGroup");
static void *Child(void *);


int main()
{
    pthread_t tid;
    if (pthread_create(&tid, 0, Child, 0) != 0)
    {
        std::cerr << "Unable to create thread\n";
        return 1;
    }

    for (;;);
    return 0;
}

static void *Child(void *)
{
    // Note:
    // The test which use this binary want to catch the moment in which the process exit/terminate.
    // For PinDB we could just use 'exitgroup' (Linux) / 'exit' (macOS) in order to catch this  moment since PinDB has a special
    // breakpoint for it, but for a regular debugger the easiest way to do it is to write our own assembly function
    // which terminate the process and place a breakpoint on it (right before the system call)
    BareExitGroup();
    return 0;
}
