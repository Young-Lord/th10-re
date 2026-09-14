# Post-Run Replay Save Menu — 0x004236f0

`RunPostRunReplaySaveMenuStackAbi` (native `retn 4`; record on the stack).
Module: `src/PostRunReplaySaveMenu.cpp/.hpp`.

## Subsystem

The single caller `0x004223f0` is the calculation-record body dispatcher over
the 0x3ac-byte scheduler-record family (record+4 mode byte, common epilogue
comparing the rate float against `flt_470b68/b64` exactly like the player
mode dispatcher). Its jump table maps modes 0→`0x00422ab0`, 1..5→
`0x00422c80` (both separate targets), 6..13→`0x004236f0`, everything else to
the shared epilogue. `0x00422a90` is a thin `push esi/mov esi,ecx` ECX→ESI
adapter. `0x00422660` and `0x00422c80` are unrelated siblings.

## Record layout used

| Offset | Meaning |
| ------ | ------- |
| `+0x04` | mode (switch on `mode-6`, 8 cases at `jpt_423715`) |
| `+0x10..+0x20` | five-field scaled timer (`0x00405410` arm with duration 0) |
| `+0x24/+0x28/+0x2c` | cursor record A: value / saved / max (+0xf4 wrap flag) |
| `+0xfc/+0x100/+0x104` | cursor record B (charset selector), +0x1cc wrap flag |
| `+0x1d4`, `+0x1d8` | entity handle slots (A drives state words) |
| `+0x1e0` | typed-name length/cursor (name at `+0x2b4`) |
| `+0x1e4` | extra-stage flag (also drives `dword_491fb8`) |
| `+0x1e8` | replay-present gate (skips typing when set) |
| `+0x1ec` | 25 parsed `th10_NN.rpy` records |
| `+0x2b4` | name buffer (9+ bytes) |
| `+0x2c0` | float copied to `flt_476f78` in mode 13 |

## Mode map

- **6**: after timer>=10 and `0x474e36 & 0x1001`: menu sound 10 (EDI=0xa
  across the `0x43dc90` call), mode=7, state word 2 on handle A, arm timer.
- **7**: timer>=10; publish final score (0x421f60: score `0x474c44` to
  HUD owner `0x47770c`+0x9e78, max `0x474c40` raised). Practice
  (`byte 0x474ca0 & 0x10`): merge run score into the per-stage best at
  save data `0x47783c + (474c6c + 3*chara)*0x437c + (stage + 6*shot)*8
  + 0x4dc`, then mode-8 setup. Non-practice: `0x00423570` boundary
  (InsertScoreRecordEdi wrapper), arm timer; if `+0x1e8` set → mode-8 setup
  (max=3, clamp 0→2/>2→2/else v-1, `0x40c4d0` fire, state word
  zero-ext(v)+7); else mode=0xc + clear flag bit 2 (`0x4495e0`).
- **8**: replay-slot selector on cursor A. Shift ±1 on 0x10/0x20 keys
  (either flag dword or the 0x474e34 byte); sound 12 + sign-extended
  state word on change; under `0x1001`: highlight the child of kind
  `slot+0x7c` via `0x4497d0` + `0x4243f0` (stop word 6), sound 10, arm,
  mode=13, publish score. Selection 1 arms mode 0xa, clears flag 2, pushes
  cursor (`0x44be20`), max=25, wrap=1, clamped-zero store, and parses
  `th10_%.2d.rpy` for slots 1..25 into `+0x1ec`. Selection 2 re-arms with
  mode=13 and sound 10. Tail (0xa keys): sound 11; unless slot==2, clamp
  selector (2/2/v-1) and re-publish the zero-ext state word.
- **10**: file/slot browser on cursor A. Under `0x1001`: mode=0xb, write the
  clamped `+0x104` countdown into `+0xfc`, set `+0x104` = strlen(charset
  `0x4746d8`), wrap flag `+0x1cc`=1, `time()` into the replay header
  (`[0x477838]+0x18`+0xc), header stage = 8 when `+0x1e4` set and not
  practice else `0x474c7c`, copy the saved name `0x47783c+0x1d878` into
  `+0x2b4`, shift cursor B by -1 when the name is nine spaces, and trim
  trailing spaces into `+0x1e0`; sound 10. Tail (0xa keys): mode=8, set
  flag bit 2 (`0x449590`), finalize cursor A (`0x44be70`), sign-extended
  state word, max=3/wrap=1, release all 25 parsed slots (destroy + free),
  arm timer twice (mode written 8 both times), sound 11.
- **11**: name entry on cursor B (rows of 13: shifts -13/+13 and the
  `value % 13 == 0 / == 12` edge corrections). Under `0x1001`: index <
  len-3 types the character (advance +1; when the cursor reaches 8 the
  selector is parked via `0x40ad20(len-1, cursor B)`); len-3 inserts a
  space with the same advance; len-2 deletes one character (sound 11,
  early return); len-1 is END — sound 0x2c, release the previous parse of
  the selected slot, `CommitReplaySave(0x477838, "th10_NN.rpy", name)`,
  re-parse into the slot, arm with mode=0xa, and copy the file name into
  `0x47783c+0x1d878`. Tail (0xa keys): sound 11; non-empty name deletes
  the last character, empty name re-arms with mode=0xa.
- **12**: score-name entry. With `+0x1e8` clear the same 4-key cursor
  navigation runs first. Type path quirk: the character is written at the
  name cursor but the cursor is **not** advanced (unlike mode 11); only the
  space path advances. END stores the name at
  `0x47783c + (chara + 3*474c6c)*0x437c + 3*(cursor + 10*shot)*8 + 0x1e`
  (native register asymmetry versus mode 7 preserved) and into
  `0x47783c+0x1d878`, then flag bit 2, mode=8, max=3, clamp
  (>=0→0/else v-1), fire, zero-ext state word, sound 10. Tail (0xa keys):
  cursor A >= 0 and non-empty name → backspace; cursor A < 0 or empty →
  mode=8, clamp, arm, sound 10.
- **13**: timer>=12; `0x00423510` inline reconstruction: mode=0xd, first-time
  `+0x20|=1` and `+0x10=0xfff0bdc1` (immediately overwritten by -1 — native
  quirk kept); unconditionally count/acc=0 and prev=-1; expire both handles
  (`0x409e50`); `flt_476f78 = [+0x2c0]`. Then cursor A: 0 →
  `dword_491fb8 = (+0x1e4 != 0) ? 10 : 13` (neg/sbb/and 0xfffffffd chain);
  1 or 2 → `RunGameOverPathBStackAbi(0x491c28, 4)`.
- **9 / default**: bare epilogue.

## State-word ABI encodings

Two native encodings feed `0x449470`: `xor esi,esi; mov si,value; add si,7`
(zero-extended, used at 0x423829/0x423a5b/0x4241f9/0x424262) and
`movsx esi,ax; add si,7` (sign-extended high word kept, used at
0x4238e6/0x423c52). Both are modeled (`StateWordArgZeroExt` /
`StateWordArgSignExt`).

## Boundaries kept

`0x004296f0` `ParseDemoRecord`,
`0x004294a0` `DestroyDemoParseObject`. Implemented inline here:
`0x00421f60`, `0x004243f0`, `0x00449590` (set-twin of `0x4495e0`),
`0x00423510`, `0x004297b0`. The former `0x00423570` boundary is now the
semantic `SaveRunHighScoreEntry` (`src/SaveRunHighScoreEntry.cpp`,
`docs/evidence/save-run-high-score-entry.md`).

## Verification

`g++ -m32 -std=c++98 -fsyntax-only -I src src/*.cpp` — 0 errors.
