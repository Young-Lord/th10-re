# Title -> Game-Scene Setup Orchestration (TH10 0x00417870)

Implemented as `SetupGameSceneFromTitle` in `src/TitleSceneSetup.cpp/.hpp`.
Native stdcall with one stack argument (the published title-screen state;
caller 0x00417c70). Returns 0 on success, -1 through the shared failure
tail (0x00417b3e..0x00417b7e) on any setup error. The native forward jumps
into that tail are modeled with a `failed` flag in source order.

## Flow

1. `state[+0x58] |= 4` (scene-setup busy). Wait loop: while either of the
   first two signed dwords of the render owner (DAT_00491c10 object) is
   still >= 0, `Sleep(1)`; the 0x491ff4 bit 0x80 aborts into the failure
   tail.
2. `flt_476f78 = 1.0`; 0x474c88/0x474c8c cleared.
3. When 0x491fc4 != 0 (game active):
   - mode 0x474c7c == 7 forces difficulty block 4 and 0x474c74 = 4;
   - the stage-record row of the save bank (DAT_0047783c) is addressed as
     `base + 34552*A + 17276*(A+B)` (A = 0x474c68, B = 0x474c6c) plus
     `240*difficulty`; +24 feeds 0x474c40 and byte +29 feeds 0x474c94;
   - 0x474ca0 bit 3 clear resets 0x474c90; 0x474c44 = 0;
     0x418b80(50000) resets the frame-state block;
   - 0x474ca0 bit 0x10 selects the practice start index (0x474cac - 1 or
     9) over the default 2 (0x474c70); bit 2 is cleared;
   - `state[+0x5c] == 0` increments the play count at save offset
     `17276*(A+B+2A)+1224`, capped at 99999;
   - 0x474c9c = 0; 0x474c98 = 0xFFFFFE00 when bit 3 set, else 0.
   Else (`0x491fc4 == 0`) 0x474c44 > 0x474c40 raises the high score.
4. `0x474c50 = 9`; the scene calc record (callback 0x4187c0, priority 10)
   and draw record (0x4187d0, priority 4) are created via 0x449ed0 with
   the enabled bit (0x4+2) cleared and the title state stored at +0x20,
   then registered via 0x449ae0/0x449b70 on the scheduler at DAT_00491be4
   (modeled with `CallbackSchedulerApi`); stored to state +0x08/+0x0c.
5. 52 bytes of manager-state defaults from DAT_00491d48 copy into
   state +0x24; `state[+0x04] = *DAT_00477848`.
6. Scene build:
   - 0x474ca0 bit 1 set (replay/continue reuse): 0x42a6a0 (now the
     semantic `PrepareReplaySceneReuse(g_GameModeObject)`, EBX = the
     DAT_00477838 replay context; see
     docs/evidence/replay-scene-reuse.md), 0x413a20 on
     DAT_0047770c, then 0x402640(record+4, 0);
   - otherwise the cascade 0x429610(state[+0x5c], 0x477710),
     0x402640, CreateAsciiHudOwner 0x414830, CreatePlayerStateBlock
     0x425020, CreateEffectManagerRoot 0x406060, 0x41aed0, 0x41c290,
     0x422360, 0x419090 and finally 0x42b660 — any zero result fails.
7. Post-build: 0x474ca0 bits 0/8 → 0x417800, else 0x40d6b0(record+12);
   then 0x40af90 / 0x4056b0 / 0x408c90. With bit 0x20 clear:
   0x420c00 and 0x420a90 on channels 0/1 with the name pointers at
   record+16/+20. Doubles at DAT_00477708+36/+44 zeroed; 0x405410(0);
   spin on 0x494518 with `Sleep(16)`; 0x474c84 semantics clear 0x474c8c.
8. Success tail (0x00417c2b..): 0x421070(&0x491c28) (now the semantic
   `LeaveGameManagerGateStackAbi` — it stops the three slot background VMs
   with state word 1 and clears the +0x6fc latch; see
   docs/evidence/game-manager-gate-vms.md), `+0x58 &= ~4`,
   `0x474ca0 &= 0xFFFFFFF4`, 0x492264 = 0, 0x492260 = 1, 0x4918a4 = 0,
   0x409e20 re-enables the scheduler records; return 0.

Failure tail: `+0x58 |= 8`, 0x421300(&0x491c28) (semantic
`EnterGameManagerGateStackAbi`, state word 2 / latch 2), 0x492264 = 0,
0x492260 = 1, both registered records re-enabled (`flags |= 2`), return -1.

## Notes

- The manager-creation leaves and the scheduler API are reused semantic
  bodies; every other mode-worker leaf (0x413a20, 0x402640,
  0x429610, 0x41aed0, 0x41c290, 0x422360, 0x419090, 0x42b660,
  0x417800, 0x40d6b0, 0x40af90, 0x4056b0, 0x408c90, 0x405410)
  remains an explicit declaration-level boundary with its native ABI noted
  at the declaration site.
- `scripts/compile-main-chain-cpp.sh` does not list the new translation
  unit yet (per instructions the script was not modified); MSVC syntax of
  the shared headers is unchanged and the g++ -m32 -std=c++98 syntax check
  passes.
