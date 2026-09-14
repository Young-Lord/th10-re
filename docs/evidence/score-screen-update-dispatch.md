# Score-Screen Menu Dispatch (TH10 0x00431ee0 / 0x0042d420)

Both routines are per-state calculation bodies dispatched by the game-manager
calculation controller 0x0042cdf0 (jumptable at 0x0042ce37) with the manager
in EBX and a plain `retn` returning 1 in EAX — the same ABI as the
already-reconstructed result-screen body 0x0042f540
(`UpdateResultScreenStateMachineEbxAbi`).

## 0x0042d420 — state 2: eight-row mode-selection menu

Implemented as `RunManagerStateBody2` in `src/ScoreScreenUpdate.cpp/.hpp`
(global scope to match the extern in `TitleGameManagerLifecycle.cpp`).

Sub-state at +0x20:

- **0**: arm the cursor record at +0x24 (value +0x24, copy +0x28, maximum
  +0x2c = 8). When the score-save unlock bank probe 0x0042c850 (EAX =
  DAT_0047783c; tests the six bytes at +0x1d808..+0x1d80d) reports the bank
  empty, one `1` is appended to the pending-flag list at +0xb4 (index word
  +0xf8, incremented in place). When DAT_00474ca0 carries bit 0x10, the
  cursor value is seeded with the native sign-split idiom (max 0 or > 2 →
  2, else max - 1) and the bit is cleared. Script 0x58 is spawned when the
  handle at +0x424 fails the 0x4491c0 lookup (EDX = manager, id pushed);
  script 0 always spawns. Sub-state = 1. Native case 0 falls through into
  the case 1 body.
- **1**: once the +0x2b4 timer (signed) exceeds 10: sub-state = 2, the
  +0x2c4 entity runs with stop word 3 (0x449250), and slot
  `u16(+0x24) + 17` gets stop word 0 (0x42c770: EAX = manager, ESI = slot,
  ECX = value).
- **2**: cursor handling over the 0x474e36 low byte / 0x474e34 byte masks.
  Shifts via 0x44bea0 (0x10 → -1, 0x20 → +1); on change: boundary channel
  0xc, 0x449250 stop word 3 on the +0x2c4 entity, and slot
  `u16(cursor) + 7` stop word 0. The 0xa low-byte mask drives the
  exit/accept channel (0xb); non-row-7 accepts clamp the cursor to
  maximum - 1 (native sign-split), run the 0x42c750 side effect on slot 0
  and refresh the slot-(cursor+7) stop word. The 0x1001 dword mask then
  queues the slot-6 stop word and dispatches on the row:
  rows 0-4 → channel 0xa, slots 90/91 stop word 7, sub-state 4;
  row 5 → the same plus a second channel-0xa reservation;
  row 6 → channel 0xa, sub-state 4, then 0x42c750 on slots 90 and 91;
  row 7 → channel 0xb, sub-state 4.
- **4** (timer >= 20): acts on the accepted row:
  row 0 → clear 0x474ca0 bit 0x10, release slot 0x58, state 6, cursor
  finalize 0x44be20, then clamp DAT_00474c74 below 4 (>= 4 → 1) and push
  the resulting difficulty into the cursor record via 0x40ad20
  (ECX = value, EDX = record);
  row 1 → clear bit 0x10, release 0x58, state 6, 0x44be20, park the
  difficulty in +0x58f0, set DAT_00474c74 = 4, and re-clamp the cursor
  from the maximum (positive/zero → 0, negative → max - 1);
  row 2 → set bit 0x10 then the row-0 tail;
  rows 3/4/5 → release 0x58, states 12/11/14, 0x44be20;
  row 6 → state 4, 0x44be20 (no release);
  row 7 → state 3 only (no release, no finalize).

## 0x00431ee0 — state 0xB: extra-mode unlock menu

A body already existed in `src/GameManagerStateBodies.cpp`
(`RunManagerStateBodyB`); it was audited against the disassembly and
completed this session. Corrections/additions:

- case 0 now falls through into the case 1 timer check (native
  `goto LABEL_15`), and the case 1 threshold is `> 6`.
- The four cursor-shift directions run the 0x449250 sound-entity stop
  word 2 on the handle slots +0x56c/+0x570 (difficulty cursor) and
  +0x564/+0x568 (shot-type cursor).
- The secondary (difficulty) cursor change releases the OLD +0x160 slot
  and spawns the NEW +0x160 script (was inverted), and the page-cursor
  reload follows the native sign-split of the +0x1dc page count
  (0 or > 1 → 1, == 1 → 0), gated on the page cursor being positive.
- The primary (shot-type) change releases old+0x154 / spawns new+0x154 and
  old/3+0x152 / spawns new/3+0x152 (was inverted).
- The 0x1001 accept branch now splits after the page shift: page != 0
  refreshes the list render (0x432690), page 0 expires the ten +0x5d4
  handles (0x409e50 loop) — previously the refresh ran unconditionally.
- The previously missing **22-key unlock-sequence listener**
  (0x432396..0x4324ce) is implemented as `RunExtraUnlockCodeListener`,
  active while the difficulty cursor (+0xfc) is 4 and the shot cursor
  (+0x24) is 2:
  - any 0x474e36 bit in 0x160b resets the progress (0x4979a4) and idle
    timer (0x4979a0);
  - 0x100 bytes of keyboard state are shifted current→previous
    (0x497e90 → 0x497d90) and the keyboard sampler 0x44b010 refills the
    current buffer (zero-fill, 0x4924fc gate, 0x491ff4 bit 0x200 DI
    selection with one Acquire on failure; nonzero return = DI path ran);
  - on the DI path, the four interleaved pressed-edge lanes at 0x4979a8
    are recomputed as `cur & (cur ^ prev)`;
  - progress >= 0x16 completes the sequence: 0x42c8c0 unlocks every spell
    card (flag bytes 0x1d802..0x1d811 = 0x01, 6x4x6 grid at save +0x4e9
    stride 17276 = 1) and reserves boundary channel 0x2c;
  - otherwise a press on `edge[0x46ef10[progress]]` (signed-byte test)
    advances the sequence; any press among the every-third triples of edge
    bytes (indices 0..56 step 3, bytes k/k+1/k+2 — the native address
    arithmetic at 0x4979a6+eax resolves to the same lanes) restarts it;
  - `++0x4979a0 > 300` resets progress and timer.

## Status

`g++ -m32 -std=c++98 -fsyntax-only -I src src/*.cpp` passes. IDA names and
comments applied at both addresses.
