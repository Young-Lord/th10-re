# Title Calc Cluster (0x424d90, 0x42a450, 0x417040, 0x404450, 0x417770, 0x418a00, 0x409f90)

All seven are the remaining boundaries of the covered title-screen calc body
`0x418190` (`src/TitleScreenCalcBody.cpp`). Implemented in
`src/TitleCalcCluster.cpp/.hpp`; every fixed global and offset below was read
from the IDA (idalib) disassembly of th10.exe.

## 0x00424d90 ResetOptionPositionRecordsEsiAbi

Native ABI: ESI = the option-position manager (DAT_00477834), plain `ret`,
and a tail `jmp 0x413790` (life-icon refresh).

- Sets `[esi+0x458] = 1`.
- Three option sub-records, each `{dword, dword, dword, rate ptr, first-use
  flag}` based at `+0x460`, `+0x474`, `+0x488` (flag at `+0x470`, `+0x484`,
  `+0x498`). On first use (flag bit 0 clear): `{0, -999999, 0, &flt_476f78
  (1.0f), flag|1}`. Always afterwards: record 0 `{dword0 = -2, dword1 = -1,
  dword2 = 0xBF800000 (-1.0f)}`, records 1/2 `{dword0 = -1, 0, 0}`.
- Releases the entity id at `[esi+0x329c]` through `0x4492a0`
  (EDX = DAT_00491c10, stack = id) and stores 0 into the slot twice (the
  native repeat is preserved).
- Tail: `0x413790` with EAX = DAT_0047770c and ECX = DAT_00474c70, i.e. the
  existing `RefreshLifeIconsEaxStackAbi(g_PlayerLivesRemaining)`.

## 0x0042a450 ApplyOptionPositionStateEbxAbi

Native ABI: EBX = the game-mode object (DAT_00477838, pushed by both 0x418278
and 0x418477 call sites), plain `ret`.

- Marks the scheduler records at `rec+8`, `rec+0x1cc`, `rec+0xc`
  (bit 1 of `record+4`, null-checked per record).
- `mode = [rec+0x10]`:
  - **0 (build)**: slot = `[rec + 0x1c + 4*DAT_00474c7c]`; calls `0x42ab20`
    (ECX = rec, boundary) and stores `0x42aa50(rec)` into `rec+0x9c`
    (boundary). While DAT_00491fc4 == 0 copies the run stats into the slot:
    `+0xc = 0x474c44`, `+0x10 (u16) = 0x474c48`, `+0x14 = 0x474c4c`,
    `+0x18 = 0x474c58`, `+0x1c = 0x474c70`, `+0x20 = 0x474c98`,
    `+0x1b4 = 0x474c90`. Then from the DAT_00477834 manager:
    `slot+0x24/0x28 = mgr+0x3cc/0x3d0`, `memcpy(slot+0x2c, mgr+0x436c,
    0x108)`, the scattered 4-iteration copy (src base `mgr+0x32d4`, src
    stride 0x98; dst base `slot+0x134`, dst stride 8; dword pairs at dst
    offsets 0/4/0x20/0x24/0x40/0x44/0x60/0x64), `slot+0x1b8 = 0x474c9c`,
    `rec+0x1d0 = DAT_00474c7c`, `slot+0x1bc = mgr[0x4474]`.
  - **1 (commit)**: slot = `[rec + 0xb0 + 0x24*DAT_00474c7c]`;
    `rec+0x1d0 = DAT_00474c7c`; `0x428e10` with ECX = `slot+0x24`,
    EAX = manager (boundary); `memcpy(mgr+0x436c, slot+0x2c, 0x108)`;
    `mgr[0x4474] = slot[0x1bc]`; `0x426f70` option rebuild; the inverse
    scattered copy back into `mgr+0x32d4..` — with the extra-stage quirk:
    when `0x474c68 + 0x474c6c + 2*0x474c68 == 5` the last dword pair
    (`mgr+0x32ec/0x32f0`) is overwritten with the first pair
    (`slot+0x134/0x138`); clears `mgr+0x332c/0x33c4/0x345c/0x34f4`; tail
    call `0x424d90` (ESI = manager).
- Finally `[rec+0x1c8] = 0` on every path.

## 0x00417040 UpdateInGameScoreDisplayEsiAbi

Native ABI: ESI = the ASCII HUD owner (DAT_0047770c), plain `ret`.

- Score ramp: while `[esi+0x9e78] != DAT_00474c44`, the rate candidate is the
  **signed** `(target - displayed) / 32`, clamped above at 578910 (0x8d55e)
  and floored at 1 only when exactly zero (negative candidates pass through);
  the stored rate at `+0x9e7c` only grows, is capped at `target - displayed`,
  and the displayed score accumulates; hitting the target clears the rate.
  Crossing 1e8 upward sets the word at `+0x48d8` to 4.
- Best score: when `DAT_00474c40 < displayed`, publish it, copy
  `0x474c90 -> 0x474c94` and set `0x474ca0 |= 4`. The native then tests bit 2
  of the just-updated `0x474ca0`, which is always set, so the `0x448d00`
  spawn block at `0x41711f..0x417159` (kind 0x4a, arg 15, script
  `[esi+0x9ec8]`, target slot `+0x9e18`) is **dead code**; documented, not
  emitted.
- `[esi+0x9e64] >= 20`: publish `0x474c40` to `+0x9e74` (and set the word at
  `+0x2420` to 4 when crossing 1e8).
- Digit redraw: nine iterations over the VM pairs at `esi+0x3bc` (best score)
  and `esi+0x2874` (displayed), stride 0x3ac, digits consumed
  least-significant first, entry index `digit + 8`, resource `[esi+0x9ec8]`;
  `0x43ee30` runs on both VMs each iteration.
- Auxiliary rows: `esi+0x10` with entry `DAT_00474c94 + 8` and `esi+0x24c8`
  with entry `DAT_00474c90 + 8`, then `0x43ee30` on both.
- Rank gate: if `[esi+0x9eb4] & 0x20 == 0`, the table is `0x474488` when
  `DAT_00474c74 == 4` else `0x474474`; when `displayed >= table[0x474c9c]`,
  call `0x4188a0` (EAX = &DAT_00474c40, ECX = 1) and increment `0x474c9c`.

## 0x00404450 InitializeTitleSecondaryStateStackAbi

Native ABI: one stack argument (the 0x2b64-byte secondary title state,
DAT_004776e8), `__stdcall ret 4`; returns `[state+0x1c]`.

- Sets bit 1 of `record+4` for the records at `state+8`, `state+0xc`,
  `state+0x2a40`.
- Walks the s16 count at `[state+0x10]`: for each stage pointer in the table
  at `state+0x14`, sets `entry[3] = 1` and, while the s16 kind at
  `record+0x1c+..` is >= 0, initializes the player VM
  (`0x404f30`: stack arg = `[state+0x178]`, ESI = `[state+0x17c] +
  running_index*0x3ac`, EAX = the s16 script at `record+4`), stores the
  running index (u16) at `record+6`, and advances by the s16 stride at
  `record+2` (`running_index++`, VM offset += 0x3ac).
- Finally `[state+0x4c] = [state+0x1c]`.

## 0x00417770 ReleaseOwnerRecordChainEaxAbi

Native ABI: EAX = owner, plain `ret`.

Drains the doubly linked chain at `owner+0x18`: per node `{vtable, prev,
next}` it calls `vtable+0x10` (index 4, thiscall ECX = node), unlinks
(`prev->next = next`, and `next->prev = prev` when next exists) and frees the
node with `j__free` (0x4524a1, the shared `FreeMainChainObject` boundary).
The loop is do-while on `next`, so the tail node (next == 0) is processed
too.

## 0x00418a00 TickTitleFrameStateEaxAbi

Native ABI: EAX = the shared frame-state block (DAT_00474c40), plain `ret`.

- While `[block+0x18]` (DAT_00474c58) > 0: tail call `0x44bf40` with
  ESI = `block+0x14` and the stack float -1.0f (the existing
  `ShiftTimerByEsiStackAbi`).
- Otherwise the power ladder: while `[block+0xc]` (DAT_00474c4c) > 5000,
  subtract `[block+0x10]` (DAT_00474c50), clamp the decrement to >= 18 after
  use, and clamp the result to >= 5000. (The sibling 0x418a90 reset is
  already modeled in `TitleGameManagerLifecycle.cpp`.)

## 0x00409f90 ReleaseAsciiHudConditionalState

Native ABI: one stack argument (the DAT_00477704 HUD conditional state),
`__stdcall ret 4`; the object stays allocated.

- Releases the four entity resource pools: `0x4493e0` with EAX = 0x491c10
  and EDX = each dword at `0x491c10 + 0x3accb0` (the existing
  `ReleaseEntitiesUsingResourceEaxEdxAbi`).
- Walks the chain at `state+0x58` (`node = {object, next}`) and calls
  `vtable[5]` (offset 0x14) with argument 1 on every non-null object.
- Clears `state+0x64` and `state+0x10`, then clears bit 1 of `record+4` for
  the two records at `state+8` and `state+0xc` (null-checked).

## Shared helper 0x004188a0 AwardExtendedLifeEaxEcxAbi

Not one of the seven, but reconstructed in the same file because 0x417040 is
its only caller. Native ABI: EAX = the 0x474c40 block, ECX = increment.
`[block+0x30] += inc` (this is DAT_00474c70, the signed lives dword): when
the result exceeds 9 it is clamped to 9; otherwise the extend sound
`0x43dc90` (ECX = 0x492590, EDI = 0x2c, stack 0), the `+0x9e18` entity
release/respawn (`0x4492a0` then `0x448d00(script = [hud+0x9ec8], kind 0x4b,
arg 15)`, id stored to `+0x9e18`), both branches finishing with the
`0x413790` icon refresh. The third `0x448d00` stack argument (15, stored to
`entity+0x20`) is not modeled by the shared `SpawnSetupEffectVmListABack`
helper; documented as a known omission.

## Wiring

- `src/TitleScreenCalcBody.cpp`: the seven boundaries are replaced with the
  semantic bodies; `0x42a450` now receives DAT_00477838 as its EBX argument
  (previously the call was modeled with no inputs).
- `src/EclSelectMenu.cpp` and `src/TitleGameManagerLifecycle.cpp`: local
  externs of `0x424d90` / `0x409f90` removed; the call sites use the shared
  header.
- `scripts/compile-main-chain-cpp.sh`: `TitleCalcCluster.cpp` added next to
  the title lifecycle sources.

Baselines: `scripts/compile-main-chain-cpp.sh`, `g++ -m32 -std=c++98
-fsyntax-only -I src src/*.cpp`, and `git diff --check` all pass.

## Game-mode record helpers (0x00428e10 / 0x0042ab20 / 0x0042aa50)

- `0x00428e10` `PublishSelectedRunStatsEaxEcxAbi` (native EAX = the
  0x477834 option-position manager, ECX = slot+0x24): copies the two
  dwords to manager+0x3cc/+0x3d0, publishes their signed*0.01f floats at
  +0x3c0/+0x3c4, and sets the four flags at +0x332c/+0x33c4/+0x345c/
  +0x34f4 to 1.
- `0x0042ab20` `FreeGameModeChainEntriesEaxEcxAbi` (native EAX = bucket
  index from DAT_00474c7c, ECX = the game-mode object): walks the chain
  at object+64+12*index (nodes {record, next}) unlinks each record
  (prev at record+0x627c, next at record+0x6280) and frees it with the
  shared delete.
- `0x0042aa50` `AllocateGameModeChainEntryEsiStackAbi` (native ESI =
  bucket index, stack = the game-mode object): allocates and zeroes the
  0x6284-byte record (self pointers +0x5470/+0x6278, node at +0x6278 with
  next +0x627c / prev +0x6280) and links it at the tail of the bucket
  chain at object + 4*(3*index+15) (first pointer at +4), returning the
  node base.

The native call sites in 0x42a450 pass the slot index from DAT_00474c7c
for both bucket helpers.
