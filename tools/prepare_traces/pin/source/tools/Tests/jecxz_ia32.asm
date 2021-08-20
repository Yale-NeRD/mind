; Copyright 2002-2020 Intel Corporation.
; 
; This software is provided to you as Sample Source Code as defined in the accompanying
; End User License Agreement for the Intel(R) Software Development Products ("Agreement")
; section 1.L.
; 
; This software and the related documents are provided as is, with no express or implied
; warranties, other than those that are expressly stated in the License.

PUBLIC main

.686
.model flat,c

; This assembly file should built to an executable file
; It tests the correctness of jcxz instruction and exits
; with status 0 if everything's OK.

.code

main PROC
   mov ecx, 10000H
   xor eax,eax
   jcxz test_pass
   mov al, 1
test_pass:
   ret
main ENDP
	
end
