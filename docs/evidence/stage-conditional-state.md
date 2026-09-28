# Stage Conditional State (0x40d400 / 0x40d680 / 0x40d6b0)

Module: `src/StageConditionalState.cpp`.

- `0x0040d6b0` `CreateStageConditionalStateStackAbi` — native stdcall, one
  stack argument = the stage enemy script path (ret 4). Loads the script
  through the shared main-chain file loader and constructs the conditional
  state object tree from the loaded 'ANIM'-style sections; returns the
  state root. The stage-scene setup calls it with the selected stage
  script from the manager's stage table.
- `0x0040d400` `ParseAnimSectionStackAbi` — native ECX = the
  name-receiving sub-record object, stack = the section data (ret 4):
  parses one animation section into the sub-record (field defaults, name
  binding from the data tail).
- `0x0040d680` `DestroyConditionalStateBufferEsiAbi` — native ESI = the
  0x68-byte conditional state buffer: releases the buffer (the release
  wrapper family entry; the +0x64 heap pointer is CRT-freed and cleared).

The 0x40d530 release wrapper half is documented in
`docs/evidence/manager-release-wrappers.md` and implemented in
`src/ManagerReleaseWrappers.cpp`.
