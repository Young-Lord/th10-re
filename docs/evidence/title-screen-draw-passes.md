# Title-Screen State Draw Passes (0x00402850 / 0x00402ca0) and 0x00405300

Implemented in `src/TitleScreenDrawPasses.cpp/.hpp`. These are the two draw
scheduler records of the 0x2a78-byte title-screen state object (the same
object destroyed by 0x00402440 `DestroyTitleScreenStateBufferInPlace`). All
evidence below is from raw `objdump` disassembly of `resources/th10.exe`
(IDA MCP unavailable in this session).

## Registration context (0x00402230)

The state constructor 0x00402230 (native EAX = state, stack = is-secondary
flag; publishes DAT_004776e4 / DAT_004776e8) creates three scheduler records
through 0x004449ed0/0x00449ae0/0x00449b70, each storing the state at
`record+0x20`:

| record slot | adapter  | target    | chain |
|---|---|---|---|
| +0x08       | 0x403050 | 0x00402720 (`mov eax,ecx; jmp`) | calculation |
| +0x0c       | 0x403060 | 0x00402850 (`push ecx; call`)   | draw        |
| +0x2a40     | 0x403070 | 0x00402ca0 (`push ecx; call`)   | draw        |

Both draw bodies are stdcall `ret 4` with the state as the only stack
argument and return 1 on every path. Both start with the shared guard:
`if (state+0x2a18 & 8) return 1;` and the front block is skipped when
`(state+0x2a18 & 4) && (i32)state+0x2a20 >= 0x3c` (signed compare).

## Shared device-interface model

`DAT_00491c30` is the D3D9 device wrapper already modeled in
`MainChainD3DDevice.cpp` / `AsciiOwnerTraversal.cpp`. Slots used here:
vtable `0xbc/4 = 47` SetViewport, `0xac/4 = 43` Clear, `0xe4/4 = 57`
SetRenderState. Render state ids used: `0xe` (z-write enable), `0x17`
(z compare func; 4 vs 8), `0x1c` (fog enable — the DAT_00492378 cache pairs
with it exactly as in `AsciiOwnerTraversal.cpp SetFogEnabled`), `0x22/0x24/0x25`
(fog color / fog end / fog density per the D3D9 numbering; values come from
state `+0x2b60` / `+0x2b48` / `+0x2b4c`).

Every SetRenderState is preceded by a render-owner vertex flush
(0x00442f50); the one exception class is the Clear calls, which have no
preceding flush (including the z-only clear).

## 0x00402850 `RunTitleScreenDrawPass0StackAbi`

1. Guard (above). Front block unless skipped:
   - flush; copy the float render offsets `DAT_00491e64/68` (semantic
     `g_AsciiOverlayRenderOffsetX/Y`, raw dword copies) into `+0x2b34/+0x2b38`;
   - copy the state camera snapshot `+0x2a4c` (0x46 dwords = one
     `MainChainCameraWork`, 0x118 bytes) over `DAT_00491d7c`, set
     `DAT_00491fac = DAT_00491d7c`, run 0x004215a0
     (`UpdateMainChainCameraWorkEdiAbi`), SetViewport(camera + 0xcc);
   - `DAT_00491fb0 = 0` (`g_AsciiActiveViewIsDefault`); flush;
     SetRenderState(0xe, 1); flush; SetRenderState(0x17, 4); then flush +
     SetRenderState(0x22/0x24/0x25) from `+0x2b60/+0x2b48/+0x2b4c`;
   - Clear(count 0, no rects, flags 2 = z-buffer only, color 0, z 1.0,
     stencil 0) — no preceding flush;
   - menu region clear: local rect `{32, 16, 416, 464}`; when
     `(flags & 4) && (i32)+0x2a34 < 0x22` the region clears to black
     (color 0), otherwise to `+0x2b60 & 0xffffff`.
2. Fade-in arm (always reached): with `(flags & 4)`, while
   `(i32)+0x2a20 < 0x1e` a fresh kind-3 overlay context is created per frame
   via 0x0043c8b0 (`CreateAsciiOverlayContext(3, 0x1e, 0, 0, 0, 0xf)` —
   native passes the draw priority 0xf in EBX) and the result is discarded;
   then flags bit 0 is latched and the timer at `+0x2a1c` is ticked
   (0x00405410, duration 1). Once the timer reaches 0x1e the flags bit 0 is
   cleared and byte `+0x1eeb` is zeroed.
3. When byte `+0x1eeb != 0`, the owner modulation words are published:
   `owner+0x73245c = 1`, `owner+0x732458 = *(u32*)(state+0x1ee8)`.
4. Scene counters `+0x2a0c/+0x2a10/+0x2a14` are cleared unconditionally.
5. With flags bit 0 set: the background-VM block runs only when the dword at
   `state+0x514` is nonzero — that is `VM[1]+0x394` (the second 0x3ac record
   of the +0x180 array; +0x394 is a resource pointer), not a dedicated flag.
   Native quirk, preserved. The block:
   `SelectMainChainDrawWork(DAT_00491c28, 0)` (0x00405300), then
   0x00420cd0 `DisableMainChainFogIfNeeded`, two flushes,
   SetRenderState(0xe, 0), dispatch of the eight 0x3ac records at `+0x180`
   (each only when its `+0x394` pointer is nonzero) through 0x004451c0
   (`DispatchAsciiAnimationVmRenderMode`, native EAX = vm, ECX = owner),
   flush, SetRenderState(0xe, 1), camera republish (DAT_00491fac = DAT_00491d7c,
   0x004215a0, SetViewport), `DAT_00491fb0 = 0`.
6. Fog cache forced to 1 (flush + SetRenderState(0x1c, 1) when it differed).
7. Scene channels 0..7 through 0x00403a30 (`RenderAsciiSceneChannel`),
   then flush.
8. `+0x1ee4` increments only while already nonzero (latch quirk).
9. Modulation restore: `owner+0x73245c = 0`, `owner+0x732458 = 0x80808080`;
   if `+0x1eec != 0` instead `1 / 0xff404040` is published.
10. flush, SetRenderState(0xe, 0), flush, SetRenderState(0x17, 8); return 1.

## 0x00402ca0 `RunTitleScreenDrawPass1StackAbi`

1. Guard (above). Front block unless skipped:
   - flush; `+0x2b34/+0x2b38` offset copy; camera snapshot restore and
     viewport (as in pass 0);
   - fog cache forced to 1 (flush + SetRenderState(0x1c,1)); `DAT_00491fb0 = 0`;
   - flush, SetRenderState(0xe, 0), flush, SetRenderState(0x17, 8);
   - kind chain 0x11 (0x00448980 `DrawLargeRenderOwnerKindChain`, native
     EAX = kind, EDI = owner `DAT_00491c10`); flush;
     SetRenderState(0x17, 4); fog cache forced to 0; kind chain 0x12; flush;
     SetRenderState(0x17, 4);
   - flush + SetRenderState(0x22/0x24/0x25) from `+0x2b60/+0x2b48/+0x2b4c`.
2. Fade latch: with `(flags & 4) && (i32)+0x2a20 >= 0x1e`, byte `+0x1eeb = 0`.
3. With flags bit 0 set: flush, SetRenderState(0xe, 0), fog cache forced to 1,
   scene channels 8..11 (0x00403a30), flush.
4. Modulation restore as in pass 0 (`0 / 0x80808080`, or `1 / 0xff404040`
   when `+0x1eec != 0`), then flush, SetRenderState(0xe, 0), flush,
   SetRenderState(0x17, 8).
5. Fade drain: while `(i32)+0x2a20 > 0`, shift the `+0x2a1c` timer by
   -1.0 frames (0x0044bf40 `ShiftTimerByEsiStackAbi`); when the count is
   `<= 0` afterwards (signed): byte `+0x1eeb = 0xff`, and when flags bit 1
   was set bit 3 (`0x8`, the draw-disabled bit tested by both passes) is
   latched; finally flags bits 1 and 2 are cleared (`& ~6`),
   `+0x1ee8 = 0xffffff` (white modulation for the next mode's pass 0).
6. flush, SetRenderState(0xe, 0), flush, SetRenderState(0x17, 8), fog cache
   forced to 0; return 1.

## 0x00405300 `SelectMainChainDrawWork`

Small helper (native ESI = context `DAT_00491c28`, EBX = slot index):

```
work = context + 0x154 + index*0x118        // slot 0/1 camera work
context->draw_work_pointer (+0x384) = work
UpdateMainChainD3DFrameStateEdiAbi(work)    // 0x00421480
SetViewport(context->draw_target (+8), work + 0xcc)
context->draw_initialized (+0x388) = index  // stores the slot index
```

The context layout matches the asserted offsets in `MainChainContext.hpp`
(unknown_0154/draw_work_026c slots, draw_work_pointer, draw_initialized,
draw_target at +8). Only index 0 is observed from the title passes; the
index is fully general in the helper.

## Globals and ABI notes

- `DAT_00492378` is used here strictly as the fog-enable cache paired with
  render state 0x1c (same idiom as `AsciiOwnerTraversal.cpp`). The
  `g_MainChainPresentColor` name used in `MainApplicationFrame.cpp` refers to
  the same address; the two readings coexist in the codebase.
- `DAT_00491e64/68` are copied as raw dwords into the state (`+0x2b34/+0x2b38`)
  before the camera restore; the draw-finalize callback zeroes them.
- The front-block skip threshold (0x3c) differs from the overlay/fade
  thresholds (0x1e intro, 0x22 counter, 0x1e latch); all compares are signed.
- State offsets used: `+0x180` (8 VM records), `+0x514`, `+0x1ee4`,
  `+0x1ee8`, `+0x1eeb`, `+0x1eec`, `+0x2a0c..+0x2a14`, `+0x2a18`,
  `+0x2a1c` (timer), `+0x2a20` (timer count), `+0x2a34` (intro counter),
  `+0x2a4c` (camera snapshot), `+0x2b34/+0x2b38`, `+0x2b48/+0x2b4c/+0x2b60`.
