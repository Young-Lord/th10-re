# Result-Screen Stat Digits (TH10 0x0042f8b0)

Implemented as `UpdateResultScreenStatDigitsEaxAbi` in
`src/ResultScreenDigits.cpp/.hpp`.

## ABI

- Native entry: EAX = result-screen state object, ECX = dead register
  input. Returns the last glyph VM init result (or the last resolved
  child pointer when the child was missing) in EAX.

## Semantics

The state object carries a parent-entity id slot at `+0x2cc` and five
signed 16-bit stat fields at `+0x59cc`, `+0x59ce`, `+0x59d0`, `+0x59d2`,
`+0x59d4`. For each of twenty blocks (child kinds 67..86 in native
order) the function:

1. Resolves the parent entity via `FindEntityEdxStackAbi` (0x4491c0,
   manager = `DAT_00491c10`); when the lookup fails it clears the
   `+0x2cc` slot. (The native keeps walking the child chain even for a
   null parent — the walk then starts at node 0x10; preserved.)
2. Walks the parent's inline child chain at `+0x10` (nodes
   `{child, next}`) looking for the block's u16 kind at child `+0x38a`;
   the child id is `child[0]`.
3. Resolves the child via `FindEntityEdxStackAbi` (no slot write on
   failure) and, when found, re-runs
   `InitializeAsciiAnimationVmEntry` (0x43e5a0) on the child record with
   entry index `digit + 51` and the resource pointer at child `+0x308`.

Digit mapping: kinds {67,68}/{77,78} show field `+0x59cc` tens/units,
{69,70}/{79,80} show `+0x59ce`, {71,72}/{81,82} show `+0x59d0`,
{73,74}/{83,84} show `+0x59d2`, {75,76}/{85,86} show `+0x59d4` — i.e.
kinds 77..86 repeat the same five digits in a second display column.
The tens digit is the signed /10 (native magic-division, truncating),
units the signed %10; both offset by +0x33 (51).

## Callers

`0x42f540` and `0x430250` (result-screen update paths, not yet
reconstructed).

## Status

Baselines pass (`scripts/compile-main-chain-cpp.sh`, g++ -m32 -std=c++98
syntax check). IDA name/comment applied at 0x42f8b0.
