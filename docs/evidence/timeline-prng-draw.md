# Timeline PRNG draws (DAT_004918b0 LCG-B)

Covers TH10 0x0044b9b0, 0x0044b9e0, 0x0044ba80, 0x0044bb20, 0x0044bb90 —
module `src/TimelinePrngDraw.{hpp,cpp}`. These are the standalone native
accessors over the LCG-B state object reconstructed as
`g_TimelinePrngStateB` in TimelineRenderObjectSetup.cpp (state word at
+0, draw counter at +4). The ECL VM (0x44e1a0) inlines the same
recurrence with a duplicated-half quirk; the standalone entries do not.

## Half-step recurrence

    x    = (state32 ^ 0x9630) - 0x6553
    next = x * 4 + ((u16)x >> 14)

The shift term is always computed on the low 16 bits (`mov dx, ax` /
`shr dx, 14`), so only the low half of the state feeds the seed sequence;
the high garbage carried through the 32-bit chain between steps is
observable nowhere. Only the low 16 bits of a step are stored back.

## Entries

* 0x0044b9b0 `TimelinePrngAdvanceSingleEcxEaxAbi` — one half-step, counter
  +1, returns the full 32-bit step value (native EAX keeps the untruncated
  chain value).
* 0x0044b9e0 `TimelinePrngDrawPairEcxEaxAbi` — two half-steps, counter +2,
  returns `(h1 << 16) | h2`. The native stores h1 into the seed slot
  mid-way and then overwrites it with h2; only the final seed and the
  packed return are observable. (Contrast the EclScriptVm inlined copy
  `LcgDrawRaw32Duplicated`, which returns `(h2 << 16) | h2`.)
* 0x0044ba80 `TimelinePrngDrawQuadPackedEcxEaxAbi` — four half-steps,
  counter +4, seed lands on h4, returns `(h1 << 16) | h2` (the first two
  draws packed). No direct xrefs; the retail callers all use the inlined
  or pair forms.
* 0x0044bb20 `TimelinePrngDrawUnitDoubleEcxEfiAbi` — pair draw converted
  with `fild` (signed), `fadd 2^32` when negative (DAT_00470b98), then
  `fmul 2^-32` (DAT_00470bf0): a [0, 1) double in ST0. 10 call sites
  (SpawnSceneTriggerFromDescriptor, ReadVmFloatRegister, ribbon ring
  buffer rebuild, ...).
* 0x0044bb90 `TimelinePrngDrawSignedDoubleEcxEfiAbi` — same conversion
  scaled by 2^-31 (DAT_00470bec) minus 1.0 (DAT_00470afc): a [-1, 1)
  double. 17 call sites (SpawnEnemyDeathScatter, ReadVmFloatRegister,
  ECL bullet motion, ...).

The float powers of two are exact, so the C99 `value / 4294967296.0`
shapes reproduce the x87 results bit for bit.

## Verification

Reference disassembly `build/reference/0044b9b0_sub_44B9B0.asm` through
`0044bb90_sub_44BB90.asm`; constants read from 0x470afc/0x470b98/
0x470bec/0x470bf0 (0x3f800000, 0x4f800000, 0x30000000, 0x2f800000).
