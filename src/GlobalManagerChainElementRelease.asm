; FUNCTION: TH10 0x0041ff60
; EAX is a base pointer. Releases the chain element stored at EAX + 0x62c.
BITS 32

SECTION .text ALIGN=4
GLOBAL FUN_0041ff60

EXTERN FUN_0044c150
EXTERN PTR_FUN_004703e4

FUN_0041ff60:
    push esi
    db 0x8b, 0xf0                 ; mov esi, eax
    add esi, 0x62c
    mov dword [esi], PTR_FUN_004703e4
    call FUN_0044c150
    pop esi
    ret
