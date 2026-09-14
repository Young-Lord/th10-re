# Result-Screen State Machine — 0x004172e0

Module: `src/ResultScreenState.cpp/.hpp`
(`ApplyResultScreenStateEaxStackAbi`). Native EAX = mode, stack =
{state owner, bonus}; `ret 8`. The boundary declaration previously kept in
`src/SpellBulletVtable.cpp` was replaced: `FinishSpellCardPracticeEaxAbi`
(0x409c00) calls mode 0 with the DAT_0047770c owner after the captured
bonus was added to the score.

## Dispatch

`cmp eax,6 / ja epilogue` then `jmp [eax*4+0x417560]`. Jump table:

| mode | target | behavior |
| ---- | ------ | -------- |
| 0 | 0x4172f9 | full score-digit rebuild (below) |
| 1 | 0x417441 | release + respawn the +0x9e14 banner (script 0x48) |
| 2 | 0x417460 | release + respawn the +0x9e18 banner (script 0x49) |
| 3 | 0x4174a0 | same with script 0x4a |
| 4 | 0x4174e0 | same with script 0x4b |
| 5 | 0x417558 | no-op: jumps straight into the epilogue (native quirk) |
| 6 | 0x417520 | release + respawn the +0x9e14 banner (script 0x4c) |

Out-of-range modes fall through `ja` to 0x417559. All banner respawns share
the 0x41753a tail: `0x448d00` (`SpawnSetupEffectVmListABack`, dead first
stack argument = the +0x9ec8 resource, kind 0xf), unchecked dereference of
the returned record's first dword into the handle slot.

## Mode 0 — score rebuild (0x4172f9)

1. Release the +0x9e14 handle (`0x4492a0`) and zero it; respawn it from
   script 0x47 / kind 0xf via 0x448d00 and store the record's first dword
   (unchecked dereference).
2. Loop `i = 0..7` over the eight digit slots at +0x9df4 (stride 4):
   - release and zero the slot's handle,
   - allocate a VM from the 4096-slot pool (`0x449950`, ESI = DAT_00491c10),
     set kind 0xf at +0x20, OR 0x40000000 into +0x35c,
   - `0x449870` binds script `0x27 + i` (the native also carries the
     +0x9ec8 bind context in ECX; that manager-work bind tail lives inside
     the semantic body),
   - `0x4489d0` links the VM into list 1 and assigns the id, stored into
     the slot,
   - signed `idiv` of the running bonus value by the divisor (starts at
     0x989680 = 10^7, then divided by 10 each pass via the 0x66666667
     magic): quotient = this digit, remainder feeds the next pass,
   - `0x4491c0` resolves the new id; when found, `0x43e5a0`
     (`InitializeAsciiAnimationVmEntry`) rebinds the VM to entry
     `quotient + 8` with the resource at entity+0x308,
   - the +0x35c bit-2 visibility flag is set through `0x449590` once a
     nonzero digit was seen and cleared through `0x4495e0` for leading
     zeros (slot-based helpers over DAT_00491c10; the set-twin is a local
     copy, matching the existing pattern in PostRunReplaySaveMenu.cpp).

Net effect: the eight score digits (most significant first, scripts
0x27..0x2e, digit sprite entries 8..15) with leading-zero suppression.

## Status

`g++ -m32 -std=c++98 -fsyntax-only -I src src/*.cpp` passes. CSV row
appended for 0x004172e0.
