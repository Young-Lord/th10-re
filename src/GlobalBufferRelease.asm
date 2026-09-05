; FUNCTION: TH10 0x0041f930
; Releases and clears two pointers stored at global-base + 0x3ad084/+0x3ad088.
BITS 32

SECTION .text ALIGN=4
GLOBAL FUN_0041f930

EXTERN DAT_00491c10
EXTERN FUN_00447810
EXTERN _free

FUN_0041f930:
    push esi
    mov esi, [DAT_00491c10]
    add esi, 0x3ad084
    push edi
    mov edi, [esi]
    test edi, edi
    jz .second_buffer
    call FUN_00447810
    mov eax, [esi]
    push eax
    call _free
    add esp, 4
    mov dword [esi], 0
.second_buffer:
    mov esi, [DAT_00491c10]
    mov edi, [esi + 0x3ad088]
    add esi, 0x3ad088
    test edi, edi
    jz .done
    call FUN_00447810
    mov ecx, [esi]
    push ecx
    call _free
    add esp, 4
    mov dword [esi], 0
.done:
    pop edi
    db 0x33, 0xc0                 ; xor eax, eax
    pop esi
    ret
