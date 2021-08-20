; Copyright 2002-2020 Intel Corporation.
; 
; This software is provided to you as Sample Source Code as defined in the accompanying
; End User License Agreement for the Intel(R) Software Development Products ("Agreement")
; section 1.L.
; 
; This software and the related documents are provided as is, with no express or implied
; warranties, other than those that are expressly stated in the License.

PUBLIC TestIpRead
PUBLIC TestIpWrite
PUBLIC Dummy


.code
TestIpRead PROC

    mov        eax, DWORD PTR [mylabelR]
mylabelR:
    
    ret

TestIpRead ENDP

TestIpWrite PROC

    mov        BYTE PTR [mylabelW], 90H
mylabelW:
    nop
    
    ret

TestIpWrite ENDP


Dummy PROC

    ret

Dummy ENDP

end
