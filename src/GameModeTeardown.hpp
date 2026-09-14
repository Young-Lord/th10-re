#pragma once

#include "Th10Types.hpp"

namespace th10 {

// Game-mode / manager teardown cluster (destructor bodies whose callers own
// the outer allocation with the shared main-chain delete 0x004524a1).

// TH10 0x004294a0. In-place destructor of the 0x4a4-byte game-mode object
// published at DAT_00477838. Frees the two scratch buffers and the eight
// entry arrays, removes the three scheduler records (+8/+0x1cc/+0xc) under
// the scheduler lock, clears the published global when it still points at
// this object, and runs the eh vector destructor iterator over eight
// 0x24-byte chain records at +0xa0.
void DestroyGameModeObjectInPlace(void *object);

// Boundary aliases: existing call sites extern-declare 0x004294a0 under
// these names; all of them bind to the same native entry.
void DestroyUnknownMainChainObjectInPlace(void *object);
void DestroyOpaqueMainChainManagerInPlace(void *object);
void DestroyDemoParseObject(void *parsed);

// TH10 0x00422220. In-place destructor of the DAT_00477830 game-state
// manager: removes the two scheduler records at +8/+0xc, tears down and
// frees the 25 game-mode sub-objects at +0x1ec, soft-releases the entity
// handle published at +0x1dc, and clears the manager global.
void DestroyGameStateObjectInPlace(void *object);

// TH10 0x00408af0. In-place destructor of the DAT_004776f4 spell/bullet
// base: soft-releases the three entity handles at +0x768/+0x76c/+0x770,
// removes the scheduler records at +8/+0xc/+0x37ac, clears the base global,
// and runs three eh vector destructor iterators over 0x3ac-byte VM records.
void DestroySpellBulletBaseInPlace(void *object);

// TH10 0x0041f930. Releases the two large render-owner slot/buffer words at
// render-owner +0x3ad084 and +0x3ad088 (slot contents, slot block, word).
void ReleaseGlobalLifecycleCoordinatedSlots();

// TH10 0x00401260. Base-object in-place destructor of the DAT_004776e0
// ASCII manager host: plants the base vtable 0x0046cb14, removes the three
// scheduler records at +0xc/+0x10/+0x89a8, releases the three large
// render-owner slots at +0x3ad074/+0x3ad06c/+0x3ad078, and frees the two
// buffers at +0x718 and +0x36c.
void DestroyAsciiManagerHostInPlace(void *host);

// TH10 0x0041fb50. In-place destructor of the 0x3f0-byte global lifecycle
// manager published at DAT_00477820 (the owner modelled by the static
// TeardownGlobalLifecycleManagerInPlace in GlobalLifecycleManager.cpp,
// whose ReleaseGlobalLifecycleCoordinatedGlobals boundary is expanded
// here). Stops the coordinated thread, removes the two scheduler records,
// releases the coordinated render-owner slots, tears down and frees the
// ASCII manager host, flushes and frees the score-save record, frees the
// +0x388 owned buffer, plants the destructed vtable 0x004703e4, and stops
// the thread a second time.
void DestroyGlobalLifecycleManagerInPlace(void *manager);

} // namespace th10
