#pragma once

#include <stddef.h>

#include "Th10Platform.hpp"

namespace th10 {

// TH10 callback node allocated by 0x00449ed0. The scheduler writes arg into
// ECX immediately before calling callback, so all callback targets use the
// one-register __fastcall form in this reconstruction.
typedef i32 (TH10_FASTCALL *ChainCallback)(void *arg);
typedef i32 (TH10_FASTCALL *ChainLifetimeCallback)(void *arg);

struct ChainElem;

struct ChainLink {
    ChainElem *owner;
    ChainLink *next;
    ChainLink *previous;
};

struct ChainElem {
    i32 priority;
    u32 flags;
    ChainCallback callback;
    ChainLifetimeCallback registration_hook;
    ChainLifetimeCallback calculation_followup;
    ChainLink link;
    void *arg;
};

// TH10 0x00449aa0 initializes two adjacent ChainElem sentinels. Active
// calculation and draw records are linked through their respective sentinels.
struct CallbackScheduler {
    ChainElem calculation_sentinel;
    ChainElem draw_sentinel;
};

// TH10 0x449aa0 semantic body. The native constructor deliberately does not
// initialize either sentinel's argument field or all flag bits.
CallbackScheduler *ConstructCallbackScheduler(void *storage);
// TH10 0x438880 in-place lifecycle teardown. The caller owns the outer free.
void DestroyCallbackSchedulerInPlace(CallbackScheduler *scheduler);

typedef char AssertChainElemSize[sizeof(ChainElem) == 0x24 ? 1 : -1];
typedef char AssertChainElemFlagsOffset[
    offsetof(ChainElem, flags) == 0x4 ? 1 : -1];
typedef char AssertChainElemCallbackOffset[
    offsetof(ChainElem, callback) == 0x8 ? 1 : -1];
typedef char AssertChainElemAddedCallbackOffset[
    offsetof(ChainElem, registration_hook) == 0xc ? 1 : -1];
typedef char AssertChainElemFollowupOffset[
    offsetof(ChainElem, calculation_followup) == 0x10 ? 1 : -1];
typedef char AssertChainElemLinkOffset[
    offsetof(ChainElem, link) == 0x14 ? 1 : -1];
typedef char AssertChainElemArgumentOffset[
    offsetof(ChainElem, arg) == 0x20 ? 1 : -1];
typedef char AssertCallbackSchedulerSize[
    sizeof(CallbackScheduler) == 0x48 ? 1 : -1];

enum ChainElemFlags {
    ChainElemFlag_OwnedByScheduler = 1,
    ChainElemFlag_Enabled = 2,
};

// These model the semantic operations provided by TH10 0x00449ed0,
// 0x00449ae0, 0x00449b70, and 0x00449f60. Their native entry conventions
// use registers beyond ordinary C++ calls, so a later ABI bridge owns those
// details rather than leaking them into reconstructed callers.
struct CallbackSchedulerApi {
    static ChainElem *Create(ChainCallback callback);
    static i32 AddToCalculationChain(CallbackScheduler *scheduler,
                                     ChainElem *element,
                                     i32 priority);
    static i32 AddToDrawChain(CallbackScheduler *scheduler,
                              ChainElem *element,
                              i32 priority);
    static void Remove(CallbackScheduler *scheduler, ChainElem *element);
    // Native callers such as AsciiManager destruction acquire the global
    // scheduler lock and bracket removal with the activity-depth byte.
    static void RemoveSynchronized(CallbackScheduler *scheduler,
                                   ChainElem *element);
    static i32 TH10_STDCALL DispatchCalculation(CallbackScheduler *scheduler);
    static i32 TH10_STDCALL DispatchDraw(CallbackScheduler *scheduler);
};

} // namespace th10
