; FUNCTION: TH10 0x00420470
; Registers one calc callback and three draw callbacks with g_MainChainContext.
BITS 32

SECTION .text ALIGN=4
GLOBAL FUN_00420470

EXTERN FUN_0041ff80
EXTERN FUN_00420000
EXTERN FUN_004200c0
EXTERN FUN_004200d0
EXTERN LAB_004201b0
EXTERN DAT_00491fb4
EXTERN DAT_00491fb8
EXTERN DAT_00491fc0
EXTERN DAT_00491be4
EXTERN DAT_00491c28
EXTERN FUN_00449ed0
EXTERN FUN_00449ae0
EXTERN FUN_00449b70

FUN_00420470:
    push ebx
    push ebp
    push esi
    push edi
    db 0x33, 0xc0                 ; xor eax, eax
    push FUN_0041ff80
    mov dword [DAT_00491fb4], 0xfffffffe
    mov [DAT_00491fb8], eax
    mov [DAT_00491fc0], eax
    call FUN_00449ed0
    mov esi, [eax + 4]
    mov ecx, [DAT_00491be4]
    mov ebx, 2
    db 0x0b, 0xf3                 ; or esi, ebx
    mov [eax + 4], esi
    mov ebp, DAT_00491c28
    push ecx
    mov edi, 1
    db 0x8b, 0xf0                 ; mov esi, eax
    mov [eax + 0x20], ebp
    mov dword [eax + 0xc], LAB_004201b0
    call FUN_00449ae0
    test eax, eax
    jnz .return
    push FUN_00420000
    call FUN_00449ed0
    mov edi, [eax + 4]
    mov edx, [DAT_00491be4]
    db 0x0b, 0xfb                 ; or edi, ebx
    mov [eax + 4], edi
    push edx
    mov edi, 1
    db 0x8b, 0xf0                 ; mov esi, eax
    mov [eax + 0x20], ebp
    call FUN_00449b70
    push FUN_004200c0
    call FUN_00449ed0
    or dword [eax + 4], ebx
    mov ecx, [DAT_00491be4]
    push ecx
    mov edi, 0x28
    db 0x8b, 0xf0                 ; mov esi, eax
    mov [eax + 0x20], ebp
    call FUN_00449b70
    push FUN_004200d0
    call FUN_00449ed0
    or dword [eax + 4], ebx
    mov edx, [DAT_00491be4]
    push edx
    mov edi, 0x32
    db 0x8b, 0xf0                 ; mov esi, eax
    mov [eax + 0x20], ebp
    call FUN_00449b70
    db 0x33, 0xc0                 ; xor eax, eax
.return:
    pop edi
    pop esi
    pop ebp
    pop ebx
    ret
