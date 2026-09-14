# TH10 Sprite-View Debug Overlay Text

Module: `src/SpriteViewDebugText.cpp/.hpp`.

## Scope

`0x0040a940` (`DrawSpriteViewOverlayTextEdiAbi`) is the draw callback of
the hidden sprite-viewer debug mode. It is registered by `0x0040a130`
(priority 39 through `0x449ed0`/`0x449b70`, alongside the priority-5 calc
callback `0x0040abe0` -> `0x0040a450`) behind the `0x0040abf0` thiscall
thunk (`mov edi, ecx`). The state object is the sprite-view record whose
pointer is published to `DAT_004776f8` by the `0x0040a040` constructor
(the same record embeds the `0x3ac`-byte VM at `+0x2c8`, which is why
`+0x674/+0x678` sit right past it). It is general UI text infrastructure
in the sense of `registration-draw-render-adapter.md`: it only queues
primary entries through `0x00401690` and never renders directly.

## Behavior

Native EDI = state; always queues `"SprtView\n"` at `(0, 0)` first, then
switches on the view selector at `+0x30` (jump table at `0x40abc8`,
`dec`-keyed so the raw modes are 1..4):

- Mode 1 (file browser): `"File %s"` with `file_table[+0x114]` (table
  pointer at `+0x34`) or `"File not found."` when `+0x38` is zero, both at
  `(42, 16)`; `"Ecl %s"` with the name from
  `DAT_00477704+0x54 -> +0x8c -> [+0x1ec]` (or `"Ecl %d"` with the raw
  index when `DAT_00477704` is null) at `(42, 26)`; `"Quit"` at
  `(42, 36)`; the `" >"` cursor marker at `(48, (i32)+0x3c * 10 + 16)`.
  Falls into the shared tail.
- Mode 2 (loading): sets the manager text color to `0xffff4040`, queues
  `"Loading %s"` with the same file-table entry at `(48, 16)`, resets the
  color to `0xffffffff`, and returns without the tail.
- Mode 3 (position): sets the color to `0xffa0a0a0`, queues
  `"Pos %.3d %.3d"` from the integers at `+0x678/+0x674` at `(48, 16)`,
  falls into the shared tail.
- Mode 4 (enemy count): queues `"Enemy %d"` from `DAT_00477704+0x60` (no
  null check — preserved) at `(500, 240)`, and returns without a color
  reset or tail.
- Default: header only.

Shared tail (`0x40ab61`): reset the manager color to `0xffffffff`, then
when `state+0x684` has bit 1 set, store `{320, 240, 0}` into the anchor
vec3 at `+0x5fc` and draw the embedded VM at `+0x2c8` through
`0x00443080` (`DrawAsciiAnimationVmUnscaledToOwner`, owner
`DAT_00491c10`).

Every branch returns 1.

## Preservation notes

- The color writes go to `manager+0x8974` directly (the constructor
  default `0xffffffff` is a reset, not a save/restore) and the queued
  entry snapshots the color at insertion time, exactly as documented for
  the FPS call site.
- `0x00401690`'s "selected" store lands on `primary_entries[count - 1]`
  even when the queue was full; that quirk lives inside the existing
  `AsciiManager::AddFormatTextSelected` semantic body and is inherited by
  this module.
- Mode 4's unconditional `DAT_00477704` dereference is a native unchecked
  pointer and is kept.

## Evidence basis

- `resources/th10.exe`, `0x0040a040-0x0040abc6` (constructor, callback
  registration, calc/draw thunks, overlay body, jump table at
  `0x40abc8`), strings `0x46cdf4-0x46ce55`, and the x87/vararg call sites
  listed above.
- Consumption of the queued entries follows
  `docs/evidence/registration-draw-render-adapter.md`.
