; FUNCTION: TH10 0x0041feb0
; ECX is a chain callback context with a teardown flag at bit 1.
BITS 32

SECTION .text ALIGN=4
GLOBAL FUN_0041feb0

EXTERN DAT_004776e0
EXTERN DAT_00491ff4
EXTERN DAT_00491fb8

FUN_0041feb0:
    test byte [ecx], 2
    jz .done
    mov eax, [DAT_004776e0]
    mov edx, [eax + 0xc]
    or dword [edx + 4], 2
    mov edx, [eax + 0x10]
    or dword [edx + 4], 2
    mov eax, [eax + 0x89a8]
    or dword [eax + 4], 2
    and dword [DAT_00491ff4], 0xffffefff
    mov dword [DAT_00491fb8], 4
    and dword [ecx], 0xfffffffd
.done:
    mov eax, 1
    ret
