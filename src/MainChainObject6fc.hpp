#pragma once

#include <stddef.h>

#include "Th10Types.hpp"

namespace th10 {

struct ChainElem;

// ---------------------------------------------------------------------------
// The main-chain bullet-resource object (TH10 DAT_004776fc, source alias
// g_MainChainObject6fc), 0x18 bytes. Allocated by ::operator new(0x18) in
// CreateMainChainObject6fc (0x0040af90: inline zero of 6 dwords, |= 2,
// publish, then RegisterMainChainObject6fcRecordsEbxAbi 0x0040ae70);
// destroyed in place by DestroyMainChainObject6fcInPlaceEaxAbi (0x0040af00).
// Created from SetupGameSceneFromTitle (0x00417870 @0x417b8b); torn down
// from TeardownTitleScreenStackAbi (0x417fb1).
//
// RunTitleScreenCalcBodyStackAbi (0x418190 @0x418351/0x418551) re-enables the
// two scheduler elements (node+4 |= 2) on title (re)entry, without null
// checks. Both node callbacks (0x40b050 / 0x40b060) just return 1.
// ---------------------------------------------------------------------------

struct MainChainObject6fc {
    u32 flags_0000;               // +0x000 bit 1 = constructed/enabled (set by
                                  //   the creator, never cleared)
    u32 field_0004;               // +0x004 (zeroed, never referenced)
    ChainElem *calc_element;      // +0x008 (calc chain, priority 0x17)
    ChainElem *draw_element;      // +0x00c (draw chain, priority 0x20)
    void *bullet_resource_0010;   // +0x010 RequestManagerWork(7, render owner
                                  //   0x491c10, "bullet.anm" 0x46cd88);
                                  //   failure -> diagnostic 0x46cf74, ret -1
    u32 field_0014;               // +0x014 (zeroed; size padding)
};

typedef char AssertMainChainObject6fcSize[
    sizeof(MainChainObject6fc) == 0x18 ? 1 : -1];
typedef char AssertMainChainObject6fcCalcOffset[
    offsetof(MainChainObject6fc, calc_element) == 0x8 ? 1 : -1];
typedef char AssertMainChainObject6fcResourceOffset[
    offsetof(MainChainObject6fc, bullet_resource_0010) == 0x10 ? 1 : -1];

} // namespace th10
