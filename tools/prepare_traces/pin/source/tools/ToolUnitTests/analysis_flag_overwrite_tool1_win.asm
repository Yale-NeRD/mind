; Copyright 2002-2020 Intel Corporation.
; 
; This software is provided to you as Sample Source Code as defined in the accompanying
; End User License Agreement for the Intel(R) Software Development Products ("Agreement")
; section 1.L.
; 
; This software and the related documents are provided as is, with no express or implied
; warranties, other than those that are expressly stated in the License.

PUBLIC ReadWriteFlags_asm

.686
.model flat, c
.code
ReadWriteFlags_asm PROC
    pushfd       
    pop        eax
    mov        eax, 0
    add        eax, eax
    ret

ReadWriteFlags_asm ENDP

end