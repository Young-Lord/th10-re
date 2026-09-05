; Exact ABI layer for the functions grouped in AsciiManagerStrings.obj.
; Addresses and instruction sequence are from TH10 1.00a via Ghidra MCP.
BITS 32

SECTION .text ALIGN=4

GLOBAL FUN_00401530
GLOBAL FUN_00401630
GLOBAL FUN_00401690

EXTERN DAT_00473660
EXTERN _vsprintf
EXTERN FUN_00458ea5

FUN_00401530:
    mov edx, [ecx + 0x896c]
    cmp edx, 0x100
    push esi
    db 0x8b, 0xf0                 ; mov esi, eax
    jge .done
    db 0x8b, 0xc2                 ; mov eax, edx
    imul eax, eax, 0x68
    push edi
    lea eax, [eax + ecx + 0x76c]
    inc edx
    db 0x8b, 0xf8                 ; mov edi, eax
    mov [ecx + 0x896c], edx
    db 0x2b, 0xfe                 ; sub edi, esi
    db 0x8d, 0xa4, 0x24, 0x00, 0x00, 0x00, 0x00
.copy:
    mov dl, [esi]
    mov [edi + esi], dl
    inc esi
    test dl, dl
    jne .copy
    mov esi, [ebx]
    lea edx, [eax + 0x40]
    mov [edx], esi
    mov esi, [ebx + 4]
    mov [edx + 4], esi
    mov esi, [ebx + 8]
    mov [edx + 8], esi
    mov edx, [ecx + 0x8974]
    mov [eax + 0x4c], edx
    mov edx, [ecx + 0x8978]
    mov [eax + 0x50], edx
    mov edx, [ecx + 0x897c]
    mov [eax + 0x54], edx
    mov edx, [ecx + 0x8980]
    mov [eax + 0x5c], edx
    mov dword [eax + 0x60], 0
    mov ecx, [ecx + 0x8988]
    mov [eax + 0x64], ecx
    pop edi
.done:
    pop esi
    ret

FUN_00401630:
    sub esp, 0x204
    mov eax, [DAT_00473660]
    mov ecx, [esp + 0x20c]
    db 0x33, 0xc4                 ; xor eax, esp
    mov [esp + 0x200], eax
    lea eax, [esp + 0x210]
    push eax
    push ecx
    lea edx, [esp + 8]
    push edx
    call _vsprintf
    mov ecx, [esp + 0x214]
    add esp, 0xc
    lea eax, [esp]
    call FUN_00401530
    mov ecx, [esp + 0x200]
    db 0x33, 0xcc                 ; xor ecx, esp
    call FUN_00458ea5
    add esp, 0x204
    ret

FUN_00401690:
    sub esp, 0x204
    mov eax, [DAT_00473660]
    mov ecx, [esp + 0x208]
    db 0x33, 0xc4                 ; xor eax, esp
    mov [esp + 0x200], eax
    lea eax, [esp + 0x20c]
    push eax
    push ecx
    lea edx, [esp + 8]
    push edx
    call _vsprintf
    add esp, 0xc
    lea eax, [esp]
    db 0x8b, 0xce                 ; mov ecx, esi
    call FUN_00401530
    mov eax, [esi + 0x896c]
    mov ecx, [esp + 0x200]
    imul eax, eax, 0x68
    db 0x33, 0xcc                 ; xor ecx, esp
    mov dword [eax + esi + 0x764], 1
    call FUN_00458ea5
    add esp, 0x204
    ret
