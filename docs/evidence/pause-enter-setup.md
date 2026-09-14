# Pause-Entry Setup (Mode 0) — 0x00422ab0

Implemented as `RunPauseEnterSetupStackAbi` in
`src/PauseEnterSetup.cpp/.hpp`. Mode-0 body of the 0x004223f0 calculation
record dispatcher (see `docs/evidence/pause-menu-modes.md`); native stack ABI
`ret 4` with the 0x3ac-byte scheduler record as the only argument.

## Flow

1. `record+0x04 = 1` (mode 1 — the item-menu arm state).
2. Timer family init at +0x10..+0x20, the same reduced sequence as 0x00423510
   (see `docs/evidence/post-run-replay-save-menu.md`): when the +0x20 flag
   bit 0 is clear, store 0 / NaN sentinel 0xFFF0BDC1 / 0 into +0x14/+0x10/
   +0x18, the rate pointer `&DAT_00476f78` into +0x1c and set the flag bit;
   the unconditional tail then overwrites +0x14/+0x18 with 0 and +0x10 with
   0xFFFFFFFF (also NaN), making the branch stores dead. Preserved verbatim.
3. `DAT_00477810 + 0x58 |= 0x10` — the pause bit (the resume helper
   0x00422c30 clears exactly this bit).
4. Menu effect VM: pool-allocate (0x00449950, ESI = owner 0x491c10), set
   `+0x35c |= 0x40000000` and kind `+0x20 = 0xf`, bind script 0 through
   0x00449870 (native ECX = `[DAT_004776e0]+0x8998`, the ASCII manager's ANM
   manager-work; the semantic body feeds its bind context internally), link
   into the owner list B (0x00448ac0) and store the id at `record+0x1d8`
   (handle B).
5. Full-screen pause overlay: `0x00424480` (semantic
   `CreateGameOverOverlay`) with native ESI = 0x491c10 and stack
   `(handle B id, 0x20, 0x10, 0x180, 0x1c0)`; `ret 0x14`, result ignored.
6. HUD watch VM: read `edi = [DAT_0047770c]+0x9ec8` (the HUD owner's ANM
   manager-work), mirror it into `record+0x2c4`, pool-allocate, set the flag
   and kind 0xf, bind script 0x79 (native ECX = edi), link into list B with
   the id written directly into `record+0x1d4` (handle A), then fire handle A
   (0x0040c4d0, state word 3).
7. Pause sound: `0x43dc90` (semantic `ReserveContextChannel`) on context
   0x492590 with native EDI = 0x20 and stack 0. Then the "Pause" BGM command
   (string at 0x46e0c0; "UnPause" at 0x46e0b8) through 0x43e460
   (`QueueBgmCommand(0x492590, "Pause", 6, 0)`).
8. When `[DAT_00477810]+0x5c != 0`: resolve the kind-0x75 child of handle A
   (0x004497d0, native ESI = the handle slot, EBX = kind, EDI = out) and
   release it through 0x004492a0 (EDX = owner) — unconditionally, using the
   possibly-zero resolved handle — then zero the out slot.
9. Park the frame-time scale: `record+0x2c0 = *(u32*)&DAT_00476f78` (raw
   dword copy) and reset `DAT_00476f78 = 1.0f`.

## Record layout used

| Offset | Meaning |
| ------ | ------- |
| `+0x04` | mode (set to 1) |
| `+0x10..+0x20` | five-field scaled timer (NaN-sentinel init quirk) |
| `+0x1d4` | handle A (HUD script-0x79 VM; fired) |
| `+0x1d8` | handle B (menu script-0 VM) |
| `+0x2c0` | saved DAT_00476f78 float (restored by the resume helper) |
| `+0x2c4` | mirror of `[DAT_0047770c]+0x9ec8` |

## Verification

`g++ -m32 -std=c++98 -fsyntax-only -I src src/*.cpp` — 0 errors.
