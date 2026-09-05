# TH10 0x00413bc0 — ResetAsciiHudOverlayEdiAbi

Semantic body: `src/AsciiHudOverlayUpdate.cpp` (`th10::ResetAsciiHudOverlayEdiAbi`).

## ABI

- Native EDI = the DAT_0047770c ASCII HUD overlay owner. ECX is stored to a
  stack slot at entry and never read again (caller garbage).
- Returns garbage EAX (last list node / counter reads); modeled `void`.
- Both callers (the ECL-select spell-exit sequence at `0x40a450` via
  `src/EclSelectMenu.cpp` and one other) pass the owner loaded from
  `DAT_0047770c`.

## Verified behavior (0x413bc0..0x41436c)

1. Scheduler records: `owner+8` and `owner+0xc`, when non-null, get bit 1
   set in their `+4` flag dword (disable).
2. If `owner+0x9e58 == 0`, a pool VM is spawned (script 0, resource
   `owner+0x9ec8`) and its entity id stored at `+0x9e58`. If
   `owner+0x9e5c == 0`, a second VM (script 1) is spawned and its id is
   discarded. The spawn sequence is the inlined `0x449950` /
   `0x449870` / `0x4489d0` chain: pool VM, `vm+0x20 = 15`,
   `vm+0x35c |= 0x40000000`, script bind, list-A tail link with the
   wrapping id counter at `manager+0x732454`.
3. Unless `owner+0x36c` bit 0 is set, every glyph pool VM is rebound via
   `0x43e710` (`AssignAnmScriptToVmEcxEaxBbxAbi`, ECX = resource,
   EAX = vm, EBX = script):
   - 10 iterations: `owner+0x10+i*0x3ac` with script `10+i` and
     `owner+0x24c8+i*0x3ac` with script `20+i`
   - 9: `owner+0x4980`, scripts 30..38
   - 4: `owner+0x6a8c`, scripts 47..50
   - 2: `owner+0x793c`, scripts 80..81
   - 7: `owner+0x8094`, scripts 51..57
4. Life-slot flags at `owner+0x4cdc + i*0x3ac` (9 slots): the first
   `DAT_00474c70` (signed lives) get `|= 2`, the rest get `&= ~2`. The
   native enable loop runs the counter verbatim, so values above nine keep
   setting flags past the table; the clear loop only runs when
   `counter < 9`.
5. Lives display via `0x43e5a0` (`InitializeAsciiAnimationVmEntry`,
   EAX = vm, EDX = entry, ECX = resource): `(short)DAT_00474c48 / 20 + 8`
   into `owner+0x6a8c`, and the remainder scaled by
   `100 * (v % 20) / 20` split into tens/ones entries `+8` into
   `owner+0x71e4` / `owner+0x7590` (VMs 2 and 3 of the 0x6a8c pool).
6. Background pair selection: shared status `DAT_00491fb8 == 8` skips the
   pair; otherwise when `DAT_00474ca0` bit 0x20 is clear, scripts 0 and 1
   are spawned with resource `owner+0x9e80` (note: not `+0x9ec8`) and the
   ids discarded. When bit 0x20 is set (any status), script 113 is spawned
   with resource `owner+0x9ec8`.
7. The auxiliary VM at `owner+0x9a48` is rebound with script 0 and the ANM
   manager-work at `DAT_004776e0+0x8994` (this matches the `+0x9a48`
   conditional pool in the HUD batch `0x415800`).
8. Script-79 spawn when `DAT_00474c7c == 1 && [DAT_00477810+0x5c] == 0 &&
   DAT_00474c90 == 0` (stage-start text layer), id discarded.
9. When `DAT_00491fc4 != 0`: script `102 + DAT_00474c74` (difficulty) is
   spawned with resource `owner+0x9ec8`; its id is stored at
   `owner+0x9e50` (zero when the counter reads zero, in which case the
   resolve is skipped) and then resolved through the inline
   `0x449470`-equivalent walk (lists A then B, state word 3 at
   `entity+0x304`, child propagation over `entity+0x14` while
   `entity+0x18 == 0`). Modeled with the shared
   `SetEntityStateWordEaxEsiAbi`.
10. Unconditionally: script `107 + difficulty` spawns, and the current id
    counter is copied to `owner+0x9e54`; the `+0x9e50` id is resolved again
    with state word 3; finally `owner+0x9e90 = 0` on every exit path
    (including the not-found early return at `0x414327`).

## Boundaries

All callees were already reconstructed and are called as semantic bodies:
`AllocatePoolVmEsiAbi`, `AssignPoolVmScriptEcxEaxAbi`,
`LinkEntityAndAssignIdEaxEsiAbi`, `SetEntityStateWordEaxEsiAbi`
(`src/EntityHelpers.cpp`), `AssignAnmScriptToVmEcxEaxBbxAbi`
(`src/PlayerShotData.cpp`), `InitializeAsciiAnimationVmEntry`
(`src/AsciiAnimationVm.cpp`).

Globals `DAT_00474c90` (`g_HudGateState474c90`) and `DAT_00491fc4`
(`g_HudGateFlag491fc4`) had no prior names in the tree; they are declared
with neutral address-derived names.

## Wiring

`src/EclSelectMenu.cpp` now calls `ResetAsciiHudOverlayEdiAbi` in place of
the former `CleanupAsciiHudOverlayEdiBoundary` extern declaration.
