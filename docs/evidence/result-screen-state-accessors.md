# Result Screen State Accessors (`0x00415db0` family)

Batch B reconstruction evidence for the result-screen script state creation
and the flag/interpolator accessors around it. Bodies live in
`src/ResultScreenStateAccessors.cpp`.

| Address | Name | Contract |
|---------|------|----------|
| 0x00415db0 | `CreateResultScreenScriptSlotEdiEsiAbi` | EDI = main chain context, ESI = slot. Allocates a 0x90 result-screen script state, initializes it against the script blob at `[ctx+0x9ebc] + [base+8*slot+4]`, publishes it at ctx+0x9eb8, stamps the slot at state+0, advances the result-slot counter (`DAT_00474c84` = slot+1) and clears `DAT_00474c8c` when the counter actually moved. Returns slot+1. The native stores the slot stamp unconditionally, so a failed allocation would null-deref (operator new aborts in practice). |
| 0x004175b0 | `ReadEntityFlagBit2EaxAbi` | Flag bit 2 read of record +0x60. |
| 0x00417600 | `ReadEntityFlagBit4EaxAbi` | Flag bit 4 read of record +0x60. |
| 0x00417690 | `ReadEntityFlagBit5EaxAbi` | Flag bit 5 read of record +0x60. |
| 0x00417710 | `ReadManagerFlagBit3At2A18EaxAbi` | Flag bit 3 read of main chain context +0x2a18. |
| 0x004177c0 | `MarkResultPairFlagsThiscallA` | ECX = owner. Marks flag bit 1 (0x2) on the two records at owner+8 / owner+0xc (no null checks; the records are always present for live owners). |
| 0x004177e0 | `MarkResultPairFlagsThiscallB` | Identical twin of 0x004177c0. |
| 0x00417800 | `RearmInterpTimerAt40EaxAbi` | Re-arms the animation timer at +0x40 (flags at +0x50, rate pointer at +0x4c), then zeroes +0x58/+0x5c and returns 0. |

## Verification

Reference disassembly: `build/reference/00415db0_*.asm` ..
`build/reference/00417800_*.asm`. The twins 0x004177c0/0x004177e0 were
byte-compared identical in the reference dumps.
