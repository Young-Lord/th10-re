# ASCII HUD Gameplay Update (`0x00414900`)

## Call Chain And ABI

- `0x00414900` (3805 bytes, 308 blocks) is the real body of the gameplay-side
  ASCII HUD update. Its only code xref is the 7-byte thunk `0x00415AE0`
  (`push ecx; call 0x414900; retn`), whose address is pushed as a task callback
  by the `front.anm` loader `0x00413980` (called from `CreateAsciiHudOwner`,
  `0x00414830`). There it is registered through `0x449AE0` with callback id
  `0x18` on the owner record at `DAT_0047770C` (`[esi+20h] = ebx` owner, id
  stored via the scheduler); the sibling callback `0x00415AF0` (id `0x2B`,
  body `0x00415800`, the HUD batch renderer) is registered in the same
  sequence.
- Native entry is stdcall ret 4: one stack argument, the `0x9ed0`-byte HUD
  owner. The reconstruction `UpdateAsciiHudGameplayStackAbi` in
  `src/AsciiHudGameplayUpdate.cpp` keeps the same signature and always
  returns 1, matching the native `mov eax, 1` epilogue.

## Verified Behavior (against disassembly)

1. **Render-mode latch.** If `+0x9eb4` has bit `0x10`, `+0x9ecc` counts up
   (native `mov ecx,[edi+9ECCh]` / `inc`), and at 120 the HUD render mode
   `dword_491FB8` becomes `0` when `dword_491FF4 & 0x1000`, else `14`.
2. **VM pools.** Three `FinalizeTimelineRenderObjectSetup` sweeps:
   9 records at `+0x4980`, 4 at `+0x6a8c`, 2 at `+0x793c`, stride `0x3ac`
   (native `mov ebx, 9 / 4 / 2`).
3. **Region state words.** When `dword_477834` (screen target block) is set,
   its `+0x3c0/+0x3c4` x/y gate the seven state words at `+0x8398`
   (`mov ecx, 7; mov word ptr [eax], 3/2`): entering (`y > 432` and
   `x < -128`, NaN comparisons cull) sets words to 3 and latches bit 0 of
   `+0x9eb4`; leaving (`y < 400` or `x > -112`) sets 2 and clears bit 0.
4. **Score digits.** `+0x8094` finalized, then five digit VMs at
   `+0x8440 + i*0x3ac` re-initialized with glyph `digit + 0x1e` from the
   `front.anm` resource at `+0x9ec8`, divisor starting `0x2710`
   (`mov esi, 2710h; mov ebx, 5`), then `+0x969c` finalized.
5. **Boss battle block** (gated on `dword_477704` and its `+0x10` battle
   record, and `+0x9eb8 == 0`):
   - HP: `battle+0x23fc` copied to `+0x9e8c`; `+0x9e84` rises by `0.025`
     (`flt_470cf8`) toward `hp/hp_max` (`battle+0x2400`) and is clamped down.
   - Bench region: bit 3 of `+0x9eb4` toggles via the target block
     `y < 80 / y <= 64 && x < -64` tests; state words 3 (inside, also
     propagating `+0x304` to child chains, native `0x414dbf`) and 2
     (outside) are applied to `+0x9e24` and the `+0x9e28` slots.
   - Spawn: when `+0x9e24` is free, the stage script switch
     (native `0x414e02`, jump table with 7 cases, `ebp = 0x85` default)
     selects `0x85..0x8d` using `dword_474C7C` (stage, 1..7) and
     `dword_474C84 >= 0x18` (progress). Cases 2 and 3 with progress below
     `0x18` jump to `loc_414F33`, which is the fill-loop entry — the spawn is
     skipped but the fill loop still runs. The spawn (`0x414e91`) is
     `AllocatePoolVmEsiAbi` (0x449950, ECX = `+0x9ec8` resource), `+0x35c |=
     0x40000000`, `+0x20 = 0xf`, script bind (`0x449870`), list-A back append
     into the `0x491C10 + 0x72dad4/0x72dad8` chain, unique id counter at
     `0x732454` (wraps 0 by incrementing again), id stored to `vm[0]` and
     `+0x9e24`.
   - Fill loop (`0x414f33`): ten `+0x9e28` slots, bounded by `+0x9e90`;
     empty slots below the bound spawn scripts `0x5b + i`
     (`lea ecx, [ebx+5Bh]`), excess non-empty slots get state word 1 and are
     cleared.
   - Boss-gate failure path: `+0x9e24` released with state word 1 and the
     accumulators `+0x9e84/+0x9e94/+0x9e9c/+0x9ea4/+0x9eac` are zeroed.
6. **Result-screen script state** `+0x9eb8`: runs
   `RunResultScreenScriptStreamStackAbi`; on completion the six handle slots
   `ss+0x40..0x54` are killed (`0x4000000` flag on `+0x35c` with the `+0x14`
   child-chain walk unless `+0x18` is set), the state is `j__free`d and
   cleared. While running, the shared scaled-timer epilogue (`0x415797`
   inlined form) ticks the `ss+4` block: rate pointer at `+0xc`, the
   `0.99..1.01` NaN-aware window (native `0x470b68/0x470b64`) selects
   `+1.0` step vs raw rate accumulation.
7. **Spell/timer block** (battle record required): with `+0x9ec0` seconds
   >= 0 and no active result script, changes vs the shown value `+0x9ec4`
   drive state words 9/8 (seconds <= 5 / <= 10) with sound cues `0x24/0x1b`
   through the `0x492590` channel, word 7 on increase, and the two timer
   digit VMs `+0x793c`/`+0x7ce8` re-init with glyph `seconds/10 + 8` and
   `seconds%10 + 8`.
8. **Spell-card flag block** (`battle+0x2480` bit 4 set, bit 0 clear):
   practice mode (`dword_4776f4 + 0x378c` bit 0) uses thresholds
   `0x7d0/0x3e8/0x190/0x190`, otherwise `0x2bc/0x190/0xc8/0xc8` against
   `battle+0x2404`; the two-bit mode at `+0x9eb4 >> 1` selects which
   threshold applies, updating the `+0x9eb4` bits and the `+0x9d4c` word
   (7..0xa). The boss overlay anchor is `battle+0x1068 + 224.0`
   (`flt_470b4c`) with `+0x9d8c = 480.0`; when `|boss_x - anchor| < 64`
   the `+0x9d47` alpha byte is `0x40 - (u8)(|dx| * 0.75)` (byte-wrapped,
   native quirk preserved), else `0xff`, and anchors outside
   `[-192, 192]` force `0`.

## Globals Used

`DAT_00491C10` (entity manager), `dword_477704` (boss battle state),
`dword_477834` (screen target block), `dword_4776f4` (stage state),
`0x492590` (sound gate context), `dword_491FF4` (global mode flags),
`dword_491FB8` (HUD render mode), `dword_474C7C` (stage index),
`dword_474C84` (stage progress), `dword_474C4C` (score), `0x732454`
(entity id counter).

## Reconstruction

- `src/AsciiHudGameplayUpdate.cpp` / `src/AsciiHudGameplayUpdate.hpp`
  (`th10::UpdateAsciiHudGameplayStackAbi`). Helpers reused from
  `EntityHelpers`, `AsciiAnimationVm`, `TimelineRenderObjectSetup`,
  `ResultScreenScript`.
