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

#ifdef TARGET_MAC
.global _SupportsAvx2
_SupportsAvx2:
#else
.type SupportsAvx2, @function
.global SupportsAvx2
SupportsAvx2:
#endif
    push %rbp
    mov  %rsp, %rbp
    push %rbx
    push %rcx
    push %rdx
    mov $1, %rax
    cpuid
    andq $0x18000000, %rcx
    cmpq $0x18000000, %rcx    # check both OSXSAVE and AVX feature flags
    jne NotSupported         
                       # processor supports AVX instructions and XGETBV is enabled by OS
    mov $0, %rcx       # specify 0 for XFEATURE_ENABLED_MASK register
                       # 0xd0010f is xgetbv  - result in EDX:EAX 
    .byte 0xf, 0x1, 0xd0
    andq $6, %rax
    cmpq $6, %rax      # check OS has enabled both XMM and YMM state support
    jne NotSupported
    mov $7, %rax
    mov $0, %rcx       # specify 0 for XFEATURE_ENABLED_MASK register
    cpuid
    andq $0x10, %rbx
    cmpq $0x10, %rbx    
    jne NotSupported  # no AVX2
    mov $1, %rax
done:
    pop %rdx
    pop %rcx
    pop %rbx
    leave
    ret

NotSupported:
    mov $0, %rax
    jmp done
