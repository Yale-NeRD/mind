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

/*
  This application causes exception in indirect call instruction and catches it in caller.
  The call instruction is located in code region being replaced by Pin probe.
  Pin translation should not affect propagation of the exception to the C++ exception handler.
*/
#ifdef TARGET_WINDOWS

#include <windows.h>

#endif
#include <stdio.h>

#ifdef TARGET_WINDOWS
#define FASTCALL __fastcall
#define DLLEXPORT __declspec(dllexport)
#else
#define FASTCALL 
#define DLLEXPORT 
#endif

bool destructed = false;

// cpp exceptions - Exercise windows exception mechanism 
class MyClass
{
public:
    ~MyClass() 
    { 
        destructed = true;
    }
};


static int (*pBar)() = 0;

int bar()
{
    return 0;
}
extern "C"
DLLEXPORT
int foo()
{
#if defined(TARGET_LINUX) || defined(TARGET_MAC)
    if (!pBar) throw(0);
#endif
    // May cause exception due to NULL pointer
    return pBar();
}

int main()
{
    int i = 2;
    int local = 1;

    try
    {
        MyClass ins;
        i = foo();
        local = 0;
    }
    catch(...)
    {
        // If Pin translated probed code properly, exception will reach the handler
        printf("Exception\n");
    }

    // Check that destructor was called and local var value was not changed when exception was handled
    if (!destructed || (local != 1))
    {
        return 1;
    }

    pBar = bar;

    try
    {
        i = foo();
    }
    catch(...)
    {
        // No exception expected
        printf("Exception\n");
    }

    return i;
}
