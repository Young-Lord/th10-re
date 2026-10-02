# Title-Screen State Constructor — 0x00402230

Implemented as `CreateTitleScreenStateEaxEcxStackAbi` in
`src/TitleScreenStateCtor.cpp/.hpp`. Native usercall: EAX = the 0x2b64-byte
title-screen state object (allocated and first-constructed by the 0x402640
factory, which passes the mode-record's stage-data name in ECX and the
priority base dword on the stack; `ret 4`). Returns 0 on success, -1 on the
stage-data load failure. The factory (0x402640, stdcall `ret 8`) destroys the
state through 0x00402440 + the shared delete when the constructor fails.

## Publication selector and priorities

- `test base,base` — base 0 publishes the state into DAT_004776e8
  (`g_TitleScreenStateSecondary`), any nonzero base into DAT_004776e4
  (`g_TitleScreenStatePrimary`). The observed call from 0x00417870 passes 0.
- Scheduler-record priorities are read as `base + N` values in EDI at the
  0x00449ae0/0x00449b70 registration calls: calc = base+12, draw pass 0 =
  base+7, draw pass 1 = base+10.

## Flow

1. `state+0x2a30 = DAT_00474c7c` (stage selector).
2. `0x00403850` (native EBX = state register ABI, stack = the ECX name) —
   the already-reconstructed `LoadTitleBackgroundScriptEbxStackAbi`. A
   nonzero return runs the failure path: `0x0044b8e0` with the Shift-JIS
   string at 0x46cc70 ("ステージデータが読み込めません。データが壊れています\r\n")
   and EDI = the 0x474f70 text context, then `return -1`
   (semantic boundary `AppendStageDataLoadError`).
3. Camera snapshot at `+0x2a4c`: `rep movsd` of 0x46 dwords from the startup
   camera block DAT_00491d7c (`g_AsciiCameraWork`, one 0x118-byte
   `MainChainCameraWork`), then the title camera pose is written over the
   copy (raw dword stores in the native):
   - translation (+0x00/+0x04/+0x08) = (0, 0, -600.0)
   - eye (+0x0c/+0x10/+0x14) = (0, 300.0, 600.0)
   - +0x18/+0x1c/+0x20 = (0, 1.0, 0) — the up-pointer slot plus the head of
     `unknown_001c`, written as one vec3
   - target (+0x3c/+0x40/+0x44) = (0, 0, 0)
4. `state+0x1ee0 = 0x4b12a310` (the background script fade scalar
   9610000.0f).
5. Three scheduler records via 0x00449ed0 (each `record->flags &= ~2`, the
   state stored at record+0x20), registered on DAT_00491be4:

   | state slot | adapter    | target    | chain | priority |
   |---|---|---|---|---|
   | +0x08      | 0x403050 (`mov eax,ecx; jmp`) | 0x402720 | calc | base+12 |
   | +0x0c      | 0x403060 (`push ecx; call`)   | 0x402850 | draw | base+7  |
   | +0x2a40    | 0x403070 (`push ecx; call`)   | 0x402ca0 | draw | base+10 |

   The draw adapters are modeled as real `TH10_FASTCALL` wrappers around the
   semantic `RunTitleScreenDrawPass0/1StackAbi` bodies; the calculation body
   0x00402720 (native EAX = state) remains a boundary
   (`TitleScreenCalcBodyEaxAbi`).
6. `state+0x2a34 = 0` (intro counter).
7. Embedded frame-state timer: the state's +0x24 block uses the same field
   family as the DAT_00474c40 score block, so its +0x14/+0x18/+0x1c/+0x20/
   +0x24 fields land at state+0x38/0x3c/0x40/0x44/0x48. When the +0x48 flag
   bit 0 is clear, the native stores 0 / the NaN sentinel 0xFFF0BDC1 / 0 into
   +0x3c/+0x38/+0x40, the rate pointer `&DAT_00476f78` into +0x44 and sets
   the flag bit — then the unconditional tail overwrites +0x3c/+0x40 with 0
   and +0x38 with 0xFFFFFFFF (also a NaN float), making the branch stores
   dead. Both sequences are preserved verbatim.
8. `state+0x2a18 |= 1` (fade-in active latch); `state+0x94 = 0`;
   `state+0xe0 = 0`; `return 0`.

## Notes

- The `push ecx` at the 0x00403850 call site is not dead: ECX is the
  constructor's second register input (the stage-data name) and 0x00403850
  reads it from the stack (appends it to the 0x497c38 staging buffer before
  the 0x44b360 load).
- `0x0042c670`-style kind 0xf / 0x40000000 VM setup is unrelated here; the
  constructor only registers scheduler records.
- The draw pass bodies are documented in
  `docs/evidence/title-screen-draw-passes.md`; the constructor fixes the
  is-secondary/ABI note found there ("stack = is-secondary flag") into the
  more precise "stack = priority base doubling as the publication selector".
