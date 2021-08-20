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

// Application that creates new process

#include <Windows.h>
#include <iostream>
#include <string>
using std::endl;
using std::cout;

//Wait for a process completion
//Verify it returned the expected exit code
bool WaitAndVerify(HANDLE process)
{
    if(WaitForSingleObject( process, INFINITE ) == WAIT_FAILED)
    {
        cout << "WaitForSingleObject failed" << endl;
        return FALSE;
    }
    DWORD processExitCode;
    if(GetExitCodeProcess (process, &processExitCode) == FALSE)
    {
        cout << "GetExitCodeProcess Failed" << endl;
        return FALSE;
    }
    if(processExitCode != 0)
    {
        cout << "Got unexpected exit code" << endl;
        return FALSE;
    }
    return TRUE;
}

int main(int argc, char * argv[])
{
    char cmd[] = "win_child_process.exe \"param1 param2\" param3";
    STARTUPINFO         si;
    PROCESS_INFORMATION  pi;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(STARTUPINFO);
    memset(&pi, 0, sizeof(pi));
    if (!CreateProcess(NULL,cmd  , NULL, NULL, TRUE, NULL, 
        NULL, NULL, &si, &pi))
    {
        cout <<  "Couldn't create child process " << endl;
        exit(0);
    }
    if(WaitAndVerify(pi.hProcess) == FALSE)
    {
        exit(0);
    }
    cout << "First Process was created successfully!" << endl;
    
    //Create suspended
    if (!CreateProcess(NULL,cmd  , NULL, NULL, TRUE, CREATE_SUSPENDED, 
        NULL, NULL, &si, &pi))
    {
        cout <<  "Couldn't create child process " << endl;
        exit(0);
    }
    ResumeThread( pi.hThread );

    if(WaitAndVerify(pi.hProcess) == FALSE)
    {
        exit(0);
    }  
    cout << "Second Process was created successfully!" << endl;

    //Create process as user
    HANDLE tokenHandle;
    BOOL res = OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &tokenHandle);
    if(!res)
    {
        cout <<  "Couldn't open process token" << endl;
        exit(0);
    }
    if (!CreateProcessAsUser(tokenHandle, NULL,cmd  , NULL, NULL, TRUE, CREATE_SUSPENDED, 
        NULL, NULL, &si, &pi))
    {
        cout <<  "Couldn't create child process " << endl;
        exit(0);
    }
    ResumeThread( pi.hThread );

    if(WaitAndVerify(pi.hProcess) == FALSE)
    {
        exit(0);
    }  
    cout << "Third Process was created successfully!" << endl;

    
    Sleep(1000);

    return 0;
}

