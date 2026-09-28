# Main-chain error log appenders

Covers TH10 0x0044b810, 0x0044b8e0 — module
`src/MainChainErrorLog.{hpp,cpp}`. Both entries are printf-style
appenders over the static error-log object at DAT_00474f70:

    +0x0000 char text[0x2000];      // g_MainChainErrorBuffer
    +0x2000 char *cursor;           // g_MainChainErrorCursor (DAT_00476f70)
    +0x2004 u8 show_dialog_on_exit; // g_MainChainErrorShowDialog

The flush side (log.txt + optional dialog) lives in
MainApplicationFinalCleanup.cpp (`FlushMainChainErrorLog`).

## Common append semantics

EnterCriticalSection(stru_4922bc), `++byte_49231f`, vsprintf into the
stack buffer, `strlen`, then when `cursor + len < object + 0x1fff`
(checked as unsigned) strcpy the text at the cursor, advance the stored
cursor by `len` and NUL-terminate. LeaveCriticalSection and
`--byte_49231f` follow. Both entries return the *format* pointer in EAX
(a quirk: the return value is not the cursor).

## 0x0044b810 AppendMainChainErrorLogFormatEcxEfxAbi

Object in ECX; 8 KiB (0x2004 incl. cookie) stack buffer. Used for the
bulk of the game's progress/error messages (59 direct call sites,
LoadMainChainConfiguration and CreateMainChainD3D9Device alone account
for ~30).

## 0x0044b8e0 AppendMainChainErrorLogFormatEdiEfxAbi

Object in EDI; 0x200-byte stack buffer. Additionally raises the dialog
flag at object+0x2004 to 1 while still holding the lock, even when the
append itself was skipped by the size check. This is the failure-path
logger (16 call sites: LoadRenderOwnerCachedSurfaceFromFile,
CreateTitleScreenStateEaxEcxStackAbi, BuildManagerWork,
ProcessSelectedManagerWorkStage, ...).

## Verification

Reference disassembly `build/reference/0044b810_sub_44B810.asm`,
`0044b8e0_sub_44B8E0.asm`; Hex-Rays shapes both as `__usercall` with
`va_start` over the stack arguments and `_vsprintf` (0x4524a6).
