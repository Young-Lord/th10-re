; FUNCTION: TH10 0x0041fef0
; ECX is forwarded as the sole stack argument to the global manager frame handler.
BITS 32

SECTION .text ALIGN=4
GLOBAL FUN_0041fef0

EXTERN FUN_0041fdd0

FUN_0041fef0:
    push ecx
    call FUN_0041fdd0
    ret
