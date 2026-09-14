# Unlock List Refresh — 0x00432690

Module: `src/UnlockListRefresh.cpp/.hpp` (implemented as
`RefreshStateBSelection`, the name already used by the three call sites
in `RunManagerStateBodyB`, TH10 0x00431ee0 — see
`docs/evidence/score-screen-update-dispatch.md`).

## ABI

Native ECX = game manager; returns 0 (`xor eax,eax` after the /GS cookie
check). The C++ semantic entry models ECX as the first argument and
drops the return value (unused at all three call sites 0x4321ba /
0x4322e5 / 0x432369).

## Manager fields

| Offset | Meaning |
| ------ | ------- |
| `+0x24` | difficulty selector (multiplied by 0x437c for the save block) |
| `+0xfc` | category value matched against the 0x4743c0 byte table |
| `+0x1d4` | category count N (scan target = `10*N - 10`) |
| `+0x2ac` | per-refresh processed-row counter (reset to 0, incremented per row) |
| `+0x5d4..` | ten entity handle slots, one per visible row |

## Behavior

1. Skip the first `10*N - 10` matches of the signed bytes at `0x4743c0`
   equal to `mgr+0xfc`. The scan never bounds-checks the 110-byte table
   (native quirk preserved); when `10*N-10 <= 0` the index starts at 0.
2. Row loop (at most 10 rows): advance the index to the next table match
   (index capped at 0x6e = 110).
   - Table exhausted: every remaining row is blanked — each row's
     `+0x5d4` handle is resolved through the render owner's two entity
     lists (`owner+0x72dad4` then `owner+0x72dadc`, nodes `{entity,
     next}` with the id at entity+0), zeroed when unresolvable, and a
     single-space text entity (`0x46cfd4`) is drawn in white through
     `0x447bb0` with the resolved entity in ESI.
   - Matched row: the 0x90-stride save entry at
     `save + 0x19a8c + index*0x90` (name) and `save + 0x19b10 +
     index*0x90` (entry dword, save = `DAT_0047783c`). When the dword is
     non-zero the name is copied and padded to 42 characters (unbounded
     native copy then `strlen` + space fill, NUL at [42]).
3. The row handle is re-resolved exactly like the blank path; stale
   handles are zeroed.
4. Populated rows draw `"No.%3d %s %4d/%4d"` (0x46eef8) with the
   1-based table index, the padded name and the two dwords at
   `save + 8 + (mgr+0x24)*0x437c + index*0x90 + 0x61c / +0x620`; the row
   color is the branchless chain `neg/sbb/and 0x100f91/add 0xefefef` on
   the +0x61c dword: non-zero → `0xffff80`, zero → `0xefefef`.
   Empty rows draw `"No.%3d "` + 20 SJIS 0x8148 dashes +
   `" %4d/%4d"` (0x46eebc) in `0x808080`.
5. `mgr+0x2ac` counts the processed rows; index/handle-slot/row all
   advance together.

## Boundaries kept

`0x00447bb0` (text-entity submission; native ESI carries the resolved
row entity, stack = (owner `0x491c10`, color, format, ...)) — declared
as `AddFormattedTextEntityEsiStackAbi`; `0x004491c0`
(`FindEntityEdxStackAbi`, which walks the same two owner lists the
native body walks inline).

## Verification

`g++ -m32 -std=c++98 -fsyntax-only -I src src/*.cpp` — 0 errors.
