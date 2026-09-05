# ECL Script Library (TH10 0x0040cfb0)

Implemented as `CreateEclScriptObjectEaxStackAbi` in
`src/EclScriptLibrary.cpp/.hpp`.

## Context

The caller chain enumerates `../../data/*.ecl` (`0x40a450`, itself called
from `0x40abe0`) and builds one 0x2518-byte ECL script object per file.
The object is a C++ record whose vtable `0x46d0c0` is installed by the
constructor `0x40d830` (native userpurge: ESI = record, one stack arg).

## ABI

- Native entry: EAX = source descriptor, two stack args (`ret 8`):
  list owner and ctor argument. Returns the record in EAX.
- Descriptor layout: dwords 0..2 -> record+0x1094..0x109c; dword 3 ->
  +0x23f8; dword 5 -> +0x23fc; dword 4 -> +0x2408; bit 0 of dword 6 ->
  +0x2480 bit 0x800 (replace); bit 0 of dword 7 -> +0x2480 bit 0x40000
  (replace); 0x20 bytes at descriptor+0x20 -> record+0x1138.
- Failed `operator new` leaves the record null and the native continues
  writing through the null pointer; the reconstruction preserves this
  unchecked behavior.

## Record initialization order (verified against disassembly)

1. Constructor via 0x40d830 (boundary `ConstructEclScriptObjectEsiStackAbi`).
2. Descriptor copies and the 0x800 flag replacement (see ABI above).
3. `record+0x1024` byte = `1 << dword_474c74` (difficulty bitmask).
4. 0x20-byte name copy from descriptor+0x20.
5. Score-anim block at +0x2458..+0x2468: first-time init
   (`+0x245c=0`, `+0x2458=-999999`, `+0x2460=0`, `+0x2464=&flt_476F78`,
   `+0x2468 |= 1`) gated on flag bit 0, then unconditional arm
   (`+0x245c=2`, `+0x2460=0x40000000`, `+0x2458=1`).
6. The 0x40000 flag replacement.
7. `RunEclScriptSetupStackAbi(record+0x1044)` — boundary for 0x40dc80
   (stack arg, ret 4; early-outs when `[arg+0x1444]` bit 0x400 is set).
8. Flag bit 0x8000 remaps the +0x2408 kind: 1 -> 10, 4 -> 11.
9. `record+0x2444 = (owner+0x64 & 1) + 2`; `record+0x2448 = 359` unless
   `record+0x1128 == 1`, in which case the switch over `record+0x112c`
   selects 359 / 356 / 362 / 365 for the kind groups
   {0,0x14,0x31} / {5,0x19,0x32} / {0xA,0x1E,0x33} / {0xF,0x23,0x34};
   `record+0x244c = 0`.
10. List append: node at record+0x116c (next +4, prev +8) spliced after
    the owner's last node (owner+0x58 head, +0x5c last, +0x60/+0x64 two
    counters, both incremented). The `last->next` fix-up only runs when
    the previous last node's next is non-null (native quirk).

## Callers

`0x40a450` (ECL file enumeration), the `0x40e5xx` menu region, and
`0x418190` (game-manager state body 2).

## Status

Baselines pass (`scripts/compile-main-chain-cpp.sh`, g++ -m32 -std=c++98
syntax check). IDA names/comments applied at 0x40cfb0.
