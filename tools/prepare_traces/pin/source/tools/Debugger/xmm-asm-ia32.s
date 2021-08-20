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

    .data
    .align 16
xmmval:
    .quad   0x123456789abcdef0
    .quad   0xff00ff000a550a55

    .text
    .align 4
.globl DoXmm
.type DoXmm, @function
DoXmm:
    movl        $1, %eax
    cvtsi2ss    %eax, %xmm0     /* %xmm0 = 1.0 (in lower 32-bits) */
    movl        $2, %eax
    cvtsi2ss    %eax, %xmm1     /* %xmm1 = 2.0 (in lower 32-bits) */
    lea         xmmval, %eax
    movdqa      (%eax), %xmm2
	nop
    ret
.globl ZeroXmms
.type ZeroXmms, @function
ZeroXmms:
    pxor       %xmm0, %xmm0
    pxor       %xmm1, %xmm1
    pxor       %xmm2, %xmm2
	pxor       %xmm3, %xmm3
    ret

