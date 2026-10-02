#include "EffectPoolLifecycle.hpp"

#include "EffectPoolEntityUpdate.hpp"
#include "TitleScreenObject.hpp"

#include <string.h>

namespace th10 {

namespace {

// TH10 0x452493 / 0x452422 / 0x4524a1: operator new / free / delete.
u8 *AllocateHeapBlock(u32 bytes);
void FreeHeapBlock(void *pointer);

// TH10 0x0045252d: eh vector constructor iterator over the pool.
void InitEntityPoolArray(void *pool, u32 record_size, u32 count,
                         void *reset_fn, void *free_fn);

// TH10 0x00449ed0 / 0x00449ae0 / 0x00449b70: scheduler node alloc and
// the calc/draw registration pair.
void *AllocSchedulerCallbackNode(void *callback);
void RegisterSchedulerCalcCallback(void *node, void *heap, u32 slot);
void RegisterSchedulerDrawCallback(void *node, void *heap, u32 slot);

// Bound callbacks owned by other translation units.
void EffectPoolCalcCallback();   // TH10 0x0041ba00
void EffectPoolDrawCallback();   // TH10 0x0041ba30 (this gate)

// TH10 0x0041adf0: in-place teardown of the effect pool.
void DestroyEffectPoolInPlace(void *pool);

extern u32 g_SchedulerHeap;      // TH10 DAT_00491be4
extern void *g_EffectPoolManager; // TH10 DAT_00477818
extern void *g_MainChainContext;  // TH10 DAT_00477810

} // namespace

void CopyTipPairEaxEcxAbi(const void *src, void *dst)
{
    const u32 *source = static_cast<const u32 *>(src);
    u32 *target = static_cast<u32 *>(dst);
    target[0] = source[0];
    target[1] = source[1];
}

void CopyTipPairToSlot8EaxEcxAbi(const void *src, void *dst)
{
    const u32 *source = static_cast<const u32 *>(src);
    u32 *target = static_cast<u32 *>(dst);
    target[2] = source[0];
    target[3] = source[1];
}

void SetTipPairFromStackEaxStackAbi(void *dst, u32 value_a, u32 value_b)
{
    u32 *target = static_cast<u32 *>(dst);
    target[0] = value_a;
    target[1] = value_b;
}

void *ResetStageEntityRecordEcxAbi(void *record)
{
    u8 *bytes = static_cast<u8 *>(record);
    // Nine flag clears ahead of the wipe: the script-active bit and the
    // eight per-slot kind bits.
    static const u32 kFlagOffsets[] = {
        0x6cU, 0xb0U, 0xfcU, 0x128U, 0x174U, 0x1b0U, 0x1fcU, 0x228U,
        0x378U
    };
    for (u32 index = 0; index != 9U; ++index)
        *reinterpret_cast<u32 *>(bytes + kFlagOffsets[index]) &= ~1U;
    memset(bytes, 0, 0x3ac);
    *reinterpret_cast<u16 *>(bytes + 0x384) = 0xffffU;
    *reinterpret_cast<u32 *>(bytes + 0x3d8) &= ~1U;
    return bytes;
}

void FreeEntityRibbonBufferEcxAbi(void *entity)
{
    u8 *bytes = static_cast<u8 *>(entity);
    void *buffer = *reinterpret_cast<void **>(bytes + 0x358);
    if (buffer != 0)
        FreeHeapBlock(buffer);
    *reinterpret_cast<u32 *>(bytes + 0x358) = 0;
}

i32 InstallEffectPoolCallbacksEbxAbi(void *manager)
{
    u8 *bytes = static_cast<u8 *>(manager);

    u8 *calc_node = static_cast<u8 *>(AllocSchedulerCallbackNode(
        reinterpret_cast<void *>(&EffectPoolCalcCallback)));
    *reinterpret_cast<u32 *>(calc_node + 0x4) &= ~2U;
    *reinterpret_cast<u32 *>(calc_node + 0x20) =
        reinterpret_cast<u32>(bytes);
    RegisterSchedulerCalcCallback(calc_node, &g_SchedulerHeap, 0x15);
    *reinterpret_cast<u32 *>(bytes + 8) =
        reinterpret_cast<u32>(calc_node);

    u8 *draw_node = static_cast<u8 *>(AllocSchedulerCallbackNode(
        reinterpret_cast<void *>(&EffectPoolDrawCallback)));
    *reinterpret_cast<u32 *>(draw_node + 0x4) &= ~2U;
    *reinterpret_cast<u32 *>(draw_node + 0x20) =
        reinterpret_cast<u32>(bytes);
    RegisterSchedulerDrawCallback(draw_node, &g_SchedulerHeap, 0x19);
    *reinterpret_cast<u32 *>(bytes + 0xc) =
        reinterpret_cast<u32>(draw_node);
    return 0;
}

void *CreateEffectPoolManagerEbxAbi(void *manager)
{
    u8 *pool = AllocateHeapBlock(0x21cec0);
    if (pool != 0) {
        InitEntityPoolArray(pool + 0x14, 0x3f0, 0x896,
                            reinterpret_cast<void *>(
                                &ResetStageEntityRecordEcxAbi),
                            reinterpret_cast<void *>(
                                &FreeEntityRibbonBufferEcxAbi));
        memset(pool, 0, 0x21cec0);
        *reinterpret_cast<u32 *>(pool) |= 2U;
        g_EffectPoolManager = pool;
    }
    if (InstallEffectPoolCallbacksEbxAbi(manager) == 0)
        return pool;
    if (pool != 0) {
        DestroyEffectPoolInPlace(pool);
        FreeHeapBlock(pool);
    }
    return 0;
}

i32 TickEffectPoolSlotsGuarded(void)
{
    // Native 0x0041ba30: null-checked byte test of the title screen's
    // +0x58 flags (bit 0x4).
    if (g_MainChainContext != 0) {
        const TitleScreen &ts =
            *reinterpret_cast<const TitleScreen *>(g_MainChainContext);
        if ((*reinterpret_cast<const u8 *>(&ts.flags) & 4U) != 0U)
            return 1;
    }
    return TickEffectPoolSlots(g_EffectPoolManager);
}

} // namespace th10
