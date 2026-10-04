# Round 11 evidence — StageObjectManager (0x45c), stage objects A/B
# (0xd58/0xd74), BulletManager (0x21CEC0)

**IDA note:** the idalib server restarted mid-round (all sessions dropped).
Session `th10ref` was re-opened on the same database
`resources/th10.exe.i64` (binary `resources/th10.exe`, imagebase 0x400000);
the round-10 type library survived (VmRecord 0x3ac / TimerNode 0x14 /
ChainElem 0x24 verified present before the new declarations).

## 1. New headers (all sizes MSVC-offsetof asserted, smoke test exit 0)

- `src/BulletManager.hpp` — BulletSlot 0x3f0 (VmRecord 0x3ac at +0;
  position_x/y/z_03ac/b0/b4; velocity_x/y/z_03b8/bc/c0;
  timer_tail_zero_03c4; timer_03c8 = TimerNode; state_03dc; kind_03e0;
  bound_kind_03e4; speed_03e8; spawn_delay_03ec) and BulletManager 0x21CEC0
  (flags_0000 bit 1, calc_element@8 prio 0x15 / draw_element@c prio 0x19 —
  native 0x41ad90 uses edi 0x15/0x19 —, slots[2198]@0x14 ending 0x21CEB4,
  live_count_21ceb4, ring_cursor_21ceb8, spawn_counter_21cebc).
- `src/StageObjectObject.hpp` — StageObjectHeader 0x424 (callback table,
  list prev/next, state_000c, TimerNode timer_0010, position/velocity
  triples 0x24/0x30, angle_003c, depth_0040, alpha_0044, zspeed_0048,
  zvel_004c, done_latch_0050 u8, spawn_id_0054, raw_0058[0x10],
  raw_0068[0x39c] (feature records, kept RAW), feature_flags_0404,
  cutoff_kind_040c, TimerNode entrance_timer_0410);
  StageObjectKindADescriptor 0x1dc (spawn pos, angle_000c, depth_0014,
  alpha_001c, zspeed_0020, script_slot/kind u16s 0x24/0x26, flags_0028);
  StageObjectKindBDescriptor 0x1f8 (LIVE image: velocity 0x0c..0x14,
  angle_0018, angle_rate_001c, depth_clamp_0020, depth_0024,
  alpha_target_0028, zspeed_002c ctor 8.0f, entrance timers 0x30..0x3c,
  script slot/kind u16s 0x40/0x42, flags_0044 latch byte);
  StageObjectKindA 0xd58 (header + descriptor_0424 + vm1_0600 + vm2_09ac);
  StageObjectKindB 0xd74 (header + descriptor_0424 + vm1_061c + vm2_09c8).
- `src/StageObjectManagerObject.hpp` — StageObjectManager 0x45c
  (calc_element@8 prio 0x13 / draw_element@c prio 0x1b, sentinel header
  @0x10, list_head_0434, node_count_0438 cap 256, spawn_id_cursor_043c,
  tween target 0x440 + broadcast velocity 0x44c, bullet_anm_work_0458).

IDB: declared BulletSlot/BulletManager/StageObjectHeader/
StageObjectKindADescriptor/StageObjectKindBDescriptor/StageObjectKindA/
StageObjectKindB/StageObjectManager; bound 0x47781c → StageObjectManager *,
0x477818 → BulletManager *; comments at both globals; IDB saved.

## 2. Adjudications made this round (all against resources/th10.exe.i64)

1. **Bullet-slot timer block = TimerNode.** 0x41bb00 seeds
   [rec+0x3c8]=0xFFF0BDC1 → -1, [0x3cc]=0, [0x3d0]=0, [0x3d4]=&flt_476F78,
   [0x3d8]|=1 and zeroes [0x3c4]; the update 0x41b807..0x41b84d publishes
   [0x3c8]=[0x3cc] and advances 0x3cc (int) / 0x3d0 (float) off *[0x3d4].
   The old "counter/integer distance/float distance" names were the same
   TimerNode {prev,count,accum,rate,flags} — count/accum double as the
   distance bookkeeping.
2. **0x404f30 ABI pinned.** ESI = record base (0x401de0 memsets 0x3ac at
   ESI, writes ESI+0x340..0x354/+0x3a0/+0x3a1/+0x38a; its ECX argument is
   unused — decompiler artifact), EAX = script id, one stack argument =
   [root+0x3E0B50] (EffectManagerRoot.bullet_resource_3e0b50). Fixes:
   PlayerFrameworkHelpers SpawnExplosionParticleEaxEcxEfxAbi dropped the
   pre-existing `record + 0x14` vm-base bias and now passes the root's
   bullet.anm work (was: the BulletManager itself).
3. **Ring-VM anm-work base = [DAT_0047781C+0x458]** (the stage-object
   manager's bullet.anm work; sites 0x41c8fd/0x41c95a/0x41dabd/0x41e305/
   0x41ed3e/0x41e5fb). Corrects the round-10 note "[DAT_00477818+0x458]" —
   the 0x477818 manager has no modeled +0x458 field (it lands inside
   BulletSlot[1].vm).
4. **0x41bb00 EAX = the manager base itself** (0x41da95/0x41e445/0x41f147
   `mov eax, dword_477818`). The old `*static_cast<void**>(g_BulletManager
   Slot)` idiom dereferenced and passed flags_0000. Fixed in
   StageObjectVtable.cpp and PlayerStageHelpers.cpp:245.
5. **0x41d7c0 vm1-only flags |= 4.** Native raises [vm1+0x35c] bit 2 at
   0x41d802..0x41d811; the vm2 path (0x41d83f `lea eax,[esi+9ACh]` →
   0x41d863 dispatch) writes NO +0xd08 flags. Removed the vm2 flags write
   from SpawnBossDropItemVmsEcxAbi.
6. **0x41ba50 kills four kinds.** state ≠ 0 && kind ∈ {1,4} → respawn 9;
   kind ∈ {0xA,0xB} → respawn 5 (0x41babb `push 5`), same angle -pi/2
   (0xBFC90FDB) and speed 2.2 (0x400CCCCD). The native zeroes the state
   dword ([edi+0x30] with edi = manager+0x3C0 = &slots[0].position_x_03ac
   → record+0x3dc), NOT +0x3ac; the death entity spawns with script 0x189
   (393) from [root+0x3E0B50] (0x41bacc..0x41bad2). TitleBulletUpdate
   gained the missing 10/11 branch and the script-id source now reads
   [g_StageRecordHolder]+0x3e0b50.
7. **0x41c330 out-of-window tail.** `fst [esi+18h]` keeps the unrounded
   float sum in the accumulator; `__ftol2` (0x463b2c) fills the +0x14
   count as a plain dword store (Hex-Rays' `(unsigned __int64)` was an
   artifact). Matches the TimerNode conversion.
8. **Kind A/B sizes closed:** header 0x424 + desc 0x1dc/0x1f8 + 0x3ac × 2
   = 0xd58/0xd74; the manager sentinel is a full StageObjectHeader at
   +0x10 and the creation wipe covers exactly [+0, +0x45c) /
   [+0x14, +0x21CEB4).

## 3. Converted files

StageObjectVtable.cpp (~90 sites; kSobOff* deleted), StageObjectManager.cpp
(12 sites), StageObjectManager.hpp (comments), EffectPoolEntityUpdate.cpp
(TickEffectPoolSlots → BulletSlot views; TickEffectNodeList →
StageObjectManager/StageObjectHeader views), TitleBulletUpdate.cpp (~60
sites; kOff* deleted), TitleBulletUpdate.hpp (comment), PlayerFramework
Helpers.cpp (SpawnExplosionParticleEaxEcxEfxAbi), PlayerStageHelpers.cpp
(deref fix). Deliberately raw: header.raw_0068 overlays (drift 0x8c..0xb4,
shrink 0xf4..0x128, 0x8000 timer 0x15c/0x160, script records 0x460..0x610
which alias the descriptor image and vm1's first 0x10 bytes), the
g_TransitionRoot effect-ring offsets 0x620/0x650/0x680/0x408, the 0x3f0
stage-entity record DAT_00477820 accessors, and all vtable dispatches.

## 4. Build

`bash scripts/compile-main-chain-cpp.sh` → exit 0, 0 errors, 233 objs.
