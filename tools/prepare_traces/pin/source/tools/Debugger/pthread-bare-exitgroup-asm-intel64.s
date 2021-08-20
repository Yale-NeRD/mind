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

    .text
.globl BareExitGroup
BareExitGroup:
    movq    $0,%rdi     # first argument: exit code
#if defined(TARGET_MAC)
    movq    $0x2000001,%rax    # system call number (exit) - exit process
#elif defined(TARGET_LINUX)
    movq    $231,%rax   # system call number (sys_exit_group) - exit process
#endif

.globl BareExitTrap
BareExitTrap:
    syscall             # call kernel
