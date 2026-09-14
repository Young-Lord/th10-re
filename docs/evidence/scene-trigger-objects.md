# TH10 Scene Trigger Objects: 0x00406d90 / 0x00426cf0 / 0x004267f0 / 0x00426660 / 0x00408750

Module: `src/SceneTriggerObject.cpp/.hpp`.

The records driven by the covered callers `0x00406240` (`sub_406240`, the
per-frame trigger update) and `0x004067d0` carry an 18-entry instruction
queue (stride `0x18`, opcode at `+0x10`, "has-args" dword at `+0x14`), a
world position float3 at `+0x3b4`, a screen anchor float3 at `+0x3c0`,
extents at `+0x41c/+0x420`, an accumulated flag dword at `+0x43c`, a state
word at `+0x458` and the instruction cursor at `+0x45c` (compared against
`0x12`). All field writes are unchecked pointer arithmetic, preserved
verbatim.

## 0x00426660 AngleToScreenTargetEaxEcxAbi

EAX = position pair, ECX = target block. Returns
`atan2(target+0x3c4 - y, target+0x3c0 - x)`; when both deltas equal exactly
`30.0` the native answers `1.75` radians without calling atan2.

## 0x00408750 SetPolarVelocityThisAbi

Thiscall: `out = {cos(angle) * speed, sin(angle) * speed}`.

## 0x00406d90 RunSceneTriggerInstructionQueueEcxAbi

Interpreter over up to 18 queued opcodes. Entry conditions preserved:

- opcode 0 ends the whole pass (`0x406dcb`), even before the argument check;
- an entry with `+0x14 == 0` only runs while `+0x43c == 0`;
- opcode `0x80000000` is a pure skip.

Opcode table (all verified against the jump table at `0x407354` plus the
direct compare chain):

- `1`: flags |= 1; timer `+0x614` ticked with duration 0; `+0x638 = 0`.
- `16`: flags |= 0x10; speed `+0x65c = a0`; angle `+0x660` through the
  ±999 fcomp chain (below); timer `+0x648` ticked; `+0x670 = a2`;
  polar velocity at `+0x664` from `(+0x660, +0x65c)`. When the cursor is
  not the first slot and `+0x458 >= 0`, channel
  `ReserveContextChannel(0x492590, state, 0)` fires (the native passes the
  object's own state word as the channel kind).
- `32`: flags |= 0x20; `+0x690/+0x694 = a0/a1`; timer `+0x67c` ticked;
  `+0x6a4 = a2`; same conditional sound channel.
- `64` / `128`: flags |= opcode; `+0x6c8` (x) and `+0x6c4` (y) through the
  fcomp chains; timer `+0x6b0` lazily initialized then reset to
  `{-1, 0, 0}`; `+0x6d8/+0x6dc = a2/a3`; `+0x6e0 = 0`.
- `0x400` / `0x800` / `0x8000000`: one shared handler (`0x4072ff`); the raw
  opcode is OR'd into `+0x43c` *and* stored at `+0x6f8`; `+0x710 = a2`;
  `+0x70c = 0`.
- `0x1000`: writes `a2` through the link dword at `+0x4` verbatim.
- `0x2000`: `+0x434 = a2`; cursor +1 (ends the pass at the capacity check).
- `0x4000`: `InitializePlayerMainVm(obj+8, [0x4776f0]->+0x3e0b50, script)`
  with `script = table_474170[a2] + a3` (the EAX register arg).
- `0x8000`: flags |= opcode; timer `+0x718` ticked with `a2`.
- `0x10000`: `ActivateStageEnemy(obj)` (0x00408030, ESI = object).
- `0x20000`: positional sound `0x43dd10` with EBX = `a2`, ESI = `0x492590`
  and the object's world x as the float payload.
- `0x100000` / `0x200000`: flags |= opcode; timers `+0x74c` / `+0x780`
  ticked with `a2`.
- `0x400000`: builds the 0x21c-byte spawn packet (header words, the object
  position, the next/current record heads, then a verbatim 0x1b0-byte copy
  of the whole queue — the native `rep movsd`s 0x6c dwords from `+0x464` —
  and the trailing word/dword fields including the `-1` dword and
  `a2 & 0xff`), calls the `0x4073e0` boundary (EAX = packet, stack =
  `0x4776f0`), then advances the cursor **twice**, and a third time through
  the default tail when `a2` has the high bit set (which also calls
  `ActivateStageEnemy`). The double/triple increment is preserved.
- `0x1000000`: `+0x420 = a2`; cursor +1.
- `0x2000000`: cursor = `a2` verbatim (a jump; a value past the capacity
  ends the pass).

Angle fcomp chain (shared by opcodes 16/64/128, constants
`flt_470b50 = -999.0`, `flt_470ccc = 999.0`): values `<= -999.0` (not NaN)
take the stored `+0x3e4` fallback; NaN and values in `(-999, 999)` keep the
argument; values `>= 999.0` resolve to the live
`AngleToScreenTargetEaxEcxAbi(obj+0x3b4, 0x477834)`.

The y chain of opcodes 64/128 (`flt_470cc8 = -999.0`): anything not
strictly above `-999.0` (NaN included) takes the `+0x3d8` fallback.

## 0x00426cf0 FireSceneTriggerExpireEffect

Stdcall, ret 4. Sets `+0x458 = 4`, spawns one pool VM with script `0x162`
(flash) and 32 VMs with script `0x163` (debris) — each with `vm+0x20 = 0`,
`vm+0x35c |= 0x40000000`, `0x449870(vm, script)`, `0x4489d0` with the VM as
the entity and the id written to a stack slot, and `0x4492f0` publishing
the position `{screen.x + 224.0, screen.y + 16.0, screen.z}` (the native
adds `flt_470b4c`/`flt_470b48` itself and then uses the verbatim publisher,
not the offset one). The first `0x4489d0` reuses the outgoing stack
argument slot as the id output; the loop uses a second slot — both ids are
discarded.

Then: the `+0x474` timer block is lazily initialized (guard bit 0 at
`+0x484`, `-999999` prev sentinel, `&flt_476f78` rate) and unconditionally
reset to `{-1, 0, 0}`; `dword_477810` gates
`ReserveContextChannel(0x492590, 4, 0)` behind `+0x58` bit `0x200`; the
`+0x430c` block lazily initializes and is forced to
`{prev 5, mode 6, rate 6.0f}`; `0x43e710` detaches the object's ANM VM
(`ECX = obj+0x10`, `EAX = obj+0x14`, script 0); and once the stage state
(`dword_4776f4`) `+0x3738` counter reaches 60, `+0x3790 = 0` and bit 1 is
cleared from `+0x378c` and the seven per-life dwords
(`0xad4/0xe80/0x15d8/0x1984/0x1d30/0x20dc/0x2488`) — the same flag block
`0x00405500` maintains.

## 0x004267f0 CheckSceneTriggerRotatedRegionEaxEcxStackAbi

EAX = position pair, ECX = object, stack (rotation, scale, limit).
Rotates `anchor - position` by `-rotation`, then:

- `left = rx - +0x41c`, `top = ry - +0x420`, `right = rx + +0x41c`,
  `bottom = ry + +0x420`;
- when `left <= limit` and (`scale*192 < top` or `right < 30` or
  `scale*57 > bottom`): the deep window `left <= limit && right >= 30 &&
  scale*12 >= top && scale*144 <= top` answers **2**;
- otherwise, unless `*(dword*)(dword_47773c) == 0` or its `+0x9eb8` dword
  is set (the native tests `byte_477710+44` and `+40632`), a state word
  not in {2, 3, 4} and `+0x4310 <= 0` fire `FireSceneTriggerExpireEffect`
  and answer **1**;
- everything else answers **0**.

Note the native computes `bottom` inside the second condition only when the
first two short-circuit tests fail; the C++ short-circuit preserves this.

## 0x004266b0 (already covered)

`CheckEnemyTimeoutRegionEaxEdxEcxAbi` in `src/EclScriptLibrary.cpp`
implements this body; no new code was added. Its caller `0x00406240` passes
`dword_477864` as the region block and `obj+0x3f0` as the half extents.
