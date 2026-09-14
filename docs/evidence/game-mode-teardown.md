# Game-Mode Teardown Cluster

Implemented in `src/GameModeTeardown.cpp/.hpp`. All four entries are
destructor bodies whose callers own the outer allocation and release it
with the shared main-chain delete 0x004524a1 (`FreeMainChainObject`).
Every scheduler-record removal in the cluster is bracketed by the global
scheduler critical section (`DAT_00492274`) and the activity-depth byte
(`DAT_0049231c`), which the reconstruction expresses through
`CallbackSchedulerApi::RemoveSynchronized` guarded by a non-null record
check (the native checks the record before taking the lock).

## TH10 0x004294a0 `DestroyGameModeObjectInPlace`

In-place destructor of the 0x4a4-byte game-mode object published at
`DAT_00477838` (`g_GameModeObject`). Stdcall, one stack argument. Native
order:

1. `+0x14` scratch buffer: freed through the shared delete, **slot not
   cleared** (native quirk, preserved).
2. The eight game-mode chain-entry buckets are unlinked and freed via
   0x0042ab20 (`FreeGameModeChainEntriesEaxEcxAbi`, EAX = bucket index
   0..7, ECX = owner).
3. `+0x18` buffer: freed through the shared delete and cleared.
4. Eight entry-array pointers at `+0x1c`: each freed through the shared
   delete and cleared. The native free call is unconditional (free(0) is
   a no-op); preserved.
5. Scheduler records at `+0x08`, `+0x1cc`, `+0x0c` (in that order):
   removed when non-null. The records themselves are not cleared.
6. `DAT_00477838` is cleared only when it still points at this object.
7. eh vector destructor iterator (0x004525ff) over eight 0x24-byte
   records at `+0xa0`. The scalar destructor 0x0042ac60 (`__thiscall`
   ECX = record, modeled inline as `UnlinkGameModeChainRecordInPlace`)
   unlinks each record through its `+0x1c`/`+0x20` links and clears both.

Existing call sites extern-declare the same native entry as
`DestroyUnknownMainChainObjectInPlace` (MainChainGlobalTeardown.cpp),
`DestroyOpaqueMainChainManagerInPlace` (TitleGameManagerLifecycle.cpp)
and `DestroyDemoParseObject` (TitleGameManagerLifecycle.cpp,
GameManagerStateBodies.cpp); all three are thin aliases of the semantic
body now.

## TH10 0x00422220 `DestroyGameStateObjectInPlace`

In-place destructor of the `DAT_00477830` game-state manager
(`g_GameStateManager`). Native order:

1. Scheduler records at `+0x08` and `+0x0c`.
2. 25 sub-object slots starting at `+0x1ec`: each non-null slot is torn
   down with 0x004294a0 and freed with the shared delete. The slots are
   **not cleared** (native quirk, preserved).
3. The `+0x1dc` word is an entity id. The native inlines 0x004492a0
   here: search the list-A/list-B roots at render-owner `+0x72dad4` /
   `+0x72dadc` for the id, set the `+0x35c` release flag, and (while
   entity `+0x18` is clear) apply the flag to every child in the `+0x14`
   list. That is exactly `ReleaseEntityById(g_MainChainRenderOwner, id)`;
   the id slot is then cleared.
4. `DAT_00477830` is cleared unconditionally.

The native returns zero in EAX, but no caller consumes it (it is passed
around as a plain `void (*)(void *)` destructor pointer), so the body is
void like its siblings.

## TH10 0x00408af0 `DestroySpellBulletBaseInPlace`

EH-scoped C++ destructor of the `DAT_004776f4` spell/bullet base
(`g_SpellBulletBase`; the EH scope table only guards the outer release,
which callers already model). Native order:

1. Entity handles at `+0x768`, `+0x76c`, `+0x770`: each is soft-released
   (0x004492a0) and cleared. The native caches `LeaveCriticalSection` in
   EBP after the first scheduler removal and calls through it for the
   remaining two removals; the register reuse is semantically identical
   to a direct call and is not reproduced.
2. Scheduler records at `+0x08`, `+0x0c`, `+0x37ac`.
3. `DAT_004776f4` cleared.
4. Three eh vector destructor iterators over 0x3ac-byte VM records,
   destroyed in native order by the shared scalar record dtor
   0x00401ff0 (`DestroyTitleScreenVmRecordInPlace`):
   `+0x24d8` x 5, `+0x77c` x 8, `+0x10` x 2.

## TH10 0x0041fb50 `DestroyGlobalLifecycleManagerInPlace`

In-place destructor of the 0x3f0-byte global lifecycle manager published
at `DAT_00477820` (called by 0x0041fd00 and by the global teardown
0x004203f0). The static `TeardownGlobalLifecycleManagerInPlace` in
`GlobalLifecycleManager.cpp` already models this entry with
`ReleaseGlobalLifecycleCoordinatedGlobals` as a boundary; this body
expands that boundary. Native order:

1. Thread stop 0x0044c150 on the embedded ThreadControl at `+0x10` (the
   native passes `manager+0x10` in ESI/EDI).
2. Scheduler records at `+0x08`, `+0x0c`.
3. 0x0041f930 (`ReleaseGlobalLifecycleCoordinatedSlots`): release the
   large render-owner slot words at render-owner `+0x3ad084` and
   `+0x3ad088` — each non-null word's slot block is released through
   0x00447810 (`ReleaseLargeRenderOwnerSlotEdiAbi`), freed with the
   shared delete, and cleared.
4. Slot word at render-owner `+0x3ad070`: same treatment.
5. `DAT_00477820` cleared, then the ASCII manager host (`DAT_004776e0`,
   when non-null) is torn down with 0x00401260 and freed with the shared
   delete.
6. Slot word at render-owner `+0x3ad06c`: same treatment.
7. Score-save record `DAT_0047783c` (`g_TitleScoreSaveRecord`): handed
   to 0x0042b1e0 (`SaveScoreRecordFileEbx`, native EBX), then both
   record buffers (`+0`, `+4`) are CRT-freed (0x00452422) and cleared,
   and the record itself freed with the shared delete.
8. `+0x388` owned buffer: read, the score-record global cleared
   unconditionally, the buffer CRT-freed, slot cleared.
9. Destructed vtable `0x004703e4` planted at `+0x10`, then the thread
   stop 0x0044c150 is issued a second time on the same ThreadControl.

## TH10 0x00401260 `DestroyAsciiManagerHostInPlace`

Base-object in-place destructor of the `DAT_004776e0` ASCII manager host
(called from 0x0041fb50 before the outer free). Native order:

1. Base vtable `off_46cb14` (0x0046cb14) planted at `+0`.
2. Scheduler records at `+0x0c`, `+0x10`, `+0x89a8`.
3. Large render-owner slot words at `+0x3ad074`, `+0x3ad06c`,
   `+0x3ad078`: 0x00447810 + shared delete + clear.
4. `+0x718` buffer: read, the published host global `DAT_004776e0`
   cleared, buffer CRT-freed, slot cleared.
5. `+0x36c` buffer: CRT-freed and cleared.

## Status

`g++ -m32 -std=c++98 -fsyntax-only -I src src/GameModeTeardown.cpp`
passes; the TU was added to `scripts/compile-main-chain-cpp.sh` (line
after ManagerWorkStageAdapters). CSV rows appended for 0x004294a0,
0x00422220, 0x00408af0, 0x0041fb50, 0x0041f930, 0x00401260 and
0x0042ac60.
