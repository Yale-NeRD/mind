; Copyright 2002-2020 Intel Corporation.
; 
; This software is provided to you as Sample Source Code as defined in the accompanying
; End User License Agreement for the Intel(R) Software Development Products ("Agreement")
; section 1.L.
; 
; This software and the related documents are provided as is, with no express or implied
; warranties, other than those that are expressly stated in the License.

PUBLIC TestDfByMovsd


.686
.model flat, c
extern source:word
extern dest:word
.code
TestDfByMovsd PROC
    push       esi
    push       edi
    lea        esi, source
    add        esi, 4
    lea        edi, dest
    add        edi, 4
    movsd
    movsd
    pop        edi
    pop        esi
    ret

TestDfByMovsd ENDP

end