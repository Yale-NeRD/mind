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
    .align 4
.globl LoadYmm0
.type LoadYmm0, @function
LoadYmm0:
    mov     4(%esp), %ecx

    /*
     * This is "VMOVDQU (%ecx), %ymm0".  We directly specify the machine code,
     * so this test runs even when the compiler doesn't support AVX.
     */
    .byte   0xc5, 0xfe, 0x6f, 0x01

.globl LoadYmm0Breakpoint
LoadYmm0Breakpoint:         /* Debugger puts a breakpoint here */
    ret
