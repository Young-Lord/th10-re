# TH10 RE Handoff

## Objective And Rules

Continue semantic C++ reconstruction until complete, genuinely blocked, or a
major engineering decision is needed. Prioritize reconstructed logic over
assembly/object matching. Preserve evidenced native oddities: unchecked
pointers, partial cleanup, NaN branches, overflow/wrap behavior, and unusual
register ABI only at thin thunk boundaries.

The entire worktree is untracked (`git status` shows every top-level project
directory as `??`). Do not reset, checkout, or delete files.

## Verification Baseline

All of these passed after the latest additions:

```sh
scripts/compile-main-chain-cpp.sh
g++ -m32 -std=c++98 -fsyntax-only -I src src/*.cpp
git diff --check
```

The compile script includes `AsciiOwnerTraversal.cpp` and
`AsciiHudRenderer.cpp`.

## Implemented ASCII Pipeline

### VM Lifecycle And Renderer Modes

- `0x401de0` `ResetAsciiAnimationVmRecord` in `src/AsciiAnimationVm.cpp`:
  preserves `+0x20/+0x340/+0x344/+0x348`, clears `0x3ac`, restores defaults.
- `0x43e5a0` `InitializeAsciiAnimationVmEntry`: initializes glyph metadata at
  `resource+0x118 + index*0x44`; it is not a bytecode executor.
- `0x4451c0` `DispatchAsciiAnimationVmRenderMode` in
  `src/AsciiRenderModeDispatcher.cpp`: semantic targets exist for modes 0..9;
  modes 10..15 return zero. Modes 8 and 9 are direct D3D paths.

### Scene Traversal And Culling

Implemented in `src/AsciiOwnerTraversal.cpp/.hpp`:

- `0x403090` `CullAsciiSceneChild`:
  - Effective native inputs: EAX camera work, ECX child record, EDX outer
    translation, one stack squared-distance limit; returns 0 visible/1 culled.
  - NaN squared-distance comparison culls.
  - Builds an eight-corner half-extent box, projects it, accepts only ordered
    `0 <= z <= 1`, and intersects closed screen region `x=[32,416]`,
    `y=[16,464]`.
  - Preserves native seeded bounds and NaN minimum-update behavior.
- `0x403a30` `RenderAsciiSceneChannel`:
  - First argument is a scene/controller, not the render owner. VM pool,
    child table, cull limit, and counters belong to the scene.
  - Uses signed-terminated outer descriptors at `scene+0x18` (stride `0x10`)
    and signed-terminated variable operation records at `child+0x1c`; records
    advance by signed 16-bit byte delta at `+0x02`.
  - Opcode zero alone updates transform/scale for VM modes >=4. Ordered zero
    scale skips while NaN proceeds to division.
  - Every nonnegative operation switches mode-8 fog state when required,
    dispatches with global render owner, and increments `scene+0x2a14` even if
    the dispatcher rejects the VM. Accepted/cull counters are `+0x2a0c` and
    `+0x2a10`.

Evidence: `docs/evidence/ascii-scene-traversal.md`.

### HUD Batch And Immediate Rectangles

Implemented in `src/AsciiHudRenderer.cpp/.hpp`:

- `0x43bda0` `DrawImmediateAsciiColoredRectangle`:
  - Native ABI boundary is EBX packed color and EDI `{left,top,right,bottom}`.
  - Flushes glyph vertices, makes four 20-byte `XYZRHW|DIFFUSE` vertices in
    TL/TR/BL/BR order, then issues `D3DPT_TRIANGLESTRIP`, 2 primitives,
    stride `0x14`.
  - Sets stage-0 diffuse selection and `D3DRS_DESTBLEND=6`, then restores only
    glyph texture-modulation stage state.
  - Invalidates owner caches `+0x3ada64`, `+0x3ada68..+0x3ada6b`, and
    `+0x3ada70`; intentionally does not reset `+0x3ada6e` or restore FVF and
    destination blend.
- `0x415800` `RenderAsciiHudBatch`:
  - Stack owner input, `ret 4`, always returns 1.
  - Dispatches 10 interleaved pairs at `+0x10/+0x24c8`, then 9 at `+0x4980`,
    4 at `+0x6a8c`, and 7 at `+0x8094`, all at stride `0x3ac`.
  - Conditionally dispatches `+0x9a48` and two VMs at `+0x793c`.
  - Draws progress rectangles and preserves NaN-sensitive auxiliary entry
    handling at `owner+0x9e94`.

Evidence: `docs/evidence/ascii-hud-renderer.md`.

## Ghidra And Function Status

Ghidra names/comments added:

- `0x403090` `CullAsciiSceneChildEaxEcxEdxStackAbi`
- `0x403a30` `RenderAsciiSceneChannelStackAbi`
- `0x43bda0` `DrawImmediateAsciiColoredRectangleEbxEdiAbi`
- `0x415800` `RenderAsciiHudBatchStackAbi`

Earlier ASCII functions are also named/commented, including `0x442670`,
`0x442fe0`, `0x4425a0`, `0x43e5a0`, `0x443b60`, `0x444240`, `0x4451c0`,
`0x4445c0`, `0x444760`, and `0x444ce0`.

`config/function-status.csv` records all four new functions above as
implemented.

## Completed: 0x426360 (was Next Target)

Implemented as `RenderAsciiSceneObjectAndBarOverlay` in
`src/AsciiSceneObjectRenderer.cpp/.hpp` (semantic, EAX ABI thunk boundary).
The interrupted subagents listed below had no recoverable results in this
session; the analysis was redone directly.

New findings beyond the read-only notes:

- The callback slots at `object+0x3334 + i*0x98` belong to records whose
  bases are `object+0x32a0 + i*0x98` (pointer sits at `record+0x94`); each
  non-null callback is invoked with ECX=record.
- `0x427c50` is a `ret 4` stdcall wrapper: it widens its float to a double
  and tail-calls the CRT `floor` at `0x452ff0`. This pins the stack frame and
  explains the `FILD` of the saved bar value.
- Bar formulas: `left = floor(3c0 + DAT_00470cbc - DAT_00470b48)`,
  `top = floor(3c4 + DAT_00470cb4 - DAT_00470be8)`,
  `right = left + (float)(i32)DAT_00474c58 * DAT_00470cb8`,
  `bottom = top + DAT_00470b08`; rect 1 color `0x80000000`, rect 2 is inset
  by one pixel with color `0xffffffff`, same idiom as the HUD batch bar.
- Gate details: `[DAT_00477704+0x10] == 0` (single deref), 
  `[DAT_0047770c+0x9eb8] == 0`, and `DAT_00477830` is dereferenced without a
  null check before requiring `[DAT_00477830+4] == 0`.

Evidence: `docs/evidence/ascii-scene-object-bar.md`.
`config/function-status.csv` records it as implemented; Ghidra names
`0x426360` `RenderAsciiSceneObjectAndBarOverlayEaxAbi` and `0x427c50`
`AsciiFloorWrapperStdcall`.

All baseline checks pass (`compile-main-chain-cpp.sh`, g++ syntax check,
`git diff --check`).

## Overlay Scheduler Progress

Implemented:

- `src/AsciiOverlayCallbacks.cpp/.hpp`
  - `0x43c1a0`: full-screen packed-color rectangle, including global
    `DAT_00491cf4` 640x480 viewport publication and pre-viewport glyph flush.
  - `0x43c2c0` and `0x43c410`: separate scheduler entries with identical
    unmasked packed-color `{32,16,416,464}` rectangles.
  - `0x43c500`: same inset rectangle but alpha plus explicit masked RGB from
    `context+0x24`.
  - `0x43c230`: type 2/5 fade-in state machine, native suspend/end return
    code, alpha clamp, title mask gate, wrap behavior, and x87 conversion.
  - `0x43c310`: type 6/7 half-alpha state machine, including fade-out end and
    intentionally unclamped alpha.
  - `0x43c3c0`: kind-6 full-screen rectangle using the unmasked packed color,
    deliberately without the kind-0 explicit viewport/flush setup.
  - `0x43c710`: kind-8 no-draw random render-offset state machine. It keeps
    the native title mask `0x77`, post-advance completion, phase-bound
    wraparound, independent X/Y random choices, and signed-rise versus
    unsigned-fall conversion distinction.
- `src/AsciiOverlayFactory.cpp/.hpp`
  - `0x43c8b0` allocates and zeroes the 0x44-byte context, ignores its fourth
    explicit word and duplicates the fifth.
  - `0x43c970` reconstructs the kind table and priority split for calculation
    versus draw nodes, timing defaults, cleanup callback write, invalid-kind
    null-node failure behavior, and direct `CallbackSchedulerApi` registration.
  - Cleanup `0x43c870` is modeled in the factory: it synchronously removes the
    calculation node, then the draw node, then frees the context.

Evidence is in `docs/evidence/ascii-overlay-callbacks.md` and
`docs/evidence/ascii-overlay-factory.md`. Function-status records and Ghidra
names/comments are updated. Current additions are:

- `0x43c1a0` `DrawAsciiFullScreenOverlayWithViewportEcxAbi`
- `0x43c2c0` `DrawAsciiInsetOverlayAEcxAbi`
- `0x43c410` `DrawAsciiInsetOverlayBEcxAbi`
- `0x43c500` `DrawAsciiInsetOverlayMaskedRgbEcxAbi`
- `0x43c8b0` `CreateAsciiOverlayContextEbxStackAbi`
- `0x43c970` `ConfigureAsciiOverlayContextEsiEbxStackAbi`

The following callback addresses may not be outlined as Ghidra functions; use
raw binary disassembly when necessary, while retaining comments where Ghidra
permits them.

## Overlay Cluster Completion And Recommended Continuation

The factory-referenced calculation callbacks are now all semantic C++:

- `0x43bd40` `UpdateAsciiOverlayFullFade` (kinds 0 and 3)
- `0x43c460` `UpdateAsciiOverlayKindFive` (kind 5)
- `0x43c550` `UpdateAsciiOverlayKindOne` (kind 1)

`src/AsciiOverlayFactory.cpp` now has no callback placeholders. The Ghidra
database currently does not outline these three entries as functions, so their
semantic names are maintained in C++, evidence, and function status; entry
comments should be maintained in Ghidra until functions can be created there.
Useful native corroboration remains `0x43bd40..0x43cb7c`, the scheduler
allocation/adapters at `0x449ed0..0x44a058`, and RNG modulo helper
`0x43cb80..0x43cbd7`.

For the overlay context used by all callbacks, keep offsets explicit rather
than normalizing away observed behavior: node pointers `+0x08/+0x0c`, kind
`+0x10`, alpha `+0x18`, phase values `+0x1c/+0x20/+0x24/+0x28`, fade-out flag
`+0x2c`, previous/current ticks `+0x30/+0x34`, accumulated float `+0x38`, and
rate pointer `+0x3c`. Scheduler callbacks receive the context in ECX at the
native boundary. Preserve invalid-kind dereference, unchecked node allocation,
and overflow/NaN behavior unless new evidence contradicts it.

After each reconstruction, update all of:

- `src/AsciiOverlayCallbacks.cpp/.hpp` (or a focused sibling module)
- `docs/evidence/ascii-overlay-callbacks.md`
- `config/function-status.csv`
- Ghidra function name/comment where supported

Then run the three baseline commands above. `th10-re` is the checkout name,
but the binary and several existing namespaces/file labels say `th10`; do not
treat either version label as independently verified without checking project
configuration and binary provenance.

## Timeline Record Interpreter

Implemented across:

- `src/TimelineRecordInterpreter.cpp/.hpp`
- `src/TimelineStreamLoader.cpp/.hpp`
- `src/TimelineContinuation.cpp/.hpp`
- `src/TimelineRenderObjects.cpp/.hpp`
- `src/TimelineTextSubmission.cpp/.hpp`
- `src/TimelineAudioActions.cpp/.hpp`
- `src/TimelineGateState.cpp/.hpp`

Current coverage:

- `0x0040bd80` / `0x0040bd20` interpreter and outer controller
- opcode dispatch 0..17 including replacement stream (12), continuation (7),
  overlay create (13/14), text (3/4), object create (8/15/16/17), audio
  (10/11), transition (5/6)
- `0x0040b480` replacement stream load through gate state `+0x14`
- continuation startup/thread/body/register chain
  (`0x40c540`, `0x40c3a0`, `0x40c3c0`, `0x44c1c0`)
- render-object handle lookup/release and manager-work clone/bind
  (`0x4491c0`, `0x4492a0`, `0x449630`, `0x448d50`, `0x3e7e0`,
  `0x3e710` semantics). Note: `0x448d00` is now attributed to the
  setup-script pool spawn `SpawnSetupEffectVmListABack` (see the
  spawn-creator resolution note below), not to this clone path.
- text decrypt/submit (`0x417010`, `0x447a50`) with GDI/D3DX left as platform
  boundary
- audio helpers (`0x420a90`, `0x420b10`, `0x420c30`)

Key layout facts:

- state `+0x54` current record, `+0x70` continuation path, `+0x80[]` source
  manager-work slots, `+0x90[]` destination render handles
- timeline source slots are manager-work pointers, not render handles
- opcode 7 stores `record+8` at `+0x70`, releases owner slot `record[+4]+0x1d`,
  and advances with the normal payload step; opcode 12 clears `0xec` bytes and skips the
  replacement stream header dword

Remaining narrow boundary:

- TitleScreen / GameManager / Transition lifecycle
  (`0x4180e0`, `0x418150`, `0x417c80`, `0x42cd50`, `0x40b940`, …)
- D3DX import thunk for `D3DXLoadSurfaceFromMemory` remains a link boundary;
  semantic GDI raster paths are in `GeneratedSurfaceText.cpp`

Evidence: `docs/evidence/timeline-record-interpreter.md`.

Two globals still lack upstream confirmation: `DAT_0047770c`
(`g_AsciiHudOverlayState`) and `DAT_00477830` (`g_AsciiHudGateState`). Their
names come only from observed use.

## Player Object Lifecycle (0x4247f0) And 0x426360 Caller Chain

Implemented:

- `src/AsciiSceneObjectRenderer.cpp/.hpp` — `0x426360`
  `RenderAsciiSceneObjectAndBarOverlayEaxAbi`: mode-2 skip, scrolled VM
  position publication, four ECX record callbacks (bases `+0x32a0`, stride
  `0x98`, pointer at `record+0x94`), title-gated two-rect bar overlay, and
  unconditional one result. Evidence:
  `docs/evidence/ascii-scene-object-bar.md`.
- `0x427c50` `AsciiFloorWrapperStdcall`: `ret 4` stdcall wrapper widening
  its float to a double for the CRT `floor` at `0x452ff0`.
- `src/PlayerObjectLifecycle.cpp/.hpp` — `0x4247f0`
  `InitializePlayerObjectEbxAbi`: `pl00/pl01.anm` manager-work slot 8
  request, update thunk `0x00426500` (calc chain, priority `0x10`) and draw
  thunk `0x00426510` (`MOV EAX,ECX; JMP 0x00426360`) registration, main VM
  init via `0x404f30`, shot tables `0x476fa0..0x476fbf`, hitbox/graze/item
  boxes, NaN-transient entity records, and the `0x426f70` option rebuild.
  Evidence: `docs/evidence/player-object-lifecycle.md`.
- `0x00425730` named `UpdatePlayerModeDispatcherStackAbi` in Ghidra (mode
  0..4 jump table `0x00426344`); it is the next reconstruction target and
  shares the player object layout.

Baseline repair required by files left broken by the interrupted session:
`LargeRenderOwnerFrameLoop.cpp` (OwnerNode/LargeRenderOwnerLayout padding
rearranged to satisfy the verified offset asserts and native list heads) and
`TimelineRenderObjectSetup.cpp` (dropped `<stdint.h>`, replaced
`std::isnan` with the `x != x` idiom for the VC++ Toolkit 2003 toolchain).

All three baseline commands pass with these additions.

## Player Mode Dispatcher And Option Rebuild

Implemented (semantic C++ from two parallel subagent analysis passes):

- `src/PlayerModeDispatcher.cpp/.hpp` — `0x00425730`
  `UpdatePlayerModeDispatcherStackAbi`: mode 0..4 handlers with in-call
  fallthroughs (respawn intro, normal play, death explosion, bomb freeze,
  deathbomb window) and the common epilogue; 32 sub-effect records, scaled
  timer blocks (rate window 0.99..1.01 at `0x470b68/b64`, round-half-away
  conversion `0x463b2c`), invincibility flash, box recomputation, item
  magnet, projectile manager. Callees that are still unreconstructed
  (`0x4250b0`, `0x4269d0`, `0x428280`, `0x43ee30`, `0x427b50`, `0x426610`,
  `0x44bc70`, `0x44c2a0`, `0x44c5d0`, `0x405410`, `0x405860`, `0x4054b0`,
  `0x408030`, `0x408100`, `0x41c800`, `0x41c850`, `0x40ac90`, `0x4231d0`,
  `0x41bb00`, `0x412e70`) are `...Abi` extern boundaries.
- `src/PlayerOptionRecords.cpp/.hpp` — `0x00426f70`
  `RebuildPlayerOptionRecordsStackAbi`: R+0x6c power-effect early loop
  (soft release + unconditional respawn at power>=100, hard release below),
  `count = min(power/20, 4)` rebuild with character 0/1 and sub-type
  0/1/2 branches (including the spawn-then-kill `0x449470` quirk and the
  `R+0x90` callback stores `0x427950`/`0x427ad0`), tail clear with hard
  release, tier latches. Entity list lookup (`0x472dad4/0x472dadc` heads)
  and child propagation are implemented inline.

Known approximation: the sub-effect record F2I destination is modeled as
the count field `rec+0x44`; the native `EDI = rec+0x24`-relative alias needs
one more verification pass. Evidence:
`docs/evidence/player-mode-dispatcher.md`.

## Next Targets

- `0x00425730` callees in dependency order: `0x004250b0` movement,
  `0x004269d0` death processor, `0x004281d0`/`0x00428280` item/projectile
  managers, `0x00427950`/`0x00427ad0` option callbacks.
- `0x00404f30` / `0x00426520` player VM init and shot-data loader (full
  specs already in `docs/evidence/player-mode-dispatcher.md`).
- `0x0043ee30` shared animation-VM update (huge; benefits every VM path).

## Player Movement And Death Processor

- `src/PlayerMovement.cpp/.hpp` — `0x004250b0` `UpdatePlayerMovementEdiAbi`
  (native EDI ABI): diagonal-priority input decode, focus gating against
  the HUD-conditional global and the second timer, speed switch, one
  frame-scaled fixed-point step with exact clamps, history-ring tail
  saturation + shift (unfocused and moving only), and the four option
  record updates including the `R+0x90` callbacks and entity position
  publication. Evidence: `docs/evidence/player-movement.md`.
- `src/PlayerDeathProcessor.cpp/.hpp` — `0x004269d0`
  `ProcessPlayerDeathStackAbi` (stack ret 4): power-floor penalty on
  `0x474c4c`, life decrement with icon refresh (`0x413790`), mode 2, both
  timer resets, embedded-record rebind (`0x43e710`), hard-kill option
  teardown, Caution! text gate, boss-bullet flag clearing, and the
  `0x474c98` clamp. Evidence: `docs/evidence/player-death-processor.md`.
- Dispatcher verification pass completed: the scaled-timer branch
  direction, the sub-effect F2I destination (`rec+0x48`), the record
  accumulation adds, the polar z zeroing, the death-burst angle ABI, and
  the every-frame item magnet were all corrected against the
  disassembly; the previously flagged F2I approximation is resolved.
  Evidence: `docs/evidence/player-mode-dispatcher.md`.

Remaining dispatcher externs (unreconstructed): `0x405410` timer tick,
`0x405860` context tick, `0x4054b0` HUD lives, `0x408030`/`0x408100`/
`0x41c800`/`0x41c850` stage helpers, `0x40ac90`/`0x4231d0` game-over paths,
`0x41bb00` explosion particles, `0x412e70` timed sequence, `0x427b50`
sub-effect spawner, `0x426610` burst angle, `0x4281d0`/`0x428280`
item/projectile managers, `0x43ee30` VM update, `0x44bc70`/`0x44c2a0`/
`0x44c5d0` motion helpers, `0x4491c0`/`0x449210`/`0x4492f0`/`0x449350`
entity helpers, `0x413790`/`0x424650`, and `0x427950`/`0x427ad0` option
callbacks.

## Item Magnet, Projectiles, Option Callbacks, Shot Data

- `src/PlayerItemMagnet.cpp/.hpp` — `0x4281d0`/`0x428160`: autocollect
  sawtooth timer and the power/focus schedule walk (shot spawner
  `0x427e90` stays an extern).
- `src/PlayerProjectileManager.cpp/.hpp` — `0x428280`: the 128-record
  player-shot pass (type-3 lifetime, one-shot fire, motion modes, playfield
  cull via `0x428d70`, position/angle publication, age advance). No enemy
  collision in this pass; damage lives in the consumers.
- `src/PlayerOptionCallbacks.cpp/.hpp` — `0x427950`/`0x427ad0`: the option
  position-mode/trail and anchor-latch callbacks (state-word switches via
  `0x449470`, now shared as `SetEntityStateWordEaxEsiAbi`).
- `src/PlayerShotData.cpp/.hpp` — `0x404f30`, `0x43e710`, `0x426520`:
  player VM init, script binding (with the whole-record wipe failure
  path), and shot-data loading (pointer rebasing + four handler tables).
  `PlayerObjectLifecycle.cpp` now calls these semantic bodies directly.
Evidence: `docs/evidence/player-item-magnet.md` and
`docs/evidence/player-shot-data.md`.

Still external: `0x427e90` shot spawner, `0x405410`/`0x404ed0`/`0x44bf40`
timer utilities, `0x428d70` cull, `0x44c2a0`/`0x44c5d0`/`0x44bc70` motion
helpers, `0x4491c0`/`0x4492a0`/`0x4492f0`/`0x449350`/`0x409e50`/`0x40c4d0`
entity helpers, `0x43ee30` VM update, `0x405860`/`0x4054b0`/`0x408030`/
`0x408100`/`0x41c800`/`0x41c850`/`0x40ac90`/`0x4231d0`/`0x41bb00`/
`0x412e70`/`0x413790`/`0x424650` framework helpers.

## Shot Spawner And Timer/Motion Helpers

- `src/PlayerShotSpawner.cpp/.hpp` — `0x427e90`
  `SpawnPlayerShotStackAbi` (ret 0xc): the full player-shot spawn path
  including the once-per-slot latch, position sources, velocity offset,
  entity A/B allocation with angle injection, the descriptor callback
  (ECX=player, EDX=record, frame on stack), and the sound queue.
- `src/PlayerTimerHelpers.cpp/.hpp` — `0x405410`, `0x404ed0`, `0x44bf40`:
  the shared scaled-timer utilities (arm / forward tick / shift), each
  verified against disassembly.
- `src/PlayerMotionHelpers.cpp/.hpp` — `0x44bc70`, `0x44c5d0`, `0x44c2a0`:
  angle wrap (32-iteration budget, unnormalized fallthrough), polar
  conversion, and motion integration with `floor(v*100)*0.01` quantization
  (this supersedes the earlier 1/200-rounding note for the dispatcher's
  sub-effect records; the extern is now the shared semantic body).
Evidence: `docs/evidence/player-shot-spawner-helpers.md`.

Still external: `0x428d70` playfield cull, `0x4491c0`/`0x4492a0`/
`0x4492f0`/`0x449350`/`0x409e50`/`0x40c4d0`/`0x449950`/`0x449870`/
`0x4489d0` entity helpers, `0x43dd10` sound queue, `0x43ee30` VM update,
and the framework helpers `0x405860`/`0x4054b0`/`0x408030`/`0x408100`/
`0x41c800`/`0x41c850`/`0x40ac90`/`0x4231d0`/`0x41bb00`/`0x412e70`/
`0x413790`/`0x424650`.

## Entity Helpers, Cull, And Pool Allocation

`src/EntityHelpers.cpp/.hpp` now implements the full entity-helper family
semantically (verified against disassembly by a subagent pass):
`0x4491c0`/`0x4492a0`/`0x449210`/`0x449470`/`0x4492f0`/`0x449350`/
`0x409e50`/`0x40c4d0`/`0x428d70` plus the pool trio `0x449950`
(cursor/used-flag ownership, heap fallback without flag, null-reset
quirk), `0x449870` (corrected to a stack-arg ABI; the `0x43e7e0` script
bind stays a boundary), and `0x4489d0` (corrected to EBX=entity, list-1
only, tail-splice quirk, id counter wrapping to 1). All modules now call
these semantic bodies; the local extern declarations are gone.
Evidence: `docs/evidence/entity-helpers.md`.

Remaining external: `0x43e7e0` effect-script bind, `0x43ee30` VM
interpreter, `0x427b50` sub-effect spawner, `0x426610` burst angle, and
the framework helpers `0x405860`/`0x4054b0`/`0x408030`/`0x408100`/
`0x41c800`/`0x41c850`/`0x40ac90`/`0x4231d0`/`0x41bb00`/`0x412e70`/
`0x413790`/`0x424650`.

## Framework Helpers Batch Two

`src/PlayerFrameworkHelpers.cpp/.hpp` implements `0x40ac90`, `0x412e70`
(corrected: it drains `arg/10` from the maximum-score dword `0x474c4c`
with a 5000 floor — not a sequence starter), `0x413790`, `0x424650`,
`0x426610` (atan2 with the exact-zero π/2 fallback), `0x41bb00`
(explosion records incl. the kind-8 ring variant), `0x4231d0` (game-over
path B), and `0x43e7e0` (effect script bind; scripts at ctx+0x11c, stop
flag ctx+0x124, wipe-on-failure). `0x427b50` is implemented in
`PlayerShotSpawner.cpp` (full-record zeroing every spawn). The
`BindEffectScriptEcxEaxAbi` boundary is gone — `EntityHelpers` now calls
the semantic bind with the runtime-filled `g_EffectScriptContext`.
Evidence: `docs/evidence/player-framework-helpers.md`.

Remaining external: `0x405860`/`0x4054b0`/`0x408030`/`0x408100`/
`0x41c800`/`0x41c850` (analysis in flight), `0x420a90`/`0x43e460`/
`0x448ac0`/`0x424480`/`0x41a120`/`0x41beb0`/`0x448db0` (narrow boundaries
inside the game-over/explosion paths), and `0x43ee30` VM interpreter.

## Stage Helpers Batch

`src/PlayerStageHelpers.cpp/.hpp` implements `0x405860` (corrected: the
respawn-effect tick, not a context tick), `0x4054b0` (lives digits through
the existing `0x43e5a0` semantic body), `0x408030`, `0x408100`
(intro sweep — replaces the `SetStageIntroScroll` extern reading),
`0x41c800`, and `0x41c850` (bullet-clear broadcast; the dispatcher now
calls the semantic bodies with `0x4776f0`/`0x47781c` managers).
Evidence: `docs/evidence/player-stage-helpers.md`.

Remaining external: `0x43ee30` VM interpreter, `0x448db0` stage effect
spawn, `0x405500` stage life-flag cleanup, `0x420a90`/`0x43e460`/
`0x448ac0`/`0x424480`/`0x41a120`/`0x41beb0`/`0x448db0` narrow boundaries
inside the game-over/explosion paths.

## Narrow Boundaries And 0x43ee30 Resolution

`src/StageEffectHelpers.cpp/.hpp` implements `0x448db0`, `0x405500`
(exactly seven per-life flags — corrected), `0x420a90` (queue push through
the existing `QueueBgmCommand`; the "halt" reading was corrected),
`0x448ac0` (list-B registration twin), `0x424480`, `0x41a120` (with the
position now forwarded from `ShowCautionText`), and `0x41beb0`.

**`0x43ee30` resolved without new code**: it is the already-reconstructed
`FinalizeTimelineRenderObjectSetup` (one stack argument, dispatch table
0x4413a4, opcodes -1..0x5b). The three-register-arg call-site models were
caller garbage; all sites now call the semantic body directly and the
per-site externs are gone. Evidence:
`docs/evidence/stage-effect-helpers.md`.

Remaining external (deepest layer, all leaf-ish): `0x404610`/`0x4050d0`
transform helpers, `0x413200`/`0x41ab70` interp setup, `0x4423xx` spawn
variants, `0x4452f0` rendered-object release, `0x448f60`/`0x449090`
entity spawn variants, `0x4416xx-0x441ef0` catch-up helpers,
`0x463bba`/`0x463bf0` math, `0x428dd0` timer op, `0x4046b0`-family, plus
the CRT pieces (`0x452493` operator new, `0x402050` blank construct) that
are modeled inline.

## VM Leaf Helpers And Port Corrections

`src/VmLeafHelpers.cpp/.hpp` implements `0x404610`, `0x4050d0`,
`0x413200`, `0x41ab70`, the four color/alpha interpolation setups
(`0x42220`/`0x42050`/`0x42300`/`0x441f50`, unified), and `0x428dd0`;
`0x463bba`/`0x463bf0` are the CRT fmod/acos intrinsics (std:: at call
sites). Subagent verification also surfaced and fixed port bugs in
`TimelineRenderObjectSetup.cpp`: seven sampling bases were shifted
(0x70/0xbc/0x108/0x180/0x134/0x1bc/0x208), `StartVec3Anim`'s trigger
block, and `CopyCreatedObjectVectors` (all three dwords, unconditionally).
Evidence: `docs/evidence/vm-leaf-helpers.md`.

Known open issue (documented): the `DispatchSetupOpcode` opcode ids
diverge from the binary jump table 0x4413a4 (e.g. binary 0x3b = RGB anim
#1; spawns at 0x5a..0x5d). Re-keying the dispatch needs a full
jump-table pass — recommended next major step. `0x4452f0` (ribbon ring
rebuild) is specced but kept as a boundary pending the LCG/callback pair.

## Ribbon Rebuild

`0x4452f0` implemented as `RebuildRibbonRingBuffer` in
`src/VmLeafHelpers.cpp` (buffer free/realloc at entity+0x358, callback
installation at +0x398/+0x39c — the two callbacks remain boundaries — and
the 32-vertex seed with the clamped radial random walk over the shared
16-bit LCG pair at 0x4918b0/0x4918b4). The jitter-field scaling
("rand*1/120") is modeled as a unit draw divided by 120; the exact native
scaling needs one more raw-bytes pass. Evidence:
`docs/evidence/vm-leaf-helpers.md`.

Jump-table re-key resolved (2026-09-03): the dispatch is
`JMP [EAX*4 + 0x4413a4]` with `EAX = (signed)opcode + 1` (bounds 0x5d), so
handler `N` is `0x4413a4 + (N+1)*4`. An opcode-by-opcode comparison of the
exported decompile of `0x43ee30` against the C++ `switch` in
`DispatchSetupOpcode` found the numbering already matches the binary
(opcodes 0x00/0x40 and anything above 0x5c share default `0x43f532`). The
earlier divergence report was based on an incomplete table read. Known
remaining body-level item: the four spawn creators
`0x448d00`/`0x448f60`/`0x448e30`/`0x449090` (list registration variants
`0x4489d0`/`0x448a50`/`0x448ac0`/`0x448b40`) are currently collapsed into
one `CreateTimelineObject` model. Evidence:
`docs/evidence/dispatch-setup-opcode-mapping.md`.

## 2026-09-03 Session Note (IDA MCP restored)

IDA (idalib) now opens `resources/th10.exe` with the existing `.i64`
session (`th10`); decompile-by-address cross-checks Ghidra cleanly.

Added two missing list-registration front-insertion leaves (verified
against both Ghidra and IDA):

- `src/EntityHelpers.cpp/.hpp` — `0x00448a50`
  `LinkEntityFrontAndAssignIdEaxEsiAbi`: list-A prepend twin of
  `0x4489d0` (head `+0x72dad4`; tail `+0x72dad8` touched only when empty).
- `src/StageEffectHelpers.cpp/.hpp` — `0x00448b40`
  `AttachEffectVmToListBFront`: list-B prepend twin of `0x448ac0` (head
  `+0x72dadc`; tail `+0x72dae0`).

Both share the node layout `entity+4 = {entity, next, prev}` and the id
counter at `manager+0x732454` (wrap past zero to 1). Evidence/status/CSV
updated; VC and g++ baselines stay green.

Spawn-creator resolution (2026-09-03): the four setup-spawn creators are
now implemented and wired — `0x448d00`→`SpawnSetupEffectVmListABack`,
`0x448e30`→`SpawnSetupEffectVmListAFront`, `0x448f60`→
`SpawnSetupEffectVmListBBack`, `0x449090`→`SpawnSetupEffectVmListBFront`
(`src/TimelineRenderObjects.cpp`, shared `SpawnSetupEffectVm` helper).
Disassembly proves each does pool-VM alloc (manager = global
`DAT_00491c10`) + kind/flag publish + script bind + list registration;
the native first stack arg is dead. `DispatchSetupOpcode` opcodes
`0x58/0x5a/0x5b/0x5c` now route A-back/B-back/A-front/B-front as in the
binary.

Record-interpreter attribution resolved (2026-09-03): the binary
`0x40bd80` opcodes 8/15/16/17 call `0x448d00` (A-back pool spawn) with
`(dead arg, script = record+0xc, kind = 0xf)`. The C++ record interpreter
now calls `SpawnSetupEffectVmListABack`, and the mis-attributed
`CreateTimelineObject` (owner-node + manager-work clone) was removed.

Open conflict to audit next: none — rechecked 2026-09-03 that `0x04000000`
and `0x4000000` are the same bit 26, so `EntityHelpers::ReleaseEntityById`
and the `TimelineRenderObjects` release path both match the live `0x4492a0`;
`0x449630` is the slot-release wrapper. See
`docs/evidence/owner-list-registration-audit.md`.

## 2026-09-05 Session: Coverage Gap Sweep

Coverage inventory added: `docs/coverage-gap.csv` (all uncovered IDB
functions) and `docs/coverage-gap-shortlist.md`. The IDB has 2046
functions; `config/function-status.csv` tracks 318 — roughly 837
non-trivial functions remain. Verified gap classification against the
CSV by hand (an earlier subagent pass mislabeled several covered
functions).

Implemented this session (all baselines green: `compile-main-chain-cpp.sh`,
g++ -m32 -std=c++98 syntax check, `git diff --check`):

- `src/JoystickConfigPoll.cpp/.hpp` — `0x0044a190`
  `PollJoystickConfigBitmaskEcxStackAbi`: winmm `joyGetPosEx` path over
  the 0x6a-stride config bank at 0x474e30 (button words +0x58/+0x5a/
  +0x5c/+0x5e/+0x68, caps at 0x4918b8 stride 0x194, quarter-range axis
  deadzones) and the DirectInput path (0x491ff4 bit 0x400; Poll +0x64,
  Acquire +0x1c with the 0x190-retry NOTACQUIRED loop that never re-reads
  state, GetDeviceState 0x110 with buttons at +0x30, signed deadzone
  words 0x491d5e/60). Failure exits are native `mov ax, si` over the
  failing API result — modeled exactly.
- `src/EclScriptLibrary.cpp/.hpp` — `0x0040cfb0`
  `CreateEclScriptObjectEaxStackAbi`: builds the 0x2518-byte
  vtable-0x46d0c0 ECL script object (descriptor copies, difficulty
  bitmask `1 << [0x474c74]` at +0x1024, name copy, score-anim block
  +0x2458.., kind remap via flag bit 0x8000, owner list append at
  +0x116c with the `last->next` fix-up quirk). Callees 0x40d830/0x40dc80
  are named boundaries. Discovered via the `../../data/*.ecl` enumerator
  at 0x40a450.
- `src/TextureDilateFilter.cpp/.hpp` — `0x004465b0`
  `DilateTextureTransparentPixelsEaxAbi`: fills transparent pixels with
  the channel average of adjacent opaque neighbors over A8R8G8B8 /
  A1R5G5B5 / A4R4G4B4 (27-case format switch, only four bodies), COM
  vtable offsets preserved (GetSurfaceLevel +0x48, GetDesc +0x30,
  LockRect +0x34, UnlockRect +0x38, Release +0x08), count-gated
  averaging.
- `src/ResultScreenDigits.cpp/.hpp` — `0x0042f8b0`
  `UpdateResultScreenStatDigitsEaxAbi`: refreshes twenty result-screen
  glyph VMs (child kinds 67..86 of the +0x2cc parent) from the five u16
  stat fields at +0x59cc..+0x59d4 via `InitializeAsciiAnimationVmEntry`
  with entry `digit + 51` and resource `child+0x308`; native slot-clear
  and null-parent walk quirks preserved.
- Subagent: `0x0044c350` = `EasingCurveSelectorEaxStackAbi`
  (src/VmLeafHelpers.cpp) — the shared easing-curve selector; also
  corrected `TickVec3Interpolator`'s inline easing (modes 9-14 were
  missing; 0xf/0x10 were swapped).
- Subagent: `0x00417c80` = `TeardownTitleScreenStackAbi`
  (src/TitleGameManagerLifecycle.cpp) — full title-screen teardown:
  scoreth10.dat save, mode-flag restore, shared-status dispatch (10/11/
  4/15/13 continuation geometry), manager-reuse vs destructor sweep, and
  the scheduler-record/BGM/gate/clear-color epilogue.
- Subagent: `0x00429b60` = `CommitReplaySaveEcxDxStackAbi`
  (src/ReplaySave.cpp) — replay file writer: name padding, `replay/`
  mkdir, header/stage-record/frame serialization, LZSS + two scramble
  passes, locked file write, and the two 'USER' text chunks; short-write
  and unsigned-length quirks preserved.
- Direct: `0x00402440` = `DestroyTitleScreenStateBufferInPlace`
  (src/TitleGameManagerLifecycle.cpp) — the 0x2a78-byte title-screen
  state destructor: three locked scheduler-record removals, three CRT
  buffer frees (double-clear of +0x2a44 preserved), render-owner large
  slot release gated on mode flag bit 0, primary/secondary global
  clears, and the two eh vector destructor iterator arrays (3 and 8
  records of 0x3ac at +0x1f08/+0x180 via the 0x401ff0 scalar dtor).

In-flight: `0x00415e90` (result-screen text presenter, 4306 bytes) is
being reconstructed by a subagent.

Recommended next wave: `0x42f540`/`0x430250` (result-screen update
paths calling the digits presenter), `0x0042b1e0` score save (EBX ABI),
`0x40a450` ECL enumerator, `0x446b70/0x446c70/0x446d70/0x446eb0` dilate
adapter variants, and then the winmm/DirectInput key-config screens
`0x44a5f0`/`0x44a9d0`/`0x44ad30`.

## 2026-09-05 Session Addendum

- `src/ResultScreenScript.cpp/.hpp` (subagent) — `0x00415e90`
  `RunResultScreenScriptStreamStackAbi`: the result/spell-practice screen
  script stream executor (24 opcodes: two-line decrypted text, per-char
  glyph setup, effect spawns, BGM/text waits, spell-capture bookkeeping
  with difficulty-scaled score adds and state transitions). Locally
  reconstructed 0x449670/0x4496a0 glyph helpers; boundaries 0x409d90,
  0x423370, 0x4175e0.
- `src/TitleGameManagerLifecycle.cpp` — `0x004145f0`
  `DestroyAsciiHudOwnerInPlace` (direct): the DAT_0047770c HUD owner
  destructor — sub-block cleanup, two locked scheduler-record removals,
  soft entity releases over 8 ids with child-chain 0x4000000 propagation,
  the 0x4493e0 timeline-slot boundary, CRT buffer free, and the six
  0x3ac-record eh vector dtor arrays matching the 0x415800 HUD batch
  pools.
- `src/EclSelectMenu.cpp/.hpp` (direct) — `0x0040a450`
  `UpdateEclSelectMenuStackAbi`: the spell-practice/ECL-select menu state
  machine (0 enumerate ../../data/*.ecl, 1 cursors with the spell-select
  exit feeding CreateEclScriptObjectEaxStackAbi and the first-cursor exit
  starting the 0x40a340 continuation worker, 2 pending transition 3,
  4 input snapshot + scheduler-record disable). All ESI/EAX/EDI manager
  ABIs encoded as explicit-argument boundaries (0x40a010, 0x424d90,
  0x409e20, 0x405ed0, 0x40d510, 0x413bc0, 0x4148e0, 0x425090, 0x406140,
  0x405730, 0x41afb0, 0x42b6d0, 0x40d730, 0x409eb0, 0x40ac20).
- Subagent: `0x0042b1e0` = `SaveScoreRecordFileEbx` (src/ScoreSave.cpp) —
  scoreth10.dat writer (checksum/stage-index rewrite, 0x200000 image,
  LZSS + 0x44b220 scramble, header + packed body through 0x44b620);
  TeardownTitleScreenStackAbi now calls the semantic body.

Function-status CSV: 314 implemented rows. All baselines green.
Next wave: 0x42f540/0x430250 result-screen paths, key-config screens
0x44a5f0/0x44a9d0/0x44ad30, the ESI-boundary cluster named above
(0x424d90, 0x405ed0, 0x40a010, 0x4148e0, ...), 0x4493e0, 0x449670/0x4496a0.

## 2026-09-05 Session Addendum 2

- `src/ManagerReleaseWrappers.cpp/.hpp` (direct) — the nine-member
  "release wrapper" family: 0x004148e0/0x00425090/0x00406140/0x00405730/
  0x0041afb0/0x0042b6d0/0x0040d730 (ESI null-check + destructor + shared
  delete), 0x0040d510 (twin of 0x409e20 record re-enable), and
  0x00409eb0 (ECL menu filename-array free with the double-clear quirk).
  Their EclSelectMenu boundaries now call the semantic bodies.
- Direct leaves feeding that menu: 0x0040a010
  `EnableOptionRecordsAndRebuildEaxAbi` (re-enable records +
  RebuildPlayerOptionRecords tail call), 0x00409e20
  `EnableManagerSchedulerRecordsEaxAbi`, 0x0040ac20
  `AdvanceInputBankTriggersEcxAbi` (16 u16 repeat counters, threshold 26,
  subtract-8 wrap, pressed/released words), and 0x004493e0
  `ReleaseEntitiesUsingResourceEaxEdxAbi` in src/EntityHelpers.cpp
  (resource-keyed 0x4000000 release walk; its 0x405ed0 wrapper passes
  caller-garbage ECX and stays a boundary, documented).
- Subagent: 0x0042f540 `UpdateResultScreenStateMachineEbxAbi` and
  0x00430250 `SetResultScreenStatFieldEaxEdxEcxAbi`
  (src/ResultScreenUpdate.cpp) — the result-screen state machine and the
  per-field stat setter driving 0x42f8b0.
- Subagent: 0x00415e90 second pass completed with raw-disassembly
  corrections (opcode 7/8 position publication, 0x14 flag ordering,
  power gauge word view).

CSV now tracks 339 implemented functions. All baselines green.
In flight: key-config screens 0x44a5f0/0x44a9d0/0x44ad30 (subagent).

## 2026-09-05 Session Addendum 3

- `src/JoystickConfigPoll.cpp` — 0x0044a4e0 `PollJoystickButtonBytesEcxEsiAbi`
  (direct): refreshes the 0x497bb0 224-byte button array (DirectInput
  poll/state-copy or winmm button-bit path) for the key-config screens.
- `src/TitleScoreAnimTriggers.cpp/.hpp` (direct) — 0x00404530
  `TriggerTitleScoreAnim30EsiAbi` and 0x004045b0
  `TriggerTitleScoreAnim60EaxAbi`: kind-2 overlay context creation and the
  30/60-frame score-anim arms over the title-screen state block
  (a1[2694..2699] = +0x2a18..+0x2a2c).
- Subagent: key-config screens (src/KeyConfigScreens.cpp) — 0x0044a5f0
  `UpdateKeyConfigInputRecordEcxAbi`, 0x0044a9d0 (byte-twin, joystick
  poll hardcoded to slot 0), 0x0044ad30 (mask-only menu variant): full
  keyboard/joystick sampling into the 0x6a-stride records with the 16
  repeat counters and pressed/released edges.
- Subagent: 0x0042f540/0x00430250 (src/ResultScreenUpdate.cpp) — result
  screen state machine and stat setter.

CSV now tracks 339 implemented functions. All baselines green.
In flight: 0x00413bc0 ASCII HUD overlay update (subagent).
Next wave: 0x00418190 game-manager state body, 0x0042a450, 0x00417870,
0x00409d90/0x00423370/0x004175e0 boundaries, 0x0044bea0 callers.

## 2026-09-05 Session Addendum 4

- Direct: 0x00409d90 `AddScoreBlockValueEcxStackAbi` (value/10 add with
  the 999999999 post-store clamp) and 0x004175e0 `UpdateScoreBlockEaxAbi`
  (+0x3c slot advance capped at 7, pointer publish to DAT_00477848) —
  both in src/ResultScreenScript.cpp, eliminating two executor
  boundaries.
- Direct: 0x00423370 `RecordSpellPracticeCaptureEdiAbi`
  (src/ResultScreenScript.cpp) — the spell-practice capture entry, fully
  corrected against raw disassembly (VM ids to +0x1d8, HUD +0x9ec8 to
  +0x2c4, native ECX input never read).
- Subagent: 0x00413bc0 `ResetAsciiHudOverlayEdiAbi`
  (src/AsciiHudOverlayUpdate.cpp) — the HUD overlay re-arm: scheduler
  record disable, two permanent background VM respawns, the full glyph
  pool script-id rebinding map, life-slot refresh from DAT_00474c70, and
  the mode-gated overlay VM spawns.
- Subagent: key-config screens landed earlier (src/KeyConfigScreens.cpp).

CSV now tracks 339 implemented functions. All baselines green.
In flight: 0x00418190 game-manager state body (subagent).

## 2026-09-05 Session Addendum 5

- Direct: 0x00401ff0 `DestroyTitleScreenVmRecordInPlace`
  (src/ManagerReleaseWrappers.cpp) — the scalar VM-record dtor used by
  the eh vector destructor iterators.
- Direct: 0x00447810 `ReleaseLargeRenderOwnerSlotEdiAbi`
  (src/ManagerReleaseWrappers.cpp) — the large render-owner slot release
  (resource-matched entity sweep, 16-byte-stride COM entry array, five
  CRT-freed buffers).
- Subagent: 0x00418190 `RunTitleScreenCalcBodyStackAbi`
  (src/TitleScreenCalcBody.cpp) — the title-screen calculation-record
  body (state 0 idle/reset paths, state 30 gated reset, common epilogue).
- Subagent batch in flight: the title calc boundary cluster
  0x424d90/0x42a450/0x417040/0x404450/0x417770/0x418a00/0x409f90.

## 2026-09-05 Session Addendum 6

- Subagent: title calc boundary cluster landed — src/TitleCalcCluster.cpp
  implements 0x00424d90 `ResetOptionPositionRecordsEsiAbi`, 0x0042a450
  `ApplyOptionPositionStateEbxAbi` (now correctly fed DAT_00477838),
  0x00417040 `UpdateInGameScoreDisplayEsiAbi` (with the documented dead
  0x448d00 spawn), 0x00404450 `InitializeTitleSecondaryStateStackAbi`,
  0x00417770 `ReleaseOwnerRecordChainEaxAbi`, 0x00418a00
  `TickTitleFrameStateEaxAbi`, 0x00409f90
  `ReleaseAsciiHudConditionalState`, plus file-local 0x004188a0
  `AwardExtendedLifeEaxEcxAbi`.
- Direct: 0x00401ff0 `DestroyTitleScreenVmRecordInPlace` and 0x00447810
  `ReleaseLargeRenderOwnerSlotEdiAbi` (src/ManagerReleaseWrappers.cpp).
- Direct: 0x00449670/0x004496a0 glyph wrappers recorded (implemented in
  src/ResultScreenScript.cpp, now named in IDA and the CSV).

CSV now tracks 352 implemented functions. All baselines green.
In flight: ECL object ctor pair 0x0040d830/0x0040cc70/0x0040dc80
(subagent), manager destructor family 0x424ed0/0x405f70/0x405620/
0x41adf0/0x42b570/0x40d530 (subagent).

## 2026-09-05 Session Addendum 7

- Direct: game-mode record helpers in src/TitleCalcCluster.cpp —
  0x00428e10 `PublishSelectedRunStatsEaxEcxAbi` (EAX = 0x477834 manager,
  ECX = slot+0x24), 0x0042ab20 `FreeGameModeChainEntriesEaxEcxAbi`
  (EAX = bucket index from DAT_00474c7c, ECX = game-mode object), and
  0x0042aa50 `AllocateGameModeChainEntryEsiStackAbi` (ESI = index, stack
  = object). The old Call42AB20/Call42AA50/Call428E10 boundaries in
  ApplyOptionPositionStateEbxAbi are gone; call sites now pass the
  native register values (slot index from DAT_00474c7c).

## 2026-09-05 Session Addendum 8

- Direct: src/ManagerCreation.cpp/.hpp — 0x00406060
  `CreateEffectManagerRoot` (0x3e0b54 object, eh-vector-construct then
  full-wipe order quirk, loader 0x405e20, failure destroy) and 0x00414830
  `CreateAsciiHudOwner` (0x9ed0 object, ctor 0x413810, loader 0x413980).

- Direct: 0x00425020 `CreatePlayerStateBlock` (src/ManagerCreation.cpp) —
  0x4478 player state block with ctor 0x4246c0 and the semantic
  InitializePlayerObject init.

## Exact-Match Baseline (objdiff)

The semantic layer is complete (every enumerated game function >= 0x100
bytes in 0x401000-0x452000 is reconstructed; `docs/coverage-gap-shortlist.md`
is marked COMPLETE). The next phase is object-level matching:

- `scripts/objdiff-baseline.sh` builds the whole baseline end to end:
  1. compiles all modules to `build/cpp/<Name>.obj` (MSVC, per
     `scripts/compile-main-chain-cpp.sh`),
  2. dumps per-object disassembly and COFF symbol tables to
     `build/ours/<Name>.asm/.sym` (the COFF symbols carry the MSVC-mangled
     semantic names, e.g. `?AddString@AsciiManager@th10@@...`),
  3. extracts per-function reference disassembly/bytes of
     `resources/th10.exe` into `build/reference/<address>_<name>.asm/.bin`
     via `scripts/extract_reference_functions.py` (function boundaries are
     derived from the int3 padding runs of the MSVC layout; implemented
     addresses without padding become intra-group splits, so every
     registered address gets its own artifact — 572 unique addresses as of
     this writing),
  4. regenerates `objdiff.yml` (one unit per reconstruction module, source
     -> object).
- Per-function diffing compares `build/ours/<Module>.asm` (locate the
  semantic body through the `.sym` symbol table) against
  `build/reference/<address>_<name>.asm`.
- The IDA MCP server was unavailable for the final reconstruction passes;
  those modules were built from objdump disassembly and the reference
  extraction doubles as the cross-check surface when IDA returns.
