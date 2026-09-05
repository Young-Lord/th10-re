; FUNCTION: TH10 0x0041f7a0
; ECX = Float2* point, [ESP+4] = x_radius, [ESP+8] = y_radius.
BITS 32

SECTION .text ALIGN=4
GLOBAL FUN_0041f7a0

EXTERN DAT_00470b04
EXTERN DAT_00470b38
EXTERN DAT_00470b3c
EXTERN DAT_00470b40

FUN_0041f7a0:
    fld dword [esp + 4]
    fadd dword [ecx]
    fcomp dword [DAT_00470b40]
    fnstsw ax
    test ah, 0x41
    jnp .outside
    fld dword [ecx]
    fsub dword [esp + 4]
    fcomp dword [DAT_00470b3c]
    fnstsw ax
    test ah, 1
    jz .outside
    fld dword [esp + 8]
    fadd dword [ecx + 4]
    fcomp dword [DAT_00470b04]
    fnstsw ax
    test ah, 0x41
    jnp .outside
    fld dword [ecx + 4]
    fsub dword [esp + 8]
    fcomp dword [DAT_00470b38]
    fnstsw ax
    test ah, 1
    jz .outside
    db 0x33, 0xc0                 ; xor eax, eax
    ret 8
.outside:
    mov eax, 1
    ret 8
