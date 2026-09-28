# Spell / Bullet Manager Lifecycle (0x408910..0x40ced0)

Module: `src/SpellBulletManagerLifecycle.cpp`. The spell/bullet manager
(DAT_004776f4, g_SpellBulletBase) is a 0x37b0-byte object owning two pools
of 0x3ac-byte VM records (8 at +0x778, 5 at +0x24d8) plus the spell-card
story state at +0x3788/+0x378c and the practice tutorial timeline handle at
+0x774.

- `0x00408910` `InitSpellBulletManagerStackAbi` — native stdcall, one stack
  argument = the manager (ret 4). Constructs the two eh vector record pools
  (8 x 0x3ac at +0x778, 5 x 0x3ac at +0x24d8; scalar ctor 0x402050 / dtor
  0x401ff0 through the 0x45252d eh iterator), clears the +0x3744 flag bit
  0, wipes the whole 0xdec-dword manager (native order quirk: the wipe
  happens after the pool construction), sets bit 1 of the first dword,
  publishes DAT_004776f4 and returns the manager.
- `0x004089c0` `RegisterSpellBulletSchedulerRecordsEbxAbi` — native EBX =
  manager. Registers three scheduler records with the manager as argument:
  calc 0x409220 (the spell-card story-state thunk) at priority 0x16 on the
  calculation chain, draw 0x409230 at priority 0xe and draw 0x409270 at
  priority 0x25, both on the draw chain. Elements land at manager+8 /
  manager+0xc / manager+0x37ac. Then lazily initializes the +0x3734 timer
  record (sentinel -999999, default rate pointer 0x476f78, flag bit 0) and
  hard-resets it to {-1, 0, 0}. Returns 0.
- `0x00408c90` `CreateSpellBulletManager` — native stdcall (stack: ret 8
  with one argument, unused by the body). `operator new(0x37b0)` + init +
  register; on a registration failure destroys (0x408af0), frees and
  returns null; otherwise returns the manager. The native runs the
  registration even for a null allocation (quirk preserved by the same call
  order). The stage-scene setup calls this entry under its boundary alias
  `RunSceneTextSetup` in `src/TitleSceneSetup.cpp`.
- `0x004091c0` `DispatchSpellBulletPoolsEbxAbi` — native EBX = manager.
  When the +0x378c story flag has bit 0 set, dispatches the animation
  render mode over every record of pool A (8 at +0x778) and pool B (5 at
  +0x24d8) against the render owner. Always returns 1.
- `0x00409230` `DispatchSpellBulletPairEcxEcxAbi` — __thiscall ECX =
  manager. When the +0x378c story flag has bit 0 set, dispatches the
  animation render mode for the +0x10 and +0x3bc records. Always returns 1.
- `0x0040cea0` `ReleaseSpellBulletTutorialHandleEsiAbi` — native ESI =
  manager. Raises the +0x378c flag bit 4 (0x10), releases the practice
  tutorial timeline handle at +0x774 (0x4492a0) and clears it.
- `0x0040ced0` `GetSpellBulletStoryFlag3EaxAbi` — native EAX = manager:
  bit 3 (0x8) of the +0x378c story flags.

Thunk adapters 0x409220 (calc, forwards ECX to 0x408d60), 0x409230
(draw, ECX passthrough) and 0x409270 (draw, forwards ECX as EBX to
0x4091c0) are modeled as TH10_FASTCALL functions in the same module.
