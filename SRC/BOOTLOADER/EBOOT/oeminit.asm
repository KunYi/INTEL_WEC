;
; Copyright (c) Microsoft Corporation.  All rights reserved.
;
;
; Use of this sample source code is subject to the terms of the Microsoft
; license agreement under which you licensed this sample source code. If
; you did not accept the terms of the license agreement, you are not
; authorized to use this sample source code. For the terms of the license,
; please see the license agreement between you and Microsoft or, if applicable,
; see the LICENSE.RTF on your install media or the root of your tools installation.
; THE SAMPLE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
;
;-------------------------------------------------------------------------------
;
;       THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
;       ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
;       THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
;       PARTICULAR PURPOSE.
;  
;-------------------------------------------------------------------------------
.486p
.model FLAT
OFFSET32 EQU <OFFSET FLAT:>

;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
.code

STACK_SIZE     EQU 4000h
        db      STACK_SIZE dup ( 0 )
STACK_START    equ     $        
 
align 4

;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
_BootUp PROC NEAR PUBLIC

        mov     esp, OFFSET32 STACK_START
        EXTRN   _BootloaderMain:FAR
        mov     eax, OFFSET _BootloaderMain
        jmp     eax


_BootUp ENDP

;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
StartUp PROC NEAR C PUBLIC

        cli
        cld
        
        mov ebx, OFFSET CacheEnabled
        mov     eax, cr0
        and eax, not 060000000h     ; clear cache disable & cache write-thru bits
        mov     cr0, eax
        jmp ebx
        align   4
CacheEnabled:
    
        wbinvd  ; clear out the cache
 
        mov eax, OFFSET _BootUp
        jmp     eax
        

StartUp ENDP

    END
