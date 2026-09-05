; FUNCTION: TH10 0x0041f800
; ECX = Float2* output, [ESP+4] = angle, [ESP+8] = length.
BITS 32

SECTION .text ALIGN=4
GLOBAL FUN_0041f800

FUN_0041f800:
    push ecx
    mov [esp], ecx
    mov eax, [esp]
    fld dword [esp + 8]
    fsincos
    fmul dword [esp + 0xc]
    fstp dword [eax]
    fmul dword [esp + 0xc]
    fstp dword [eax + 4]
    pop ecx
    ret 8
