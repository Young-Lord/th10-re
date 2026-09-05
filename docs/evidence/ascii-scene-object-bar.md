# TH11 0x00426360 — Scene Object VM Dispatch And Bar Overlay

Semantic module: `src/AsciiSceneObjectRenderer.cpp/.hpp`
(`RenderAsciiSceneObjectAndBarOverlay`). Native entry receives EAX=object,
returns `1` unconditionally, and has no stack arguments.

## Body

1. `object+0x458 == 2` skips the whole body (hidden/finished state).
2. `object+0x354 = object+0x3c0 + DAT_00470b4c` and
   `object+0x358 = object+0x3c4 + DAT_00470b48` via x87 float adds; the
   scroll constants are named `g_AsciiObjectScrollX/Y`.
3. `object+0x35c = object+0x3c8` is an integer dword copy (no FLD/FSTP).
4. `0x004451c0 DispatchAsciiAnimationVmRenderMode` runs with EAX =
   `object+0x14` and ECX = global render owner `DAT_00491c10`.
5. Four records: the callback slots live at `object+0x3334 + i*0x98`, but the
   native `LEA ECX,[ESI-0x94]` shows each record base is
   `object+0x32a0 + i*0x98` with the function pointer at `record+0x94`.
   Non-null pointers are `CALL`ed with ECX=record; the semantic boundary uses
   the project fastcall convention.
6. Gate (each failure jumps to the shared return; evaluation order preserved):
   - `DAT_00477810` (title screen) non-null and byte `+0x54` signed-negative
     (JS tests bit 7);
   - `DAT_00477704` (`g_AsciiHudConditionalState`) non-null;
   - `DAT_0047770c` (`g_AsciiHudOwner`) non-null;
   - `i32 [DAT_00477704+0x10] == 0` (single dereference);
   - `i32 [DAT_0047770c+0x9eb8] == 0`;
   - `DAT_00477830` (`g_GameStateManager`) is dereferenced without a null
     check and `i32 [+4] == 0` is required;
   - `DAT_00474c58` (`g_AsciiHudBarValue`) nonzero.
7. Bar rectangles (all x87 single precision; `0x00427c50` is a `ret 4`
   stdcall wrapper that widens its float to a double and tail-calls the CRT
   `floor` at `0x00452ff0`):
   - `left   = floor(obj+0x3c0 + DAT_00470cbc - DAT_00470b48)`
   - `top    = floor(obj+0x3c4 + DAT_00470cb4 - DAT_00470be8)`
   - `right  = left + (float)(i32)bar_value * DAT_00470cb8` (FILD/FMUL/FADD;
     the bar value is the signed int saved before the first floor call)
   - `bottom = top + DAT_00470b08`
   - Rect 1 `{left, top, right, bottom}` with EBX=`0x80000000`, then rect 2
     `{left-1, top-1, right-1, bottom-1}` with EBX=`OR`-ed `0xffffffff`,
     both through `0x0043bda0 DrawImmediateAsciiColoredRectangle`.
   - This mirrors the `RenderAsciiHudBatch` bar idiom (outer dim rect, inner
     bright rect inset by one pixel).

## Stack frame evidence

`SUB ESP,0x14` plus `PUSH EDI/EBX/ESI`; the rect lives at `[ESP+0x10]` with
the floor scratch slot and alignment `PUSH ECX` below it. The `ret 4` of
`0x427c50` (not cdecl) is what makes `FILD [ESP+0x10]` reload the saved bar
integer, which pins the layout.

## Naming

- `0x426360` `RenderAsciiSceneObjectAndBarOverlayEaxAbi`
- `0x427c50` `AsciiFloorWrapperStdcall` (forwards to CRT `0x452ff0` floor)

The only xref is an unconditional call at `0x426512` in code Ghidra has not
outlined into a function; caller-side context is pending.

## Naming update (upstream confirmation)

- `DAT_0047770c` is now named `g_AsciiHudOwner`: it is a second instance of
  the large render/scene owner type (owns the `+0x10/+0x24c8/+0x4980/
  +0x6a8c/+0x793c/+0x8094` 0x3ac-stride VM arrays and the `+0x9e..` field
  family). Its destructor `0x004145f0` clears the global after draining the
  chain elements and VM arrays; `0x00413810` publishes it.
- `DAT_00477830` is now named `g_GameStateManager`: `0x004220e0` resets a
  ~0x288-dword state object and publishes it; `0x00422220` clears it. The
  title screen (`0x00417c80`), GUI paths (`0x00418190`, `0x00415e90`), and
  this gate read it.
