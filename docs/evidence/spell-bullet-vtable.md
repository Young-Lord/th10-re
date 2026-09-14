# Spell/Bullet Base Vtable Neighborhood

Implemented in `src/SpellBulletVtable.cpp/.hpp`. The five entries belong to
the spell/bullet base object published at `DAT_004776f4`
(`g_SpellBulletBase`) — the story-mode spell-capture state machine whose
in-place destructor `0x00408af0` (`DestroySpellBulletBaseInPlace`) already
lives in `src/GameModeTeardown.cpp`. The sibling draw entries `0x004091c0`
(plain) / `0x00409270` (`push ebx; mov ebx,ecx; call 0x4091c0`) and the
update thunk `0x00409220` (`push edi; mov edi,ecx; call 0x00408d60`) remain
thunks and are not reconstructed here.

Note: the earlier `docs/coverage-gap.csv` classification "vtable-referenced
only, no code xrefs" is stale — all five entries are reached by direct
`call rel32` sites (0x408210 ← 0x410d19/0x410de8, 0x408d60 ← thunk
0x409220, 0x409280 ← 0x410f3a, 0x409c00 ← 0x410f5b); only 0x4084a0 has no
remaining reference in the binary.

Object layouts used below:

- Spell/bullet base: two ASCII VMs at `+0x10` / `+0x3bc`, eight VMs at
  `+0x778` and `+0xa7c`, five VMs at `+0x24d8` (tally cluster
  `+0x24d8/+0x2884/+0x2c30/+0x2fdc/+0x3388`), entity handles at
  `+0x768/+0x76c/+0x770/+0x774`, the scaled timer block `{prev +0x3734,
  count +0x3738, accum +0x373c, rate ptr +0x3740}`, first-run flag
  `+0x3744`, spell name at `+0x3748`, card index `+0x3788`, flag word
  `+0x378c`, bonus pair `+0x3790/+0x3794`, decay parameter `+0x3798` and
  the followed vec3 `+0x379c`.
- Bullet records: 2000 records of 0x7f0 bytes at `DAT_004776f0` manager
  `+0x60`; state word `+0x446` (0 and 3 = skip), position `+0x3b4/+0x3b8`,
  descriptor pointer `+0x39c` (size float at `descriptor+0x34`), delete
  bookkeeping dword `+0x450`, half-extent pair `+0x3f0/+0x3f4`, signed
  difficulty word `+0x7ec`, flag dword `+0` (bit 3 = deleted).
- Card slot in the score-save record (`DAT_0047783c`):
  `(shot + 3*character) * 0x437c + index * 0x90`, name buffer at
  `+0x5a4` / `+0x19a8c` (all-difficulty twin), counters at name `+0x84`
  (practice) and `+0x80` (captures), both capped at 99999 (0x1869f).

## TH10 0x00408210 `ClearStageRegionBulletsEbxStackAbi`

Stdcall `ret 4`; native EBX = the bullet manager (the `DAT_004776f0`
effect manager root), stack argument = explosion flag. Returns 0.

1. Region bounds from the manager: centers `+0x44/+0x48/+0x4c`, half
   sizes `+0x50*0.5/+0x54*0.5/+0x58*0.5` (`flt_470b0c`). The x axis pairs
   with the `+0x44` bound, the y axis with `+0x48`; the third bound is
   only forwarded to the effect spawn.
2. Over the 2000 records: skip when the `+0x446` state word is 0 or 3.
   Delete when the record box (`x ± +0x3f0*0.5`, `y ± +0x3f4*0.5`)
   overlaps the region box. The four native comparisons translate to
   plain C++ `<`/`>` (keep on strict less-than / strict greater-than), so
   NaN operands take the delete path exactly as native.
3. Deletion: record flag `+0` bit 3 set; with a nonzero stack argument
   the explosion `0x41bb00` fires (manager `DAT_00477818`, position
   `+0x3b4`, kind 8, color `0xffffffff`, angle `-1.25f`
   (`0xbfc90fdb`), speed `0.8f` (`0x3f19999a`)).
4. `0x448db0` (`SpawnStageEffectEdxEbxAbi`, ECX = `[manager+0x3e0b50]`,
   EDI = bullet position, stack = `{context, script 0x173}`): the
   `{max_x_bound, max_y_bound, max_z_bound}` trio is passed as both the
   effect position and the out-id slot (the id lands in the first dword).
5. `0x4491c0` resolves the id; with the descriptor present, the color
   picked by the descriptor size (`<= 16` → 16-entry table `0x474350`,
   `<= 32` → 8-entry `0x474390`, `> 32` or NaN → 4-entry `0x4743b0`,
   indexed by the signed `+0x7ec` word) is stored at `entity+0x2fc`.
   Unchecked-pointer quirk: a null entity still receives the store
   (write to absolute `0x2fc`).
6. Record `+0x450` cleared.

## TH10 0x004084a0 `SumBulletCornerItemValueEcxEdiStackAbi`

Native ECX = bullet manager, EDI = `{x, y}` point, one stack argument =
radius (squared on entry: `fld [esp+4]; fmul [esp+4]`). `ret 4`. No
direct callers remain in the binary. Returns the summed item value.

- The ECX cursor starts at manager `+0x3f0` (record `+0x3f0`) and walks
  2000 records of 0x7f0; the `+0x446` state word gates exactly as in
  0x408210.
- Four corner circles of radius² = argument² centered at the record's
  AABB corners: the native computes each corner from distinct float
  expressions (`min + extent` versus `half + center` for the max y, and
  `max - extent` for the min y), so the reconstruction reproduces those
  exact expressions. A hit is "sum² not greater than radius²" (unordered
  counts as a hit), hence the negated `>` test.
- On a hit with a descriptor present, the descriptor size (`+0x34`)
  selects the value: `<= 8` → +1, `<= 16` → +1, `<= 32` → +4,
  `<= 64` → +10, larger or NaN → +0 (the parity-tested chain
  `fcomp 8/16/32/64` treats unordered as greater-than).

## TH10 0x00408d60 `UpdateSpellCardStoryStateEcxAbi`

Runs while `base+0x378c` bit 0 is set (otherwise returns 1 immediately;
every return path yields 1). Native thunk `0x409220` moves its ECX into
EDI.

1. Count `+0x3738 >= 60`: `[DAT_004776e8+0x2a18] &= ~1` (spell banner
   flag cleared).
2. Count `>= 300` and flag bit 3 clear: the running bonus `+0x3790`
   decays by `(max*9/10) / ([+0x3798] - 300)` per step (magic division
   `0x66666667 >> 34` = signed /10), rounded down to a multiple of ten.
   A `+0x3798` value of exactly 300 divides by zero (native quirk,
   preserved).
3. `0x43ee30` (`FinalizeTimelineRenderObjectSetup`) ticks the VM pair at
   `+0x10` and `+0x3bc`.
4. Scaled timer block: `prev +0x3734 = count +0x3738`; rate is the
   single-dereferenced float at `*[+0x3740]` (seeded to `flt_476f78` =
   1.0 by 0x409280). Unity window `0.99 < rate < 1.01` (`flt_470b68` /
   `flt_470b64`) increments count and accumulator by 1.0; otherwise the
   accumulator absorbs the rate and the count re-derives through
   `__ftol2` (0x463b2c, modeled as truncation per the established
   convention).
5. Count `>= 120` (0x78): two-phase defeat machine against the player Y
   at `DAT_00477834+0x3c4`:
   - bit 2 clear (phase A): any ordered comparison against 96.0
     (`flt_470c80`) proceeds (only NaN is rejected). The three handles
     `+0x768/+0x76c/+0x770` get state word 3 through `0x449470`, the
     eight VMs at `+0xa7c` and five VMs at `+0x27dc` get u16 word 3, and
     bit 2 latches.
   - bit 2 set (phase B): requires player Y strictly `> 128.0`
     (`flt_470bf4`). Same writes with word 2, bit 2 cleared.
6. Flag bit 1 (capture tally running): the eight VMs at `+0x778` are
   re-armed through `0x43e5a0` with entry `quotient + 0x1e` and ticked
   (`0x43ee30`) one by one; the divisor chain is 10,000,000, then 1 for
   every later iteration, so VM 0 carries `bonus/1e7`, VM 1 (and 2)
   `bonus%1e7`, and VMs 3..7 land on entry `0x1e`. Quirk preserved.
7. Per-card digits: fields `+0x624` and `+0x628` of the current card slot
   (clamped at 99, signed) drive `+0x24d8`/`+0x2884` (tens/ones of
   `+0x624`) and `+0x2fdc`/`+0x3388` (tens/ones of `+0x628`); then all
   five tally VMs are ticked (`+0x2c30` is only ever ticked).
8. The battle record's `+0x1068` vec3 (via `DAT_00477704+0x10`,
   dereferenced without a null check) is followed at 5% per frame
   (`flt_470c84`) into `+0x379c`, which is published for the `+0x774`
   entity through `0x449350`.

## TH10 0x00409280 `StartSpellCardPracticeEaxStackAbi`

Stdcall `ret 0x10`; native EAX = base, stack = `{base, spell card index,
spell name pointer, payload id}`. The caller (the spell-practice start
handler at 0x410f06..0x410f3f) decrypts the name, resolves the card ids
through `0x44fdb0` (0/1/2) and passes the practice-table index plus the
difficult-scaled id in ESI. The EAX return value is dead (the caller
clears it).

1. First-run timer init when `+0x3744` bit 0 is clear: `+0x3738 = 0`,
   `+0x3734 = 0xfff0bdc1` (poison float), `+0x373c = 0`,
   `+0x3740 = 0x476f78`, `+0x3744 |= 1`. Afterwards — unconditionally —
   `+0x3738 = 0`, `+0x373c = 0`, `+0x3734 = -1` (the init's `+0x3734`
   value is immediately overwritten; quirk preserved), `+0x3788 = card
   index`, name copied to `+0x3748`, and flags `+0x378c` updated to
   `(flags & ~0x18) | 3`.
2. With game mode (`DAT_00477838+0x10`) != 1: the name is registered in
   the score-save record at card offset `+0x5a4` and `+0x19a8c`, each
   followed by a counter at name `+0x84` incremented while below 99999.
3. The thirteen ASCII VMs are rebound through `0x43e710` (resource
   `DAT_0047770c+0x9ec8`): eight at `+0x778` with scripts `0x3a..0x41`,
   five at `+0x24d8` with `0x42..0x46`.
4. Five pool spawns (helper described in the source): `0x449950` alloc
   from the `DAT_00491c10` pool, flag `0x40000000`, `+0x20 = 0xf`,
   `0x449870` script bind, `0x4489d0` list-A tail link with the wrapping
   id counter (`+0x732454`). Handles: `+0x768` (script 1, resource
   `DAT_004776e0+0x8994`), `+0x76c` (script 0x48, resource `+0x899c`,
   validated right after the spawn — cleared when unresolvable),
   `+0x770` (script 2, resource `+0x8994`), `+0x774` (script 0x1a1,
   resource `DAT_004776f0+0x3e0b50`). The fifth spawn (script 0x1ab,
   same stage resource) does not publish a handle; the native instead
   writes its id over the incoming third stack-argument slot, which
   `ret 0x10` discards — modeled with a local out-id (documented).
5. The resolved `+0x76c` entity feeds the tip-text call `0x447ae0`
   (native EAX = ESI = entity or null, stack `{0x491c10 manager,
   0xffffff color, spell name}`), then sound `0x0e` through `0x43dc90`
   (`ECX = 0x492590`).
6. The `+0x774` entity is seeded with the battle record's `+0x1068` vec3
   (via `DAT_00477704+0x10`, unchecked) and published through `0x449350`,
   then validated like `+0x76c`.
7. The payload id is stored at `entity+0x314` for the script-word
   `0x19f` and `0x1a0` entities (list walk matching `+0x38a`) and at
   `+0x3798`. Unchecked-pointer quirk: a miss stores through the null
   pointer (absolute `0x314`).
8. Bonus seed: `+0x3790 = (+0x3794 =) (rank*3 + 10) * score(0x474c4c) *
   10`, with `+0x3794` capped at 99,999,999 (signed compare).
9. Rank dispatch on `DAT_00474c7c - 1` (jump table `0x409be4`, bounds
   0..6; out-of-range falls straight to the epilogue). Each rank
   initializes the base VM pair through `0x404f30` (stack =
   `DAT_00477704+0x38`) and spawns its rank entity:

   | rank | VM0 | VM1 | spawn script |
   |------|-----|-----|--------------|
   | 0    | 0xc | 0xb | card index < 2 ? 0xe : 0xf |
   | 1    | 0xe | 0xf | 0x11 |
   | 2    | 0x12| 0x13| 0x15 |
   | 3    | 0x13| 0x14| 0x16 |
   | 4    | 0xc | 0xd | 0xf  |
   | 5    | 0x21| 0x22| 0x24 |
   | 6    | 0x1b| 0x1c| 0x1e (when `DAT_00474c84 >= 24`); else VM0 only (0x26) with spawn 0x28 |

   The rank-6 low branch skips the second VM entirely (native quirk,
   preserved).

## TH10 0x00409c00 `FinishSpellCardPracticeEaxAbi`

Native EAX = base (the caller at 0x410f56 loads `DAT_004776f4`; ECX and
the stack hold nothing the body reads — `push ecx` is only a scratch
slot).

1. Gate: `+0x378c` bit 0 (capture running) must be set.
2. `[DAT_004776e8+0x2a18] |= 1` (banner flag), then the three handles
   `+0x768/+0x76c/+0x770` are expired through `0x409e50`
   (`ExpireEntityHandleEaxAbi`), bit 0 cleared, the `+0x774` entity
   soft-released (`0x4492a0`) and the handle cleared.
3. Captured (bit 1 set): `DAT_00474c44 += bonus(+0x3790) / 10` (magic
   signed division) with the 999,999,999 clamp (`0x3b9ac9ff`);
   `0x4172e0` (mode 0, stack `{DAT_0047770c, bonus}`) runs the
   result-screen state; with game mode != 1 the card slot capture
   counters (`+0x80` of both the difficulty slot and the `+0x19a8c`
   twin) increment below 99999; sound `0x2d` through `0x43dc90`.
4. Not captured: `DAT_0047770c+0x9e14`'s entity is soft-released, the
   slot cleared, and `0x448d00` respawns a setup VM (dead first stack
   argument, script 0x48, kind 0xf) whose first dword (the id) is stored
   into the slot. Unchecked dereference of the returned record (native
   quirk, preserved).

## Status

`g++ -m32 -std=c++98 -fsyntax-only -I src src/*.cpp` passes (the full
project). `src/SpellBulletVtable.cpp` is not yet listed in
`scripts/compile-main-chain-cpp.sh` (per instructions the script was not
touched). CSV rows appended for 0x00408210, 0x004084a0, 0x00408d60,
0x00409280 and 0x00409c00.
