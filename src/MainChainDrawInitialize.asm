; FUNCTION: TH10 0x00420000
; ECX = g_MainChainContext. Referenced pointee types are not yet recovered.
BITS 32

SECTION .text ALIGN=4
GLOBAL FUN_00420000

EXTERN DAT_00491c10
EXTERN DAT_004923a8
EXTERN DAT_00491c30
EXTERN FUN_004215a0

FUN_00420000:
    mov eax, [DAT_00491c10]
    push ebx
    push esi
    db 0x8b, 0xf1                 ; mov esi, ecx
    mov cl, 0xff
    db 0x33, 0xdb                 ; xor ebx, ebx
    push edi
    lea edi, [esi + 0x26c]
    mov [eax + 0x3ada70], ebx
    mov [eax + 0x3ada64], ebx
    mov [eax + 0x3ada69], cl
    mov byte [eax + 0x3ada68], 3
    mov [eax + 0x3ada6b], cl
    mov [eax + 0x3ada6c], cl
    mov [eax + 0x73245c], ebx
    mov dword [eax + 0x732458], 0x80808080
    mov [eax + 0x3ada6e], cl
    mov [eax + 0x60], ebx
    mov [eax + 0x5c], ebx
    mov [eax + 0x3ada6a], cl
    mov [esi + 0x384], edi
    call FUN_004215a0
    mov edx, [esi + 0x384]
    mov eax, [esi + 8]
    mov ecx, [eax]
    add edx, 0xcc
    push edx
    push eax
    call dword [ecx + 0xbc]
    push ebx
    push dword 0x3f800000
    mov dword [esi + 0x388], 1
    mov edx, [DAT_004923a8]
    mov eax, [DAT_00491c30]
    mov ecx, [eax]
    push edx
    push 1
    push ebx
    push ebx
    push eax
    call dword [ecx + 0xac]
    pop edi
    pop esi
    mov eax, 1
    pop ebx
    ret
