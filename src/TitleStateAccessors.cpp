#include "TitleStateAccessors.hpp"

#include "HintTextFile.hpp"

#include <string.h>

namespace th10 {

namespace {

// TH10 0x452493 / 0x452422 / 0x4524a1: operator new / free / delete.
u8 *AllocateHeapBlock(u32 bytes);
void FreeHeapBlock(void *pointer);

// TH10 0x00449ed0 / 0x00449ae0 / 0x00449b70: scheduler node alloc and
// the calc/draw registration pair.
void *AllocSchedulerCallbackNode(void *callback);
void RegisterSchedulerCalcCallback(void *node, void *heap, u32 slot);
void RegisterSchedulerDrawCallback(void *node, void *heap, u32 slot);

// Bound callbacks owned by other translation units.
void HintTextCalcCallback();     // TH10 0x004198a0
void HintTextDrawCallback();     // TH10 0x004198b0

// TH10 0x00418ee0: in-place teardown of the hint sub-record.
void DestroyHintTextOwnerInPlace(void *record);

extern u32 g_SchedulerHeap;      // TH10 DAT_00491be4
extern u8 g_HintFileGate;        // TH10 DAT_00491d6a
extern u8 g_TitleOriginGuard;    // TH10 DAT_00497d8c
extern float g_TitlePlayfieldOrigin[3]; // TH10 DAT_00497d80
extern void *g_TitleStateLists;  // TH10 DAT_00477814

const float kPlayfieldOriginX = 224.0f; // TH10 DAT_00470b4c
const float kPlayfieldOriginY = 16.0f;  // TH10 DAT_00470b48

} // namespace

float *PublishTitlePlayfieldOriginEaxAbi(const float *origin)
{
    if ((g_TitleOriginGuard & 1U) == 0)
        g_TitleOriginGuard |= 1U;
    g_TitlePlayfieldOrigin[0] = origin[0] + kPlayfieldOriginX;
    g_TitlePlayfieldOrigin[1] = origin[1] + kPlayfieldOriginY;
    g_TitlePlayfieldOrigin[2] = origin[2];
    return g_TitlePlayfieldOrigin;
}

void ClearTitleInputCountersEaxAbi(void *state)
{
    u8 *bytes = static_cast<u8 *>(state);
    *reinterpret_cast<u32 *>(bytes + 0x4c) = 0;
    *reinterpret_cast<u32 *>(bytes + 0x50) = 0;
    *reinterpret_cast<u32 *>(bytes + 0x54) = 0;
    *reinterpret_cast<u32 *>(bytes + 0x58) = 0;
}

void AdvanceTitleMenuItemIndexEaxAbi(void *state)
{
    u8 *bytes = static_cast<u8 *>(state);
    i32 index = *reinterpret_cast<i32 *>(bytes + 0x50) + 1;
    *reinterpret_cast<i32 *>(bytes + 0x50) = index;
    if (index >= 10)
        *reinterpret_cast<i32 *>(bytes + 0x50) = 9;
}

void ToggleTitleFlagBit3EaxEcxAbi(void *state, i32 source)
{
    u8 *bytes = static_cast<u8 *>(state);
    u32 flags = *reinterpret_cast<u32 *>(bytes + 0x60);
    u32 delta = (static_cast<u32>(source) << 3) ^ flags;
    delta &= 8U;
    *reinterpret_cast<u32 *>(bytes + 0x60) = flags ^ delta;
}

void ToggleTitleFlagBit1EaxEcxAbi(void *state, i32 source)
{
    u8 *bytes = static_cast<u8 *>(state);
    u32 flags = *reinterpret_cast<u32 *>(bytes + 0x60);
    u32 delta = (static_cast<u32>(source) * 2U) ^ flags;
    delta &= 2U;
    *reinterpret_cast<u32 *>(bytes + 0x60) = flags ^ delta;
}

void ToggleTitleFlagBit0EaxEdxAbi(void *state, i32 source)
{
    u8 *bytes = static_cast<u8 *>(state);
    u32 flags = *reinterpret_cast<u32 *>(bytes + 0x60);
    u32 delta = static_cast<u32>(source) ^ flags;
    delta &= 1U;
    *reinterpret_cast<u32 *>(bytes + 0x60) = flags ^ delta;
}

i32 ReadTitleFlagBit1EaxAbi(const void *state)
{
    const u32 flags = *reinterpret_cast<const u32 *>(
        static_cast<const u8 *>(state) + 0x60);
    return static_cast<i32>((flags >> 1) & 1U);
}

void ClearTitleCursorPairEaxAbi(void *state)
{
    u8 *bytes = static_cast<u8 *>(state);
    *reinterpret_cast<u32 *>(bytes + 0x48) = 0;
    *reinterpret_cast<u32 *>(bytes + 0x4c) = 0;
}

void *InitializeTitleStateListsInPlaceEdxAbi(void *record)
{
    u8 *bytes = static_cast<u8 *>(record);
    memset(bytes, 0, 0x1a8);
    *reinterpret_cast<u32 *>(bytes) |= 2U;
    // Sixteen list heads at 0xc stride from +0x18 to +0xcc, each node
    // seeding next(+4)/prev(+8) with null.
    for (u32 offset = 0x18; offset <= 0xcc; offset += 0xc) {
        *reinterpret_cast<u32 *>(bytes + offset) =
            reinterpret_cast<u32>(bytes);
        *reinterpret_cast<u32 *>(bytes + offset + 4) = 0;
        *reinterpret_cast<u32 *>(bytes + offset + 8) = 0;
    }
    g_TitleStateLists = bytes;
    return bytes;
}

i32 InstallHintTextCallbacksEbxAbi(void *manager)
{
    u8 *bytes = static_cast<u8 *>(manager);

    void *calc_node = AllocSchedulerCallbackNode(
        reinterpret_cast<void *>(&HintTextCalcCallback));
    *reinterpret_cast<u32 *>(calc_node + 0x4) &= ~2U;
    *reinterpret_cast<u32 *>(calc_node + 0x20) =
        reinterpret_cast<u32>(bytes);
    RegisterSchedulerCalcCallback(calc_node, &g_SchedulerHeap, 0x19);
    *reinterpret_cast<u32 *>(bytes + 8) =
        reinterpret_cast<u32>(calc_node);

    void *draw_node = AllocSchedulerCallbackNode(
        reinterpret_cast<void *>(&HintTextDrawCallback));
    *reinterpret_cast<u32 *>(draw_node + 0x4) &= ~2U;
    *reinterpret_cast<u32 *>(draw_node + 0x20) =
        reinterpret_cast<u32>(bytes);
    RegisterSchedulerDrawCallback(draw_node, &g_SchedulerHeap, 0x2c);
    *reinterpret_cast<u32 *>(bytes + 0xc) =
        reinterpret_cast<u32>(draw_node);

    *reinterpret_cast<u32 *>(bytes + 0x10) = 0;
    if (g_HintFileGate == 0)
        return 0;
    // TH10 0x0046d6d8 / 0x0046d6c4.
    ParseHintTextFileStackAbi(bytes, "hint/hint_auto.txt", 0);
    ParseHintTextFileStackAbi(bytes, "hint/hint_user.txt", 1);
    return 0;
}

void *CreateHintTextOwnerEbxAbi(void *manager)
{
    void *record = AllocateHeapBlock(0x1a8);
    if (record != 0)
        InitializeTitleStateListsInPlaceEdxAbi(record);
    if (InstallHintTextCallbacksEbxAbi(manager) == 0)
        return record;
    if (record != 0) {
        DestroyHintTextOwnerInPlace(record);
        FreeHeapBlock(record);
    }
    return 0;
}

} // namespace th10
