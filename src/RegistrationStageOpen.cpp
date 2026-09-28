#include "RegistrationStageOpen.hpp"

#include "EntityHelpers.hpp"
#include "StageScriptOpen.hpp"

#include <string.h>

namespace th10 {

namespace {

// TH10 0x452493 / 0x452422 / 0x4524a1: operator new / free / delete.
u8 *AllocateHeapBlock(u32 bytes);
void FreeHeapBlock(void *pointer);

// TH10 0x00447280: RequestManagerWork(g_491c10 owner, name, slot).
void *RequestManagerWorkSlotNative(void *owner, const char *name,
                                   i32 slot);

// TH10 0x00447810: ReleaseManagerWorkContents (native EDI ABI).
void ReleaseManagerWorkSlotNative(void *work);

// TH10 0x0044b810: error logger on the reserved output object.
void ReportStageOpenError(const char *text);

const char kStageOpenWorkName[] = "front.anm";  // TH10 0x0046d254
// TH10 0x0046cb68: Japanese failure message, left untranscribed.
const char kStageOpenFailureText[] = "";

// TH10 0x00449ed0 / 0x00449ae0 / 0x00449b70 / 0x00449f60: scheduler
// callback node allocator and the calc/draw registration pair.
void *AllocSchedulerCallbackNode(void *callback);
void RegisterSchedulerCalcCallback(void *node, void *heap, u32 slot);
void RegisterSchedulerDrawCallback(void *node, void *heap, u32 slot);
void ReleaseSchedulerCallback(void *node, void *heap);

// Bound callbacks owned by other translation units.
void RegistrationDrawCallback();     // TH10 0x00413690
void StageOpenCalcCallback();        // TH10 0x00415ae0
void StageOpenDrawCallback();        // TH10 0x00415af0

extern void *g_RegistrationDrawOwner; // TH10 DAT_00477708
extern void *g_ManagerWorkOwner;     // TH10 DAT_00491c10
extern u32 g_SchedulerHeap;          // TH10 DAT_00491be4
extern u32 g_ManagerModeFlags;       // TH10 DAT_00474ca0
extern void **g_StageScriptWorkSlot; // TH10 DAT_00491c10 + 0x3ad0dc

const u32 kRegistrationOwnerFlag = 0x10U;
const u32 kRegistrationOwnerFlagMask = 0x9U;

} // namespace

// TH10 0x00413350. The native zero-fill is a 0x23 dword rep stos over
// the 0x8c block (the alignment tail byte stays uninit), then flag bit
// 1 is OR'd in. The 0x24 callback node seeds its flags from the heap
// garbage at +4, masks bit 0, and later ORs bits 1+2 for the draw
// scheduler registration.
void *CreateRegistrationDrawOwner()
{
    u8 *owner = AllocateHeapBlock(0x8c);
    if (owner != 0) {
        memset(owner, 0, 0x8c);
        *reinterpret_cast<u32 *>(owner) |= 2U;
        g_RegistrationDrawOwner = owner;
    }

    u8 *node = AllocateHeapBlock(0x24);
    if (node != 0) {
        u32 flags = *reinterpret_cast<u32 *>(node + 4);
        *reinterpret_cast<u32 *>(node + 0x8) = 0;
        *reinterpret_cast<u32 *>(node + 0xc) = 0;
        *reinterpret_cast<u32 *>(node + 0x10) = 0;
        *reinterpret_cast<u32 *>(node + 0x0) = 0;
        *reinterpret_cast<u32 *>(node + 0x4) = flags & ~1U;
        *reinterpret_cast<u32 *>(node + 0x14) =
            reinterpret_cast<u32>(node);
        *reinterpret_cast<u32 *>(node + 0x18) = 0;
        *reinterpret_cast<u32 *>(node + 0x1c) = 0;
    }

    u32 node_flags = *reinterpret_cast<u32 *>(node + 4) | 3U;
    *reinterpret_cast<u32 *>(node + 0x8) =
        reinterpret_cast<u32>(&RegistrationDrawCallback);
    *reinterpret_cast<u32 *>(node + 0xc) = 0;
    *reinterpret_cast<u32 *>(node + 0x10) = 0;
    *reinterpret_cast<u32 *>(node + 0x20) =
        reinterpret_cast<u32>(owner);
    *reinterpret_cast<u32 *>(node + 0x4) = node_flags;
    RegisterSchedulerDrawCallback(node, &g_SchedulerHeap, 0x2f);
    *reinterpret_cast<u32 *>(owner + 0xc) =
        reinterpret_cast<u32>(node);
    return owner;
}

// TH10 0x004136c0. ESI = owner; the six handles release against the
// render owner global (DAT_00491c10) and are cleared even when the
// release reports failure.
void ReleaseRegistrationOwnerHandleChainEsiAbi(void *owner)
{
    u8 *bytes = static_cast<u8 *>(owner);
    void *render_owner = g_ManagerWorkOwner;
    for (u32 offset = 0x40; offset <= 0x54; offset += 4) {
        ReleaseEntityById(render_owner,
                          *reinterpret_cast<u32 *>(bytes + offset));
        *reinterpret_cast<u32 *>(bytes + offset) = 0;
    }
}

void SetManagerRegistrationFlagEaxAbi(void *manager)
{
    u8 *bytes = static_cast<u8 *>(manager);
    *reinterpret_cast<u32 *>(bytes + 0x9eb4) |= kRegistrationOwnerFlag;
    *reinterpret_cast<u32 *>(bytes + 0x9ecc) = 0;
}

i32 ReadManagerRegistrationFlagEaxAbi(const void *manager)
{
    const u32 flags = *reinterpret_cast<const u32 *>(
        static_cast<const u8 *>(manager) + 0x9eb4);
    return static_cast<i32>((flags >> 4) & 1U);
}

i32 ReadManagerPostRegistrationFlagEaxAbi(const void *manager)
{
    const u32 flags = *reinterpret_cast<const u32 *>(
        static_cast<const u8 *>(manager) + 0x9eb4);
    return static_cast<i32>((flags >> 5) & 1U);
}

i32 OpenStageScriptSequenceEbxAbi(void *manager)
{
    void *work = RequestManagerWorkSlotNative(
        g_ManagerWorkOwner, kStageOpenWorkName, 6);
    *reinterpret_cast<void **>(
        static_cast<u8 *>(manager) + 0x9ec8) = work;
    if (work == 0) {
        // TH10 0x0046cb68: failure message for the reserved logger.
        ReportStageOpenError(kStageOpenFailureText);
        return -1;
    }
    if (OpenSceneScriptResource(manager) != 0)
        return -1;

    void *calc_node = AllocSchedulerCallbackNode(
        reinterpret_cast<void *>(&StageOpenCalcCallback));
    *reinterpret_cast<u32 *>(calc_node + 0x4) &= ~2U;
    *reinterpret_cast<u32 *>(calc_node + 0x20) =
        reinterpret_cast<u32>(manager);
    RegisterSchedulerCalcCallback(calc_node, &g_SchedulerHeap, 0x18);
    *reinterpret_cast<u32 *>(static_cast<u8 *>(manager) + 0x8) =
        reinterpret_cast<u32>(calc_node);

    void *draw_node = AllocSchedulerCallbackNode(
        reinterpret_cast<void *>(&StageOpenDrawCallback));
    *reinterpret_cast<u32 *>(draw_node + 0x4) &= ~2U;
    *reinterpret_cast<u32 *>(draw_node + 0x20) =
        reinterpret_cast<u32>(manager);
    RegisterSchedulerDrawCallback(draw_node, &g_SchedulerHeap, 0x2b);
    *reinterpret_cast<u32 *>(static_cast<u8 *>(manager) + 0xc) =
        reinterpret_cast<u32>(draw_node);
    return 0;
}

void ReleaseRegistrationOwnerFromManagerEbxAbi(void *manager)
{
    u8 *bytes = static_cast<u8 *>(manager);
    u8 *owner = *reinterpret_cast<u8 **>(bytes + 0x9eb8);
    if (owner != 0) {
        ReleaseRegistrationOwnerHandleChainEsiAbi(owner);
        FreeHeapBlock(owner);
        *reinterpret_cast<u32 *>(bytes + 0x9eb8) = 0;
    }

    if ((g_ManagerModeFlags & kRegistrationOwnerFlagMask) != 0)
        return;

    if (*g_StageScriptWorkSlot != 0) {
        ReleaseManagerWorkSlotNative(*g_StageScriptWorkSlot);
        FreeHeapBlock(*g_StageScriptWorkSlot);
        *g_StageScriptWorkSlot = 0;
    }
    void *script = *reinterpret_cast<void **>(bytes + 0x9ebc);
    *reinterpret_cast<u32 *>(bytes + 0x9e80) = 0;
    if (script != 0)
        FreeHeapBlock(script);
    *reinterpret_cast<u32 *>(bytes + 0x9ebc) = 0;
    *reinterpret_cast<u32 *>(bytes + 0x9ebc) = 0;
}

} // namespace th10
