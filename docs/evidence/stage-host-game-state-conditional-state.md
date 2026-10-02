# Round 8 evidence — StageHostObject (0x688), GameStateManager (0x2c8), ConditionalState (0x68)

Session: IDA `th10ref` on `/tmp/th10ref.exe` (IDB `/tmp/th10ref.exe.i64`, imagebase 0x400000). All native addresses below are TH10 VAs. Headers: `src/StageHostObject.hpp`, `src/GameStateManagerObject.hpp`, `src/ConditionalStateObject.hpp`; IDB types `StageHostObject` / `GameStateManager` / `ConditionalState` / `ConditionalNameRegistry` declared and globals bound (0x4776f8 → `StageHostObject*`, 0x477830 → `GameStateManager*`, 0x477704 → `ConditionalState*`).

## 1. StageHostObject — TH10 DAT_004776f8 (g_StageHostObject), 0x688 bytes

Allocation: `::operator new(0x688)` at 0x40a3c6 in **CreateStageHostObject 0x0040a3c0** (address-taken, no direct xrefs); init + full wipe (`rep stosd`, 0x1a2 dwords) in **InitStageHostObjectEdxAbi 0x0040a040**, which sets flag bit 1 and publishes the global. Composite destructor **DestroyStageHostObjectStackAbi 0x0040a1a0** (stack arg, ret 4).

The global has exactly **3 data xrefs** — 0x40a040 (publish), 0x40a1a0 (clear), 0x40a340 (timeline-continuation thunk calling `EnterGameModeSetupEsiAbi(ESI = g_StageHostObject)`). All other users receive the object in registers.

Layout (all offsets native-verified):
- +0x00 flags (bit 1 at publish); +0x08/+0x0c calc/draw scheduler elements (fn 0x40abe0 prio 5 / fn 0x40abf0 prio 0x27; freed via 0x449f60)
- +0x10 embedded `ThreadControl` (vtable off_4703e4; continuation 0x40a340 registered at 0x40a79d)
- +0x30 state machine (0 init scan / 1 select / 2 entering game / 3 exit cleanup / 4 in game; switches 0x40a46d + 0x40a981)
- +0x34 `char **stage_table` — a **malloc'd array of filename pointers** (`malloc(4*count)` 0x40a801), not a struct table; +0x38 count ("File not found." gate 0x40aa0f)
- +0x3c / +0x114 / +0x1ec — three `ManagerCursorRecord`s (0xd8 each): mode/config select (wrap modulus seeded 999, forced to 3 in state 0 at 0x40a870), stage/ECL select (modulus = file count 0x40a89a), ECL-entry select (modulus seeded from the conditional-state name-registry count at 0x40a396)
- +0x2c4 4-byte gap; +0x2c8 0x3ac embedded animation record (typed as `VmRecord`; 320.0f @+0x5fc, 240.0f @+0x600, dead u16 0xFFFF @+0x64c; nine busy flags cleared bit 0 — native mask 0xFFFFFFFE, e.g. 0x40a0c1)
- +0x674 float position display; +0x678 float 32.0f; +0x67c 0; +0x680 1000; +0x684 byte flags (bit 1 draw-tested 0x40ab77)

Aliasing kept raw: the destructor frees the malloc'd buffer pointer at **host+0x620**, which lies inside animation_vm (VmRecord+0x358) — raw access preserved with comment (0x40a2ee/0x40a303).

`ShiftManagerSelector` (0x44bea0) re-proves the shared cursor record layout (value +0, limit +8, scan table +0x90, wrap +0xd0, count +0xd4), consistent with `ManagerCursorRecord`.

## 2. GameStateManager — TH10 DAT_00477830 (g_GameStateManager / g_ScoreRecordOwner), 0x2c8 bytes

Allocation: `::operator new(0x2c8)` at 0x422366 in **CreateScoreRecordOwner 0x00422360**; defaults + **0x2c8 memset** in **ResetScoreRecordDefaultsEdxAbi 0x004220e0** (the 11 pre-wipe default stores are dead stores natively — quirk preserved in source). Scheduler nodes installed by **InstallReplayNameEntryCallbacksEbxAbi 0x00422150** (calc → +0x8, draw → +0xc, then arms the +0x10 timer). Destructor **DestroyGameStateObjectInPlace 0x00422220** (`__stdcall`, returns 0).

Field map: mode@+0x4 (0 game/idle — render gate `== 0` at 0x426423; 1-5 pause menu; 6-13 post-run/replay-save menus, 6 doubles as game-over & spell-practice scene state), TimerNode@+0x10 (rate → 0x476f78; tick epilogue 0x422450), `ManagerCursorRecord` cursor_a@+0x24 and cursor_b@+0xfc (name-entry; wrap +0x1cc), overlay entity handles +0x1d4/+0x1d8/+0x1dc (only +0x1dc released by the dtor, inlined 0x4492a0 semantics), name_length@+0x1e0, spell_practice_flag@+0x1e4, replay_gate@+0x1e8, `void *parsed_replay_files[25]`@+0x1ec (each torn down 0x4294a0 + freed by dtor and mode-10 cancel path), **+0x250..+0x2b3 unreferenced dead space**, replay_name[12]@+0x2b4, saved_time_scale f32@+0x2c0 (raw copy of 0x476f78, restored 0x422c73/0x423510), front_anm_work@+0x2c4.

Global xrefs (all verified): 0x415e90 (passes to 0x423370), 0x417c80 (teardown + node bit-2 clear), 0x418190 (node bit-2 set), 0x4220e0 (publish), 0x422220 (clear), 0x425730 (game-over path arg), 0x426360 (render gate).

## 3. ConditionalState — TH10 DAT_00477704 (g_AsciiHudConditionalState), 0x68 bytes

Creator **0x0040d6b0** (memset 0x68, |= 2), sub-record initializer **0x0040d280**, destructor **0x0040d530** (EAX), release wrapper **0x0040d730** (ESI), sub-block release / list delete **0x00409f90**.

Field map: calc/draw elements @+8/+0xc (ticker 0x40d810 prio 0x12 / no-op 0x40d820 prio 0x14); **two parallel per-ECL-record index spaces**: `published_ids`@+0x10 indexed by **record+0x248c** (slot 0 = primary stage/battle ECL record pointer, nonzero = active; read 0x405a53, 0x40910b→+0x1068, …) and `resource_table`@+0x30 indexed by **record+0x244c** (entry 0 = effect pool word `*(DAT_004776f0 + 0x3e0b50)`; +0x38 = battle/base resource for spell-practice VM init 0x409964/0x416148/0x416b91). Both tables are indexed **unboundedly** natively (ANIM parser 0x40d447 writes `+0x34+4i` from file-derived counts) → modeled [8] and [4] with raw fallback for out-of-bounds indexes. TimerNode@+0x40 (0x14; first-time seed gated by +0x50 bit 0; tick 0x40d750). name_registry@+0x54 → the **0x1098-byte stage-script name registry** (vtable 0x46d0b4 = {0x450220 RegisterScriptFileNames, 0x40d400 ParseAnimEcliSections, 0x40cd20 LoadStageScriptFile}; destroying vtable 0x46d0f0 planted at teardown): loaded_script_count@+0x4, name_entry_count@+0x8, file_data[32]@+0xc, and at +0x8c a `malloc(8*count)` name table of `{const char *name; void *file_data}` entries kept sorted by strcmp — consumers pass the **name pointer** (not an id) to CreateEclScriptObject. Script list head/tail @+0x58/+0x5c (nodes at record+0x116c/0x1170/0x1174), script_count@+0x60 (public "stage script running" gate 0x425172/0x42626e), aux_count@+0x64 (incremented with +0x60, **never decremented** — native quirk).

Vtable adjudication: live ECL-record vtable 0x46d0c0 = {0x40e760, 0x411fc0, 0x412340, 0x412350, 0x4126d0, **0x40cc50**} — slot +0x14 is the scalar deleting destructor (0x40cc50), so the 0x409f90 list walk *deletes* every ECL script object. 0x40c5e0 is slot 0 of the destruction vtable 0x46d0d8 planted by 0x40dae0; the ticker 0x40d750's direct 0x40c5e0 notify call is unaffected.

## 4. Bugs fixed this round

1. **StageEffectHost.cpp busy-flag mask**: reconstruction cleared the nine embedded flags with `&= ~2u`; native is `&= ~1u` (mask 0xFFFFFFFE, bit 0; `mov eax,0FFFFFFFEh` at 0x40a0c1). Fixed with TH10 address comment.
2. **ResultScreenScript.cpp RecordSpellPracticeCaptureEdiAbi (0x423370)**: native stores the second overlay VM's entity id (script 129) to manager+0x1d4 at 0x42349f; the reconstruction discarded it. Fixed → `mgr.handle_a_01d4` (sibling game-over path 0x4231d0 does the same natively).
3. **EclEasedTransforms.cpp** (0x40d830 constructor site): old expression double-dereferenced `g_AsciiHudConditionalState` (`LoadU32(*(u8*const*)…)`, reading the state's first dword as a pointer) where the native does a single deref of state+0x54 at 0x40da0d. Fixed by the typed `name_registry_0054` read.
4. **EclInstructionHelpers.cpp comments**: the "effect spawn host" claim is unsupported — TickEffectSpawnWaitListEbpStackAbi (0x40e6a0) never reads DAT_004776f8; 0x448db0's contract is EDI = vec3 position (scroll-added 0x470b4c/0x470b48), ECX = VM/record, EAX = out-id slot, EBX = script index; at 0x40e6ef the ECX arg is `g_AsciiHudConditionalState + 0x30 + 4*[node+0x244c]`. Comments corrected, no code change.
5. **MenuStateHelpers.cpp attribution** (0x434a80): the +0x70..+0xb8 vector-tween block belongs to the **0x3ac VmRecord's position_anim region**, not the game-manager record — both call sites (0x43427b/0x434600 in RunManagerStateBodyE 0x433ef0) pass EAX = a text-slot entity and EDX = `&vm->base_pos_x` (+0x334), and the `|Δy| >= 40` fallback branch writes base_pos directly. Parameter renamed `manager` → `vm_record`; comments corrected.
6. **ConditionalStateSubrecords.cpp vtable comment**: 0x46d0c0 slot +0x14 = 0x40cc50 (was misattributed to 0x40c5e0; see above).
7. **StageEffectHost.cpp EnterGameModeSetupEsiAbi doc comment**: said "Native ESI = the game manager"; native 0x40a340 passes ESI = g_StageHostObject and every offset matches the host map. Fixed.

## 5. Deliberate raw sites remaining (documented, not converted)

- StageHostObject: +0x620 malloc buffer pointer (aliases animation_vm / VmRecord+0x358); the 0x1a2-dword creator wipe and the busy-flag loop table.
- ConditionalState: published-id clears indexed by record+0x248c (unbounded), ANIM-parser resource_table stores (unbounded), name-table entry reads at `select*8` (menu-cursor index), the 0x1098 registry body beyond the modeled prefix.
- GameStateManager: none — fully modeled except unknown_0250 (dead space).

## 6. Files converted this round

StageEffectHost.cpp, StageHostTeardown.cpp, EclInstructionHelpers.cpp (comments), PauseMenuModes.cpp, ResultScreenScript.cpp, GameModeTeardown.cpp, MenuRecordHelpers.cpp, AsciiSceneObjectRenderer.cpp, ConditionalStateSubrecords.cpp, StageConditionalState.cpp, ManagerReleaseWrappers.cpp, EclScriptObjectTeardown.cpp, AsciiHudRenderer.cpp, EclSelectMenu.cpp, EclEasedTransforms.cpp, StageObjectVtable.cpp, SpellBulletVtable.cpp, PlayerDamageOutput.cpp, SpriteViewDebugText.cpp, MenuStateHelpers.cpp — every per-file `th10_cl` gate exit 0; full build green (232 objs).
