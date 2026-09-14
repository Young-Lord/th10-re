# Pause Menu Modes — 0x00422c80 / 0x00422c30

Module: `src/PauseMenuModes.cpp/.hpp`.

## Subsystem

Sibling mode handler of the post-run replay-save menu inside the
calculation-record body dispatcher `0x004223f0` (see
`docs/evidence/post-run-replay-save-menu.md`). The dispatcher's jump table
(byte table at `0x4224b0`, jump table at `0x4224a4`) maps mode 0 →
`0x00422ab0`, modes 1..5 → `0x00422c80` (switch on `mode-1`, five cases,
table at `0x4231bc`) and modes 6..13 → `0x004236f0`, all sharing the
epilogue that compares the rate float against `flt_470b68`/`flt_470b64`.

Pause identity evidence: the resume helper queues the BGM command with the
string `"UnPause"` (0x46e0b8; `"Pause"` sits at 0x46e0bc) and clears bit
0x10 of the `DAT_00477810` state object's +0x58 word; the dispatcher's
mode-0 gate tests `DAT_00474ca0` bit 0x20, `0x474e36` bit 3,
`0x491ff4` bit 0x10, `[0x477810]+8` bit 1 and `[0x477810]+0x14 >= 30`
before entering mode 0.

## Record layout used

| Offset | Meaning |
| ------ | ------- |
| `+0x04` | mode (switch on `mode-1`) |
| `+0x14` | timer count (shared five-field scaled timer at +0x10) |
| `+0x24/+0x28/+0x2c` | cursor record: value / copy / maximum |
| `+0xf4` | wrap flag |
| `+0x1d4/+0x1d8/+0x1dc` | three entity handle slots |
| `+0x2c0` | float restored to `DAT_00476f78` by the resume helper |

## Mode map

- **1** (timer >= 10): mode = 2; maximum `+0x2c` =
  `([0x477810]+0x5c != 0) ? 2 : 3` (neg/sbb/add-3 idiom); wrap flag = 1;
  cursor clamped against the maximum (max < 0 → max-1, else 0); state
  word `zero-ext(u16(cursor)+7)` on handle A (`0x449470`). No re-arm.
- **2** (item menu): cursor copy; shifts ±1 on `0x474e36`/`0x474e34`
  masks 0x10/0x20 via `0x44bea0`; on change state word
  `zero-ext(cursor+7)` + sound 0xc.
  - accept (`0x474e36 & 0x1001`): sound 0xa; switch on the cursor value:
    0 → expire handles B/A/C, mode 3; 1 → highlight child kind 0x75 of
    handle A (`0x4497d0` + `0x4243f0`), mode 4; 2 → highlight kind 0x74,
    mode = `([0x477810]+0x5c != 0) ? 3 : 4`; other values unchanged.
    All paths re-arm the timer (`0x405410`, duration 0).
  - `0x474e36 & 0x4000`: sound 0xa; highlight kind 0x75; arm; mode 3;
    cursor clamped (0 → 2, > 2 → 2, else max-1); expire handle B.
  - `0x474e36 & 0x200`: sound 0xa; highlight kind 0x74; arm; mode 3;
    cursor clamped (0 → 1, > 1 → 1, else max-1).
  - shared cancel tail (below).
- **3** (terminal; timer >= 12): mode = 0; cursor 0 → resume helper;
  cursor 1 → expire handle A, `0x40ac90(0x491c28, 4)`
  (`RequestGameStateTransitionEaxStackAbi`); cursor 2 → expire handle A,
  `DAT_00491fb8 = 10`. No timer re-arm.
- **4** (confirm submenu): at timer == 20 `0x44be20` pushes the cursor,
  maximum = 2, wrap = 1, cursor clamped (0 or > 1 → 1, else max-1) and
  state word 0xe is published; at timer == 30 the state word becomes
  `zero-ext(cursor+0xf)`; then the 0x474e30-bank cursor shifts (0x10 →
  -1, 0x20 → +1 via `0x44bea0`) publish `zero-ext(cursor+0xf)` + sound
  0xc on change. Accept (`0x1001`): sound 0xa, highlight child kind 0x77
  (cursor 0) or 0x78 (cursor 1), mode = 5, arm. Falls into the cancel
  tail.
- **5** (commit; timer >= 20): cursor 0 → expire handles B/A, mode 3,
  cursor finalize `0x44be70`; cursor 1 → `0x44be70`, state word
  `sign-ext(cursor+7)` (`movsx esi,ax; add si,7`), mode 2, arm, early
  return; other values only re-arm.

## Shared cancel tail (0x423059, modes 2 and 4)

While `0x474e36` bit 3 is set: clamp the cursor against `+0x2c`
(max < 0 → max-1, else 0), expire handles B/A/C, mode = 3, arm.

## 0x00422c30 — resume helper (native ESI = record)

1. Clears bit 0x10 of `[0x477810]+0x58`.
2. `QueueBgmCommand(0x492590, "UnPause", 7, 0)` (TH10 0x43e460).
3. Soft-releases the entity id in the record's +0x1dc slot through
   `0x4492a0` (`ReleaseEntityById`, EDX = owner `0x491c10`) and zeroes
   the slot.
4. `[0x476f78] = [record+0x2c0]` (float copy).

## Boundaries kept

`0x405410`, `0x44bea0`,
`0x44be20`, `0x44be70`, `0x40ace0`, `0x449470`, `0x4497d0`, `0x4243f0`
(re-implemented locally, see the reconciliation note in
`docs/evidence/replay-context-and-player-damage.md`), `0x409e50`,
`0x40ac90`, `0x43dc90`, `0x43e460`, `0x4492a0`. The former `0x00422ab0`
and `0x00423570` boundaries are now semantic bodies
(`src/PauseEnterSetup.cpp`, `src/SaveRunHighScoreEntry.cpp`).

## Verification

`g++ -m32 -std=c++98 -fsyntax-only -I src src/*.cpp` — 0 errors.
