# Enemy Death Effects (0x40c9a0 / 0x40c9d0 / 0x40e5f0)

Implemented in `src/EnemyDeathEffects.cpp/.hpp`. The task brief's tentative
labels ("ECL instruction/table init") were not supported by the binary; the
disassembly shows the enemy-death effect/drop cluster of the script manager.

## 0x0040c9d0 — death scatter (ESI = position {x,y,z}, stack ret 4 = table)

Scatter table layout (base = stack argument):

```
+0x00 u32 pending particle kind (cleared by the callers)
+0x04 u32 counts[11]      rows 0..10
+0x30 u32 extra dword cleared together with the counts
+0x34 f32 x radius        arg to cos
+0x38 f32 y radius        arg to sin
```

Per particle of row r:

- `0x413270` (thiscall) writes `{cos(angle) * rx, sin(angle) * ry}`.
- Scale draw (inline LCG at 0x4918b0, counter +2 at 0x4918b4):
  `scale = draw * 2^-33 + 0.5` (flt_470d18 / flt_470b0c), range [0.5, 1).
  The combined draw duplicates the second LCG half, `(h2<<16)|h2`.
- Position = `{x + ox*scale, y + oy*scale, z}`.
- `0x41bb00` (EAX = `*(u32*)0x477818` effect manager, ECX = position,
  stack = kind row+1, color 0xffffffff, angle -1.5707964, speed 2.2).
- Angle advance: `(draw * 2^-31 - 1) * 0.8 + angle + 1.60773`
  (flt_470bec / flt_470afc / flt_470c48 / flt_470b94 = 0x3fce147b), then
  wrapped into (-pi, pi] by `0x44bc70`; the wrapped value is the next angle.
- Initial angle = a `0x44bb90` draw (`combined*2^-31 - 1`) times 3.25
  (flt_470b18), unwrapped.

After the 11 rows, 12 dwords from table+4 (counts + the extra dword) are
cleared with `rep stosd`; the radii and the pending kind are not touched.
Return value 0.

## 0x0040c9a0 — pending-kind flush (EAX = position, EDI = kind slot)

If the slot is positive, spawns one particle of that kind at the position
(same constants), then runs the scatter with the slot as the table base and
clears the slot. Plain `ret`.

## 0x0040e5f0 — death sequence (stdcall ret 4 = script manager)

Manager record fields: `+0x1068` position, `+0x2408` pending big-burst kind
(cleared afterwards), `+0x240c..` scatter table, `+0x243c/+0x2440` radii
(not cleared), `+0x2444` death sound slot, `+0x2448` death effect id,
`+0x244c` effect table index.

1. Sound slot >= 0: `0x43dd10` (ESI = 0x492590 sound manager, stack = x).
2. Effect id >= 0: `0x448db0` (EDI = position, stack = {table entry
   `(*(u32*)0x477704)[idx] + 0x30`, effect id, manager}) — allocates a
   render VM record, position +1/256 / -0.5, flag 0x40000000, script bind
   via 0x43e710, registration via 0x4489d0 (boundary).
3. Kind > 0: one `0x41bb00` burst of that kind at the position.
4. Scatter run (table base = manager+0x2408) and kind clear.
5. `0x412ff0` with EDI = 0x474c40 and amount 10: ticks the popup counter
   (`< 130` guard, `0x44bf40` timer shift) and re-arms the 130-step
   animation block (sentinel -999999, rate 192.0f, flag bit 0). Boundary.
6. Always returns 1.

## Integration

`EclScriptLibrary.cpp` now calls the implemented
`TriggerEnemyDeathSequenceStdcallAbi` and `RunEclContextListEdiStackAbi`
(see `docs/evidence/ecl-script-vm.md`) instead of their externs. The LCG
draw helpers mirror the inline sequences; see the PrngUnitFloat scaling
note in `ecl-script-vm.md`.

## Native boundaries kept

`0x43dd10`, `0x448db0`, `0x412ff0`, `0x41bb00`, `0x44bc70`
(WrapAngleToPi, semantic), `0x413270` (modeled as cos/sin).
