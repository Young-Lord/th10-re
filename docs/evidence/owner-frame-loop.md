# Owner frame loop entries (0x004485f0-0x00448810)

Module: `src/OwnerFrameLoop.cpp`

Five dispatch wrappers around `DrawLargeRenderOwnerKindChain`
(0x00448980, registered; EAX = kind, EDI = owner):

- `0x004485f0`: kind 0, no preparation.
- `0x00448620`: binds `DAT_00491fac = &flt_491d7c`, applies
  `UpdateMainChainD3DFrameStateEdiAbi` (0x00421480), re-materializes the
  material block at +0xcc through vtable +0xbc (SetMaterial), zeroes the
  draw mode `DAT_00491fb0`, and when `DAT_00492378` is set flushes and
  clears render state 28 before the kind-3 dispatch.
- `0x00448740`: zeroes `DAT_00491e64/491e68` and owner +0x5c/+0x60, kind
  0xe.
- `0x00448770` / `0x004487c0` / `0x00448810`: three identical bodies
  binding `&dword_491e94`, draw mode 1, kind 0xf.
