; FUNCTION: TH10 0x0041ff80
; ECX is a context pointer; its concrete type is not established.
BITS 32

SECTION .text ALIGN=4
GLOBAL FUN_0041ff80

EXTERN DAT_00491fb8
EXTERN DAT_00492590
EXTERN DAT_00491c10
EXTERN FUN_00421e00
EXTERN FUN_0044a5f0
EXTERN FUN_00447700
EXTERN FUN_004218d0

FUN_0041ff80:
    push esi
    db 0x8b, 0xf1                 ; mov esi, ecx
    mov al, [esi + 0x3cc]
    test al, al
    jns .after_state_write
    mov eax, [esi + 0x63c]
    test eax, eax
    jnz .after_state_write
    mov dword [DAT_00491fb8], 3
.after_state_write:
    mov eax, DAT_00492590
    call FUN_00421e00
    db 0x33, 0xc9                 ; xor ecx, ecx
    call FUN_0044a5f0
    mov eax, [DAT_00491c10]
    push eax
    call FUN_00447700
    test eax, eax
    jz .no_global_error
    mov eax, 4
    pop esi
    ret
.no_global_error:
    mov eax, [esi + 0x648]
    test eax, eax
    jz .call_context_update
    db 0x33, 0xc9                 ; xor ecx, ecx
    cmp eax, 2
    setne cl
    pop esi
    dec ecx
    and ecx, 3
    inc ecx
    db 0x8b, 0xc1                 ; mov eax, ecx
    ret
.call_context_update:
    db 0x8b, 0xc6                 ; mov eax, esi
    call FUN_004218d0
    cmp eax, 1
    jnz .return
.return:
    pop esi
    ret
