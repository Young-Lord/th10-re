; FUNCTIONS: TH10 0x0041ff30, 0x0041ff40
; Both use EAX as the object/base pointer. Class ownership is not established.
BITS 32

SECTION .text ALIGN=4
GLOBAL FUN_0041ff30
GLOBAL FUN_0041ff40

FUN_0041ff30:
    mov [eax + 0x390], ecx
    ret

FUN_0041ff40:
    mov eax, [eax + 0x150]
    shr eax, 4
    and eax, 1
    ret
