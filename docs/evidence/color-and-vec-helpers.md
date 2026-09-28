# Color Record Math, Float3 Copies And Micro Helpers (0x401f40..0x412790)

Module: `src/ColorAndVecHelpers.cpp`.

The 6-float color record (Zun color: r g b a r2 g2 at +0x0..+0x14 plus four
byte quantizations of the last four floats at +0x18..+0x1b; quantization is
the __ftol2-style truncate toward zero, modeled as the C cast chain):

- `0x00404d40` `SubtractColorRecord6EsiAbi` — EAX = minuend, ECX =
  subtrahend, ESI = out: `out[i] = a[i] - b[i]` for the six floats, bytes
  re-quantized from the result.
- `0x00404da0` `ScaleColorRecord6StackAbi` — EAX = source, ESI = out,
  stack = scale (ret 4): componentwise scale + re-quantize.
- `0x00404e10` `AddColorRecord6EsiAbi` — EAX = a, ECX = b, ESI = out.
- `0x00405040` `SetColorRecord6StackAbi` — ESI = out, six stack floats
  (ret 0x18); bytes 0..3 quantized from floats 2..5.

Float3 copy family — copies a source triple into a fixed destination offset
of a caller-selected record (EAX = source triple, ECX = record):

- `0x00405000` `CopyFloat3ToOffset24EaxEcxAbi` (record+0x24)
- `0x00405020` `CopyFloat3ToOffset18EaxEcxAbi` (record+0x18)
- `0x00405110` `CopyFloat3ToOffset0cEaxEcxAbi` (record+0xc)
- `0x00405130` `CopyFloat3ToOffset00EaxEcxAbi` (record+0x0)
- `0x0040b370` `CopyFloat3ToOffset340EaxEcxAbi` (ECL script object position
  mirror)
- `0x0040cee0` `CopyFloat3ToOffset24SecondEaxEcxAbi` (bullet record layout
  twin)
- `0x0040cf60` `CopyFloat3ToOffset430EaxEcxAbi` (record+0x430)

Micro helpers:

- `0x004053b0` `SetFloat3StackAbi` — EAX = out, three stack floats (ret 0xc).
- `0x0040c940` `ZeroFloat3EaxAbi` — zeroes the triple.
- `0x00401f40` `SetSentinelTripleEaxAbi` — raw-dword sentinel
  (-999999, 0, 0) used for "no entry".
- `0x00401f90` `SetFirstDwordTripleEaxEcxAbi` — (value, 0, 0).
- `0x0040c920` `SubtractCounterEcxEaxAbi` — EAX = counter, ECX = amount:
  subtract, return the new value.
- `0x00409e10` `ComputeTriplePlusIndexEcxEcxAbi` — ECX = record:
  `record[+0x28] * 3 + record[+0x2c]` index arithmetic.
- `0x00412790` `StoreIndexedDword0x10StrideEaxEcxEdxAbi` — EAX = index,
  ECX = base, EDX = value: stores the dword at
  `base + (index + 0x24a) * 0x10`.
- `0x00401fd0` `WalkStrideSlotsEaxEbxAbi` — ECX = cursor, EAX = count,
  stack = stride, EBX = callback: walks `count` stride-sized slots calling
  the callback (ECX = current slot) on each; negative or zero counts do
  nothing (the `--count; if (count < 0)` guard).

`QuantizeColorRecord6Bytes` (the 0x4050a0 twin) is provided for the
0x405040 contract relation; 0x4050a0 itself belongs to a later batch.
