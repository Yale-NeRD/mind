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

PUBLIC FldzFunc
PUBLIC Fld1Func
PUBLIC FptagInitFunc
PUBLIC FldInfFunc
PUBLIC DoFnstenv
PUBLIC FstpFunc

.data
zero DD 0

.code

;initialize the FPU
FptagInitFunc PROC
        finit
        ret
FptagInitFunc ENDP

;push 1 to the FPU
Fld1Func PROC
        fld1
        ret
Fld1Func ENDP

;push 0 to the FPU
FldzFunc PROC
        fldz
        ret
FldzFunc ENDP

;push infinity to the FPU
FldInfFunc PROC
        fldpi
        fdiv zero
        ret
FldInfFunc ENDP

;save fpu state in PARAM1 address
DoFnstenv PROC
        BEGIN_STACK_FRAME
        mov GAX_REG,PARAM1
        fnstenv [GAX_REG]
        END_STACK_FRAME
        ret
DoFnstenv ENDP

;pop st(0) from FPU stack
FstpFunc PROC
        fstp st(0)
        ret
FstpFunc ENDP

end
