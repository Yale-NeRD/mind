; Copyright 2002-2020 Intel Corporation.
; 
; This software is provided to you as Sample Source Code as defined in the accompanying
; End User License Agreement for the Intel(R) Software Development Products ("Agreement")
; section 1.L.
; 
; This software and the related documents are provided as is, with no express or implied
; warranties, other than those that are expressly stated in the License.

include asm_macros.inc

PROLOGUE

PUBLIC Proc1


.code


Proc1 PROC
 mov GCX_REG, GAX_REG
 shl GDX_REG, CL_REG 
 ret 
Proc1 ENDP



end
