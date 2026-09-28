# Game Manager State Bodies — Difficulty Lookup (0x4088c0)

Module: `src/GameManagerStateBodies.cpp`.

- `0x004088c0` `GetManagerDifficultyValue` — native __fastcall EDX = the
  difficulty selector; the ECX owner argument is ignored by the body (the
  semantic signature keeps the owner parameter to document the ABI slot).
  Counts how many of the five per-scene difficulty bytes in each of the 22
  rows of the table at 0x4743c0 (110 bytes walked in steps of 5, comparing
  `byte_4743C0[column][row]` for columns 0..4) equal the selector and
  returns the total.

Callers: the stage-select state bodies derive the option page count from
the returned count via `(count + 9) / 10 + 1`
(`RunManagerStateBodyB` and companions in the same module).

## Batch B addition: Stage Flag Byte Table (0x42c8c0)

- `0x0042c8c0` `InitializeStageFlagByteTableEaxAbi` — EAX = stage table.
  Seeds the flag dword row at +0x1d882..+0x1d891 with 0x01010101 (four dword
  stores) and six flag groups at +0x4e9 + 0x437c*g (four rows of six bytes,
  8-byte stride). Reference disassembly:
  `build/reference/0042c8c0_*.asm`. Fired from the case-0 unlock-sequence
  listener documented above.
