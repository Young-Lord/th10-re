# TH10 0x004247f0 — Player Object Initialization

Semantic module: `src/PlayerObjectLifecycle.cpp/.hpp`
(`InitializePlayerObject`). Native entry receives EBX=player, returns 0 on
success and -1 on failure. The interrupted pre-session analysis was redone
from the binary (`references/` style raw dumps plus Ghidra).

## Caller chain established

- The draw callback registered here is the thunk pair
  `0x00426510` (`MOV EAX,ECX; JMP 0x00426360`), which is how
  `RenderAsciiSceneObjectAndBarOverlay` (see
  `docs/evidence/ascii-scene-object-bar.md`) is dispatched; the only xref
  noted there (`0x00426512`) is this JMP.
- The update callback is the thunk `0x00426500`
  (`PUSH ECX; CALL 0x00425730; RET`), a thiscall-to-stack-arg adapter for
  `0x00425730`, the mode-0..4 update dispatcher (`CMP [obj+0x458],4`, jump
  table `0x00426344`).
- `0x004247f0` itself receives EBX from a thiscall-style caller.

## Body (write order preserved)

1. Loads `pl00.anm`/`pl01.anm` by `DAT_00474c68` and requests manager-work
   slot 8 through `0x00447280` (native: EDX=owner `DAT_00491c10`, ECX=slot,
   stack=name, `ret 4`); stores the work at `player+0x10`. Failure appends
   the "自機データが見つかりません" text via `0x0044b810` and returns -1.
2. When `DAT_00491bf0` is null, `0x00426520` loads the shot sub-object
   (native ESI=player, EAX=`0x476fc0[shot_type + character*3]`); a nonzero
   cached pointer is taken over and cleared instead. Both failure paths log
   the same text and return -1.
3. Registers the update element (calculation chain, priority `0x10`) at
   `player+8` and the draw element (draw chain, priority `0x16`) at
   `player+0xc`; both elements clear the enabled bit and store the player as
   their argument (`+0x20`).
4. `0x00404f30` initializes the main VM at `player+0x14` against the ANM
   work (native ESI=vm, stack=work, EAX=0).
5. Position `+0x3c0/+0x3c4/+0x3cc` = (0, 400.0, 0), `+0x3d0 = 40000`;
   `+0x3d4..0x3e0` are the four shot-table entries times `DAT_00470b44`
   (100.0) truncated by the CRT float-to-int helper `0x00463b2c`.
6. Copies the pair (`+0x3cc`, `+0x3d0`) into 0x21 dword pairs at `+0x436c`.
7. Three init-once records (`+0x460`, `+0x474`, `+0x430c` blocks) seeded
   with the `0xfff0bdc1` negative-NaN transient and descriptor `0x476f78`
   (a 1.0f constant), then overwritten: record 1 gets `-2/-1/-1.0f`;
   record 3 ends with `0x77/0x78/120.0f/0x1e`.
8. Shot-table writes to `sub+4/8/0xc` from `0x476fb8/0x476fa0/0x476fb0`
   indexed by character, then three half extents: hitbox `sub+4 * 0.5`,
   graze `0x476fb0[char] * 0.5`, item box `0x476fa8[char] * 0.5`
   (`DAT_00470b0c` = 0.5). Boxes land at `+0x404`, `+0x4324`, `+0x433c`,
   and an identical copy at `+0x4354`, each as
   `pos ± {half, half, 5.0}`.
9. `0x00426f70` rebuilds the option records at `+0x32a0` (stack argument)
   and the function returns 0.

## Interrupted-session repair

Two files left broken by the previous session were fixed to restore the
verification baseline:

- `LargeRenderOwnerFrameLoop.cpp`: `OwnerNode` member order/padding made the
  offset asserts unsatisfiable; rearranged to match the asserts
  (`chain_next` 0x1c, `kind` 0x20, `flags` 0x35c, hooks 0x398/0x39c) and the
  `LargeRenderOwnerLayout` padding to put `kind_buckets` at 0x72dae4,
  matching the `0x72dad4/0x72dadc` list heads the native code iterates.
- `TimelineRenderObjectSetup.cpp`: removed `<stdint.h>` (absent in the
  VC++ Toolkit 2003 toolchain; no fixed-width types are used) and replaced
  `std::isnan(x)` with the `x != x` idiom used elsewhere in the project.
