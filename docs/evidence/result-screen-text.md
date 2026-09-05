# TH10 0x00415E90 — Result / Spell-Practice Screen Script Executor

Reconstructed as `RunResultScreenScriptStreamStackAbi` in
`src/ResultScreenScript.cpp/.hpp` (semantic, one stack argument, `ret 4`,
returns `i32`). The single caller is 0x0041490x (`0x00414900` region); no
existing src module referenced the address before this reconstruction.

## Purpose

The function is a once-per-frame executor for a byte stream that drives the
result / spell-card-list screens. The stream record layout matches the
timeline family: `u16 frame stamp, u8 opcode, u8 payload length, payload`,
advancing by `4 + payload_len`. Records run while timer A's current frame
(stored in the state object, not a local) is at or past the next record's
stamp; the timer is ticked through `0x00404ed0` at the epilogue. Opcodes
0x00..0x17 (24-entry jump table at 0x00415f16) cover text presentation
(decrypt 0x00417010 + submit 0x00447a50 into two line entities), glyph
entry setup on child entities (0x004497d0 + 0x00449670/0x004496a0), effect
spawns (0x00448d00), the spell-capture bookkeeping (opcodes 20/21), and a
BGM wait (opcode 10).

The function is *not* self-recursive in the decompile; it is re-entered once
per frame by its caller, which explains the "self-recursive" label seen in
triage tooling.

## State object (EBP frame, `ResultScreenScriptState`)

- `+0x00` selector (i32; `>= 1` gates opcode 2/13 variants)
- `+0x18` timer A `{prev i32, current i32, f32 view, rate ptr, flags}`
- `+0x2C` timer B (same layout; opcode 10 text/BGM wait)
- `+0x40..+0x54` six entity handles (a..f; text lines use d/e)
- `+0x5C` stream cursor
- `+0x60..+0x77` six float3 position slots (opcode 7/8 publish the slot at
  native byte offset `(select + 8) * 12`, i.e. `positions[0]` for select 0
  and `positions[3]` for select 1; both line handles receive the same slot)
- `+0x78` repeat counter (decremented each call; opcode 11 sets it to 1)
- `+0x7C` flags (bit0 re-arms the frame seeding block)
- `+0x80` two-line text latch (opcode 16)
- `+0x84` select index (position/owner selector)
- `+0x88` two per-select text owner handles

## Per-call prologue

1. If `repeat_counter > 0`, decrement it.
2. If `flags & 1` and `dword_474E5C & 0x100`: read the u16 stamp at the
   cursor; when timer A's flag word lacks bit 0, seed
   `prev=0, current=0, f32=0, rate=&flt_476F78, flags|=1` (prev stored as
   `0xFFF0BDC1` = -999999 at the first seed); then
   `current = frame; prev = frame - 1; f32 = (float)frame`.
3. If `current < (i32)stamp` at the cursor, tick timer A
   (`0x404ed0`, ESI ABI) and return 0.

## Opcode highlights (all verified against disassembly)

- **0**: clear `dword_474C8C` to 0 only when `dword_474C84 != 0`; always
  clear `dword_474C84`; return **-1** (no timer tick).
- **1/2/3/18/19**: spawn through `SpawnSetupEffectVmListABack` (0x00448d00);
  the native first stack argument (record buffer) is dead. Opcode 1/2 spawn
  only when `g_StageTextSprites[0]` is 0 or 1; opcode 2 picks script
  `0x477704+0x38` with kinds 32/37 for text layer 6 / (7 and selector < 1),
  otherwise script `0x477834+0x10` kind 30. Opcode 3 uses script
  `g_AsciiHudOwner+0x9EA8`, kind 90. Opcode 19 stores `*record` (the id) to
  handle f with per-layer kinds 13/16/20/21/14/35/29.
- **4/5/6/17**: expire (`0x409e50`) / fire (`0x40c4d0`) handle groups; 4 and
  5 additionally zero the expired handle (5 zeroes handle b then also
  expires handle f *without* zeroing it).
- **7/8**: fire b (7) / a (8), set state word 2 (`0x40c480`) on the other
  handle, set select index 0/1, publish positions (`0x4492f0`):
  first call targets `positions[0]` (7) / `positions[3]` (8), second
  `positions[select * 3]` (same slot once select is stored); clear the
  sub-counter.
- **9**: `flags ^= ((flags ^ payload_byte) & 1)` (byte at record+4).
- **10**: if timer B count <= 0, arm it with the payload duration
  (`0x405410`); always shift by -1.0f (`0x44bf40`). When
  `dword_474E62 & 0x1001` or count <= 0: enqueue BGM sound value 0 on
  channel 0 (`0x43dc90` with ECX = 0x492590, EDI = 0), re-arm with 0, clear
  the sub-counter. Otherwise: if `flags & 1` is clear or
  `dword_474E5C & 0x100` is clear, **return 0 without ticking timer A**
  (exit at 0x00416f2d); else re-arm with 0 and clear the sub-counter.
- **11**: `repeat_counter = 1`.
- **12**: two child resolves (`0x4497d0`, kinds 0x17/0x1a) on handle a plus
  glyph entries (0x00449670) at payload+0x34/+0x3c (sprites[0] == 0) or
  +0x2d/+0x35 (== 1).
- **13**: per text layer 1..7 child resolves (kinds 0x18/0x1b, except 6 =
  0x1c/0x1f and 7 = 0x21/0x24 when selector < 1) with entry offsets
  payload+{0x0f/0x17, 0x15/0x1e, 0x22/0x2b, 0x25/0x2e, 0x16/0x1f, 0x30/0x39,
  0x2e/0x37 or 0x44/0x4d}. The 0x004496a0 variant passes the resource dword
  `0x477704+0x38` on the stack.
- **14/15**: resolve handle d/e (zero it when the entity is missing),
  decrypt the record payload (0x00417010) and submit via 0x00447a50 with
  owner `0x491c10` and the per-select owner dword, then set state word 2 on
  the same handle.
- **16**: sub-counter 0 path submits a decrypted line to handle d flanked by
  a blank line (`" "`) to each of d/e, sets state word 2 on d, fires e, and
  increments the latch; the latch path resubmits the same decrypted payload
  to handle e and clears the latch.
- **20**: when stage (`dword_474C74`) != 4, set two "cleared" flag bytes at
  `0x47783C + (3*sprites[0] + sprites[1])*0x437C +
  (text_layer + 6*stage)*8 + {0x4E0, 0x4E1}`. Then:
  - `dword_474CA0 & 0x10`: call 0x00423370 with EDI = `dword_477830` and stop.
  - layer 6: `g_AsciiHudOwner+0x9EB4 |= 0x20`; score adds
    (`0x409d90`, ECX = 0x474C40) of `1000 * dword_474C4C` plus the
    difficulty-scaled `dword_474C70`/`word_474C48` pair
    (20M/100k, 25M/100k, 35M/200k, 40M/300k, 40M/400k);
    if `0x477838+0x10 == 1` request state 4 (`0x40ac90`, EAX =
    `dword_491c28`), else create the kind-0x31 overlay context
    (`0x43c8b0`: stack 5, 0x78, 0, 0, 0 with EBX = 0x31), set
    `+0x9EB4 |= 0x10` and `+0x9ECC = 0`, then bump the practice counter.
  - layer 7: same flags plus 1000×base, 40M×lives, 400k×gauge; state 4 in
    game mode, otherwise 0x00423370 and counter bump.
  - other layers: soft-release the handle at `g_AsciiHudOwner+0x9E14`
    (`0x4492a0`), zero it, spawn script `+0x9EA8` kind 0x4C into it, request
    state 11, and call 0x004175e0 with EAX = 0x474C40.
  - Counter: dword at `0x47783C + ((3*sprites[0] + sprites[1])*0x10DF +
    stage)*4 + 0x4D0`, incremented only while `< 99999` (0x1869F).
- **21**: BGM value 8.0 for layer 6, else 2.0 (`0x420c30`).
- **22/23**: `SetEntityStateWordEaxEsiAbi` value 7 on handle a / b.
- Default: nothing; every non-returning path advances the cursor and
  re-checks the frame gate.

## Callee boundaries

Semantic bodies reused directly: 0x00404ed0/0x00405410/0x0044bf40
(PlayerTimerHelpers), 0x00409e50/0x0040c4d0/0x004492f0/0x004492a0/0x00449470/
0x004491c0 (EntityHelpers), 0x004497d0 (GameManagerState), 0x00448d00
(TimelineRenderObjects), 0x00417010 (TimelineRenderObjects decrypt),
0x00447a50 (TimelineTextSubmission), 0x0043c8b0 (AsciiOverlayFactory),
0x0040ac90 (PlayerFrameworkHelpers), 0x00420b10/0x00420c30
(TimelineAudioActions), 0x0043dc90 (BgmRuntime), 0x0043e5a0
(AsciiAnimationVm).

0x0040c480, 0x00449670 and 0x004496a0 were small enough to reconstruct
locally in `ResultScreenScript.cpp` (state word 2 propagation; glyph entry
setup with entity-local or stack-supplied resource).

Remaining extern boundaries declared in `ResultScreenScript.hpp`:
`AddScoreBlockValueEcxStackAbi` (0x00409d90, ECX = 0x474C40, stack value,
ret 4), `RecordSpellPracticeCaptureEdiAbi` (0x00423370, EDI = 0x477830) and
`UpdateScoreBlockEaxAbi` (0x004175e0, EAX = 0x474C40).

## Globals (names reused from existing modules)

`g_MainChainRenderOwner` (0x491C10), `g_MainChainContext` (0x491C28),
`g_AsciiHudConditionalState` (0x477704), `g_AsciiHudOwner` (0x47770C),
`g_GameStateManager` (0x477830), `g_OptionPositionBase` (0x477834),
`g_GameModeObject` (0x477838), `g_SpellPracticeRecords` (0x47783C region),
`g_TitleTransitionRecord` (0x477848), `g_TransitionRoot` (0x492590),
`g_StageTextSprites[2]` (0x474C68/6C), `g_StageScoreSelector[2]`
(0x474C74/78), `g_ActiveTextLayer` (0x474C7C), `g_ScoreBonusBaseDword`
(0x474C4C), `g_PlayerLivesRemaining` (0x474C70), `g_PlayerPowerGaugeWord`
(0x474C48 word view — the native multiplies use `movsx`), `g_TextStyleFlag`
(0x474C84), `g_TextStyleAlt`
(0x474C8C), `g_GlobalModeFlags` (0x474CA0), `g_InputMaskWord` (0x474E5C
dword view), `g_ResultGateFlags` (0x474E62), `g_MainChainStartupScale`
(0x476F78).

## Boundaries resolved into semantic bodies

- `0x00409d90` `AddScoreBlockValueEcxStackAbi` (native __thiscall ECX =
  score block 0x00474c40, stack = value, ret 4): adds `value / 10`
  (signed, truncating) to the +4 total; the >= 1000000000 clamp runs
  after the store, capping at 999999999; returns the stored value.
- `0x004175e0` `UpdateScoreBlockEaxAbi` (native EAX = score block):
  advances the +0x3c slot index while below 7 and publishes the
  48-byte slot pointer `0x00474788 + 48 * index` to DAT_00477848,
  returning it.

Remaining executor boundary: 0x00423370
`RecordSpellPracticeCaptureEdiAbi` (EDI = game state manager).

- `0x00423370` `RecordSpellPracticeCaptureEdiAbi` (native usercall:
  EDI = the DAT_00477830 spell state; the ECX input is never read —
  caller garbage at the known sites). Title+0x5c == 1 publishes the
  pending shared status (2 when 0x491ff4 bit 0x1000, else 4) and returns
  it. Otherwise: sub-state 6, score-anim block seed at +0x10..+0x20 with
  accumulator -1, title+0x58 |= 0x10, two pool VMs (render mode 15,
  scripts 0/129, list-B links; first id published to +0x1d8 and used as
  the overlay entity of `CreateGameOverOverlay(manager, id, 32, 16, 384,
  448)`), HUD owner +0x9ec8 copied to +0x2c4, `StartBgmTrack
  ("bgm/th10_17.wav")`, the 0x10-gated and plain BGM commands, the clear
  byte at 0x47783c+0x1d8a3, +0x1e4 = 1, and the 0x476f78 time-scale
  swapped to 1.0 after its old value is copied to +0x2c0.

## Glyph wrappers (0x00449670 / 0x004496a0)

`SetEntityGlyphFromHandleEaxStackAbi` (native EAX = handle slot, stack =
entry) and `SetEntityGlyphWithResourceEaxStackAbi` (native EAX = handle
slot, stack = resource then entry) resolve the entity via
`FindEntityEdxStackAbi` and re-run `InitializeAsciiAnimationVmEntry` on
it — the first with the entity's own +0x308 resource, the second with
the stack resource. Both are implemented in src/ResultScreenScript.cpp.
