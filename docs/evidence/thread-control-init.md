# ThreadControl initialization

Covers TH10 0x0044c0e0 — added to module
`src/ThreadControl.{hpp,cpp}` alongside StopThreadControl (0x0044c150).

## 0x0044c0e0 InitializeThreadControlInPlaceEaxAbi

Native EAX = control object. Writes the ThreadControl vtable
(off_4703e4) to +0x00 and zeroes the four following dwords (+0x04 thread
handle, +0x08 thread id, +0x0c stop flag, +0x10 field). The vtable's
plain/deleting destructors (0x44c130 / 0x44c100, referenced from the
vtable at 0x4703e4) route through StopThreadControl.

The vtable itself is referenced from 17 sites across the lifecycle
managers (InitStageHostObjectEdxAbi, ConstructGameManagerInPlace,
InitializeScriptTestManagerEdxAbi, DestroyGlobalLifecycleManagerInPlace,
...) — those covered constructors either inline the same five stores or
call this entry.

## Verification

Reference disassembly `build/reference/0044c0e0_sub_44C0E0.asm`; vtable
xrefs enumerated via xref_query on 0x4703e4.
