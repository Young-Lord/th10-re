# Result Screen Update (0x0042f540 / 0x00430250)

Module: `src/ResultScreenUpdate.cpp/.hpp`. Both functions are the two
callers of `UpdateResultScreenStatDigitsEaxAbi` (0x0042f8b0,
`src/ResultScreenDigits.cpp`); 0x0042f540 is called by the game-manager
dispatcher 0x0042cdf0 and 0x00430250 only by 0x0042f540.

## 0x0042f540 `UpdateResultScreenStateMachineEbxAbi`

- ABI: EBX = game manager / result-screen state, plain `retn`, returns 1 in
  EAX on every path. Switch on the sub-state u32 at `+0x20` (jump table
  0x42f89c, cases 0/1/2/4 + default).
- Case 0: stores 7 into the cursor maximum at `+0x2c`, then seeds the cursor
  value at `+0x24` from it with the native sign-split idiom (`==0 -> 0`,
  `>0 -> 0`, `<0 -> v-1`; always the `>0` arm in practice, kept verbatim),
  spawns the script-2 result-screen entity via 0x42c670
  (`SpawnManagerEntityFromScript`), sets sub-state 1 via 0x42c620
  (`SetGameManagerSubState`), copies the five u16 stat words from
  DAT_00474e88/8c/98 into `+0x59cc..+0x59d4` (word moves only) and calls the
  digit presenter, then **falls through into the case 1 body**.
- Case 1 (shared entry label 0x42f5dc): when the signed `+0x2b4` timer is
  `<= 6` return 1; otherwise set sub-state 2, run the `+0x2cc` entity with
  stop word 3 (0x449250 `SetEntityStopWordByIdAndRun`) and queue the slot-2
  stop word `cursor + 17` (0x42c770).
- Case 2: cursor record is the state itself at `+0x24` (`+0x00` value,
  `+0x04` previous, `+0x08` maximum). Stores the value into `+0x28`, shifts
  by -1/+1 via 0x44bea0 (`ShiftManagerSelector`) when the 0x10/0x20 masks
  are set in the DAT_00474e36 low byte or the DAT_00474e34 gate byte, and on
  a change reserves boundary channel 0xc (0x43dc90), runs the entity with
  stop word 3 and queues slot-2 stop word `cursor + 7`.
  - Joystick pick: calls 0x44a4e0 (`PollJoystickButtonBytesEcxAbi`, ECX=0)
    returning the DAT_00497bb0 button bank, then scans indices **0..30 only**
    (the native loop caps at `edx == 0x1f`) for the first byte negative as
    signed (0x80 pressed bit). On a press with cursor `<= 4` it calls
    0x00430250 with EAX=state, **EDX = pressed button index, ECX = cursor
    value** (deterministic: ECX is reloaded from `[ebp+0]` at 0x42f6ce).
  - Page 6 accept (DAT_00474e36 & 0xa, cursor == 6): copy the stat globals
    into the fields, refresh the digits, then the shared tail 0x42f7d9:
    reserve channel 0xb, slot-2 stop word 6, sub-state 4, return 1.
  - DAT_00474e36 & 0x1001: page 5 (`cursor-5 == 0`) copies the globals into
    the fields, refreshes the digits and reserves channel 0xa; page 6
    (`== 1`) writes the fields back to DAT_00474e88/8c/98 as words, then
    snapshots the bank dwords DAT_00474e88/8c/90/94 and the word
    DAT_00474e98 into DAT_00491d4c..191d5c, then runs the shared tail.
- Case 4: when the `+0x2b4` timer reaches 10, set state 4 (0x42c5c0
  `SetGameManagerState`) and finalize the cursor record (0x44be70
  `Call44BE70`).
- Default: return 1.

## 0x00430250 `SetResultScreenStatFieldEaxEdxEcxAbi`

- ABI: EAX = state, EDX = new value, ECX = field index (0..4); plain
  `retn`; returns the state pointer in EAX unchanged.
- Reads the raw u16 at `state + 0x59cc + 2*idx`; if it equals `value` as a
  **signed 16-bit** comparison the function returns early without side
  effects.
- Otherwise, for each of the five fields other than `idx` whose signed
  16-bit value equals `value`, store the edited field's previous raw u16
  (native re-loads `[eax+ecx*2+0x59cc]` per iteration; the edited field is
  not modified inside the loop, so the decompiler's first-iteration
  special case is equivalent).
- Stores `value` (u16) into the field, calls the digit presenter 0x0042f8b0
  and reserves boundary channel 0xa with arg 0 (0x43dc90).

## Boundaries and globals

- `PollJoystickButtonBytesEcxAbi` (0x0044a4e0): unreconstructed winmm /
  DirectInput button poll filling DAT_00497bb0; link boundary.
- Everything else is a semantic body: `SpawnManagerEntityFromScript`,
  `SetGameManagerSubState`, `SetGameManagerState`,
  `SetEntityStopWordByIdAndRun`, `SetManagerSlotEntityStopWord`,
  `ShiftManagerSelector`, `Call44BE70`, `ReserveContextChannel`
  (`src/GameManagerState.cpp`), and the digit presenter.
- Globals: `g_MainChainInputBindings` (DAT_00474e88, byte-addressed to
  cover the u16 words at +0x00/+0x02/+0x04/+0x06/+0x10), `g_MenuInputFlagsByte`
  (DAT_00474e34), `g_ManagerSubGateFlags` (DAT_00474e36), and
  `g_ResultStatusSnapshot` (DAT_00491d4c, 17 bytes).

## Quirks preserved

- Case 0 falls through into the case 1 body.
- The sign-split cursor-seed idiom and the 31-byte (0..30) button scan cap.
- Signed 16-bit field comparisons with raw u16 propagation in the setter.
- All stores/loads are word/dword-faithful to the native moves; the
  DAT_00491d4c snapshot reads the bank dwords after the word writes exactly
  as the binary does.
