# ASCII HUD Owner Destructor (TH10 0x004145f0)

Implemented as `DestroyAsciiHudOwnerInPlace` in
`src/TitleGameManagerLifecycle.cpp` (previously an extern boundary; the
0x417c80 teardown path calls the semantic body directly).

## ABI

- Native stdcall, one stack argument (`ret 4`): the ASCII HUD owner object
  published at `DAT_0047770c` (offsets beyond +0x9e90).

## Body, in native order

1. `CleanupAsciiHudOwnerSubBlocksStackAbi` boundary for 0x00414370 (one
   stack argument).
2. Scheduler records at `+0x08` / `+0x0c`: removed via
   `CallbackSchedulerApi::Remove` under the scheduler critical section and
   the activity-depth byte; the `+0x08` slot is then cleared.
3. `ReleaseEntityById(g_MainChainRenderOwner, owner[10132])` (0x4492a0)
   for the id slot at `+0x9e90`; slot cleared.
4. Eight entity ids at dword index 10109 (`+0x9da4`): each non-zero id is
   resolved with `FindEntityEdxStackAbi` (native inlines the list-A then
   list-B walk over `manager+0x72dad4` / `+0x72dadc`). The found entity
   gets `+0x35c |= 0x4000000`; when its `+0x18` child count is zero the
   same flag is applied over the `+0x14` child chain. Every slot is
   cleared afterwards. The last found entity (zero when an iteration had
   no id or no match) is carried into step 5.
5. Boundary `ReleaseAsciiHudOwnerTimelineSlotEaxStackAbi` for 0x004493e0,
   native EAX = the step-4 entity, stack = the u32 at `+0x9e88`.
6. `g_AsciiHudOwner` (DAT_0047770c) is cleared, then the buffer at
   `+0x9d60` is freed through the CRT free (0x452422) and the slot
   cleared.
7. Six eh vector destructor iterators over the HUD glyph VM pools — 7
   records at `+0x8094`, 2 at `+0x793c`, 4 at `+0x6a8c`, 9 at `+0x4980`,
   10 at `+0x24c8`, 10 at `+0x10` — all 0x3ac-byte records with the
   scalar dtor 0x00401ff0 (`DestroyTitleScreenVmRecordInPlace` boundary).
   These pools match the batch dispatch of `RenderAsciiHudBatch`
   (0x415800).

## Status

Baselines pass (`scripts/compile-main-chain-cpp.sh`, g++ -m32 -std=c++98
syntax check, `git diff --check`). CSV row appended.
