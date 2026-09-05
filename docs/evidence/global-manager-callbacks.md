# Global Manager Callback Evidence

`0x0041fef0` is a 7-byte wrapper: it pushes its incoming `ECX` value, calls
`0x0041fdd0`, and returns with the callee's result. `0x0041fdd0` uses that
value as a pointer, advances fields at offsets `0x3e4`, `0x3e8`, and `0x3ec`,
and returns one.

The wrapper is installed by `0x0041fac0` through the draw-chain insertion
path. `0x0041feb0` is installed through the calc-chain path. It checks bit one
at `[ECX]`; when set, it marks three chain elements at offsets `+0xc`, `+0x10`,
and `+0x89a8` under `0x004776e0`, updates globals `0x00491ff4` and
`0x00491fb8`, clears the incoming bit, and returns one. Names here are
behavioral only and do not assert that the containing object is a particular
known game class.

Both callback objects were exported with `ExportTh10Delinker.java` and
compared on 2026-08-28. `GlobalManagerFrameCallback.obj` has a seven-byte
`.text` section and `GlobalManagerDisableChainsCallback.obj` has a 63-byte
`.text` section. Each compares byte-for-byte equal to its reconstruction and
objdiff reports `100.0%`.
