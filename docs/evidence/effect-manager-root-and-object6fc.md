# Round 9 evidence — MainChainObject6fc (0x18), EffectManagerRoot (0x3e0b54), gap survey

Session: IDA `th10ref` on `/tmp/th10ref.exe` (IDB `/tmp/th10ref.exe.i64`, imagebase 0x400000). Headers: `src/MainChainObject6fc.hpp`, `src/EffectManagerRoot.hpp`; IDB types `MainChainObject6fc` / `EffectManagerRoot` / `EffectTriggerRecord` declared and globals bound (0x4776fc → `MainChainObject6fc*`, 0x4776f0 → `EffectManagerRoot*`). Also converted by hand: `src/SpriteViewDebugText.cpp` (stage-host argument now typed `StageHostObject&`), `src/MainChainObject6fcAndGate.cpp`.

## 1. MainChainObject6fc — TH10 DAT_004776fc, 0x18 bytes

`::operator new(0x18)` at 0x40af93 in **CreateMainChainObject6fc 0x0040af90** (inline zero of 6 dwords, `|= 2`, publish, then RegisterMainChainObject6fcRecordsEbxAbi 0x40ae70); dtor **0x0040af00**. Created from SetupGameSceneFromTitle (0x417870 @0x417b8b); torn down from TeardownTitleScreenStackAbi (0x417fb1). RunTitleScreenCalcBodyStackAbi re-enables both scheduler elements (`node+4 |= 2`, no null checks) at 0x418351/0x418551.

Layout: flags_0000 (bit 1 = constructed, never cleared), +0x8 calc ChainElem (cb 0x40b050 prio 0x17), +0xc draw ChainElem (cb 0x40b060 prio 0x20), +0x10 `bullet_resource` = RequestManagerWork(7, render owner 0x491c10, "bullet.anm" 0x46cd88; failure → diagnostic 0x46cf74, ret −1). **No mismatches vs sources** — conversion only.

## 2. EffectManagerRoot — TH10 DAT_004776f0, 0x3e0b54 bytes

`::operator new(0x3E0B54)` in **CreateEffectManagerRoot 0x00406060**: eh vector ctor iterator builds **2001 records of 0x7f0 at +0x60 first**, then the whole object is memset to 0 (ctor-before-wipe quirk), then published. Called from 0x40a350 and 0x417870. Loader 0x405e20 seeds `pool_self_0010` = root+0x60 and the +0x3e0b50 resource; teardown 0x405ed0; dtor 0x405f70; release 0x406140.

Layout: +0x8/+0xc calc/draw nodes (0x406770 prio 0x14 / 0x4067a0 prio 0x1d, registered disabled); +0x14..0x2b six trigger-group heads, +0x2c..0x43 six tails; +0x5c bound-record count; **records[2001] @+0x60 ends exactly at +0x3e0b50**; +0x3e0b50 = the shared spawn/VM resource (`RequestManagerWork(7, render owner, "bullet.anm")`), passed everywhere as SpawnStageEffectEdxEbxAbi (0x448db0) arg and stored into ConditionalState resource_table[0] by 0x40d285. Aliasing quirk kept raw: the loader seeds the constant u16 5 at root+0x3e07a6 = **records[2000].kind_0446** — the 2001st record is never iterated (all loops run 2000), so its kind word doubles as a marker.

EffectTriggerRecord (0x7f0): flags_0000 (bit 0x4 expiry latch, bit 0x8 activated-in-field), activation_gate_0004, embedded **VmRecord @+0x8** (so record+0x33c/0x340/0x344 = vm.base_pos_x/y/z, record+0x364 = vm.flags, record+0x38c = vm sprite sentinel u16), position triple @+0x3b4, raw_angle_03e4, radius_03f0, frame accum mirror/int/float @+0x3f8..0x404, **rate_ptr_0404 is a pointer** to the rate float (default &g_FrameTimeScale 0x476f78), flags_0408/041c, kind_0446 u16 (0 free / 1,2 pending / 3 active), group_next_044c, group_index_0460. Bind/clear walks also touch ten stride-0x34 flag dwords in +0x624..+0x7c4 — kept raw.

Misattributions corrected: the render-owner slots +0x3ad090..0x3ad09c released by 0x40d530 belong to **g_MainChainRenderOwner (0x491c10)**, and the 2198×0x3f0 bullet-slot array at +0x14 belongs to the **bullet manager DAT_00477818** — neither is part of this root.

## 3. Bugs fixed this round (each re-verified against the native site before applying)

Scene-trigger cluster (agent-verified at the cited addresses):
1. EffectPoolEntityUpdate.cpp `k_effect_position` 0x3b0 → **0x3ac** (payload x at slot+0x3ac; 0x41b917/0x41b923/0x41b928).
2. EffectPoolEntityUpdate.cpp bound-script compares now use init_id+1 (0x158/0x161 vs init 0x157/0x160; 0x41b9b1/0x41b982) — the VM stores bind id + 1.
3. EffectPoolEntityUpdate.cpp faded branch now overwrites vm.base_pos_y = 24.0f before the alpha computation (0x41b93c).
4. EffectPoolEntityUpdate.cpp: the VM-init context passed to 0x43e5a0 is now the typed `root.bullet_resource_3e0b50` (was the bullet manager pointer).
5. SceneTriggerManager.cpp AdvanceRecordFrameAccumulator: +0x404 dereferenced as the rate **pointer** (0x406652/0x406674).
6. SceneTriggerManager.cpp BindEffectTriggerGroupsEcxEcxAbi: clears record+0x44c (group_next), not +0x4c (0x406649).
7. SceneTriggerManager.cpp TickEffectTriggerGroupEcxEaxAbi: angle step is **π/2** (0x3FC90FDB @0x406732), was 1.0f.
8. SceneTriggerManager.cpp InitSceneTriggerRecordEcxAbi (ctor cluster 0x405d00): flag clears use `&= ~1u`, not `&= ~2` (0x405d11–0x405dce).
9. SceneTriggerManager.cpp kFrameWindowLow 0.98f → 0.99f (0x470b68 holds 0x3F7D70A4; 0x4065c0 compares ≤0.99/≥1.01).

Bug-list adjudication (agent-verified):
10. **StageObjectVtable.cpp StageObjectUpdateA (0x41d3d0): real bug** — the first off-playfield probe centers on the object position (+0x24, `lea ecx,[ebp+24h]` @0x41d5b7), not the tip; the reconstruction passed the tip to both calls, wrongly deleting objects whose body was on-field. Fixed to pass `obj + kSobOffPos` for the first probe.
11. **TitleCalcCluster.cpp ReleaseAsciiHudConditionalState site: wrong offset 0x3ACCB0 → 0x3AD090** (native `lea esi,[ebx+3AD090h]` @0x409f9e = work_slots[9..12]); the suspected LargeRenderOwnerLayout conflict does not exist.
12. TitleBulletUpdate.cpp: the mixed comparison `speed > 0.99f && speed >= 1.01f` is **verbatim faithful** (fcomp 0.99f/1.01f @0x41b807-0x41b84d; no [0.99,1.01] window — comment rewritten); π comment fixed (0x3FC90FDB = π/2 @0x41b21d).
13. SpellBulletVtable.cpp: −1.25f comment → −π/2 (0xBFC90FDB @0x4083a3); 0.8f comment → 0.6f (0x3F19999A @0x40839e); values were already correct.
14. EclScriptLibrary.hpp/.cpp: "+0x1044 sub-record" doc → **+0x103c** (native callers push record+0x103c @0x40d0b3/0x40d771).
15. StageObjectVtable.cpp:275 unbounded-kind store documented as a native quirk (0x43dcf0, no clamp).
16. PlayerProximityFade.cpp verify-only: entity is a render-owner node-pool **VmRecord**; +0x12c = alpha_anim_1.duration gate, +0x40/+0x50 = scale_y/height — labels corrected, behavior already faithful.
17. EclEasedTransforms / SpriteViewDebugText / MainChainObject6fcAndGate: SpriteViewDebugText's host-argument accesses typed via StageHostObject (incl. the animation-record recenter through `animation_vm.base_pos_*`); MainChainObject6fcAndGate's object + pooled-VM accesses typed (`flags_0000`, `bullet_resource_0010`, elements, `vm.flags`/`vm.render_kind`).

## 4. Mechanical conversion pass (all gates exit 0)

SaveRunHighScoreEntry.cpp (14 sites, GameStateManager), PauseEnterSetup.cpp (17), PlayerShotHoming.cpp (14; 0x1068/0x2480 ECL-record reads stay raw), GameManagerGateVms.cpp (9, LargeRenderOwnerLayout + MainChainContext), ResultScreenState.cpp (6), ResultScreenDigits.cpp (4), EclScriptNameTable.cpp (7; unbounded file-slot table stays raw).

Scene-trigger cluster: EffectPoolEntityUpdate.cpp, SceneTriggerManager.cpp, SceneTriggerPopup.cpp, SceneTriggerUpdate.cpp, SceneTriggerObject.cpp — root/record accesses typed (records[2000] iteration is in-bounds); kept raw: the u16-5 seed, the +0x60 bulk wipe, +0x3c0 velocity triple, +0x40c timer block, +0x464 queue, +0x624 flag ring, descriptor parsing.

## 5. Gap survey (worklist for future rounds)

Unmodeled objects ranked by site count: ECL script object 0x2518 (vtable 0x46d0c0, working sub-record +0x103c; ~350 sites), scene-trigger record tail + queue regions (~460 incl. SpellBulletVtable/StageObjectVtable overlap), stage object kinds A/B 0xd58/0xd74 (manager 0x47781c; ~180), bullet record 0x3f0×2198 (0x477818), spell/bullet base 0x4776f4 (≥0x37a0), frame-state block 0x474c40 family, score-save record 0x47783c, hint tip record 0x88, scheduler fade record, ending MIDI context, replay-scene-reuse context, game-mode object 0x477838. Mechanical leftovers: EclScriptLibrary (~60 VmRecord sites), EclEasedTransforms (~105 interp-block sites), AsciiHudGameplayUpdate (~25), StageObjectVtable/TitleBulletUpdate BGM-array sites (documented dual-use).
