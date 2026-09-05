# Owner List Registration And Spawn Attribution Audit (2026-09-03)

## Four registration helpers (complete)

The render owner keeps two entity lists; each node lives at
`record+4 = {self, next, prev}` and ids come from the shared counter at
`manager+0x732454` (wrap past zero to 1):

| addr | list | insertion | semantic C++ |
| --- | --- | --- | --- |
| 0x4489d0 | A (+0x72dad4/+0x72dad8) | tail | `LinkEntityAndAssignIdEaxEsiAbi` |
| 0x448a50 | A | head | `LinkEntityFrontAndAssignIdEaxEsiAbi` |
| 0x448ac0 | B (+0x72dadc/+0x72dae0) | tail | `AttachEffectVmToListB` |
| 0x448b40 | B | head | `AttachEffectVmToListBFront` |

## Four spawn creators (implemented and wired)

`0x448d00/0x448e30/0x448f60/0x449090` each do: pool-VM alloc through the
global `DAT_00491c10`, `+0x20 = kind`, `+0x35c |= 0x40000000`, script bind
(0x449870 semantics), then one of the four registrations. The native first
stack argument is dead (`ret 0xc`; the manager is the global). C++ models:
`SpawnSetupEffectVmListABack/Front`, `SpawnSetupEffectVmListBBack/Front`
in `src/TimelineRenderObjects.cpp`. Callers:

- `DispatchSetupOpcode` spawn opcodes: 0x58→A-back, 0x5a→B-back,
  0x5b→A-front, 0x5c→B-front (verified in `0x43ee30`).
- Record-interpreter `0x40bd80` opcodes 8/15/16/17 call `0x448d00`
  (A-back) with `(dead arg, script = record+0xc, kind = 0xf)`. The C++
  record interpreter now uses `SpawnSetupEffectVmListABack`; the former
  `CreateTimelineObject` (owner-node + manager-work clone) was removed.

## Rechecked: no 0x4492a0 conflict (flag is the same bit)

`0x04000000` and `0x4000000` are the same value (bit 26; the leading zero
is cosmetic). `EntityHelpers::ReleaseEntityById`, the
`TimelineRenderObjects` release path (`MarkNodeForRelease`), and the live
`0x4492a0` all set `record+0x35c |= 0x4000000` and propagate to the child
list at +0x14 while `+0x18 == 0`. `LargeRenderOwnerFrameLoop` consumes that
same bit to reclaim nodes. `0x449630` is the slot-release wrapper
(`0x4492a0(*slot); *slot = 0`), modeled by `SetTimelineObjectPhase`.
