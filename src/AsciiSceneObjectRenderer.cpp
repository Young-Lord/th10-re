#include "AsciiSceneObjectRenderer.hpp"

#include "AsciiHudOwner.hpp"
#include "AsciiHudRenderer.hpp"
#include "AsciiRenderModeDispatcher.hpp"
#include "GameStateManagerObject.hpp"
#include "TitleScreenObject.hpp"

#include <cmath>

namespace th10 {

namespace {

extern void *g_MainChainRenderOwner; // TH11 DAT_00491c10
extern void *g_TitleScreen; // TH11 DAT_00477810
extern void *g_AsciiHudConditionalState; // TH11 DAT_00477704
extern void *g_AsciiHudOwner; // TH11 DAT_0047770c (second render owner instance; destroyed by 0x4145f0)
extern void *g_GameStateManager; // TH11 DAT_00477830 (published by 0x4220e0)
extern u32 g_AsciiHudBarValue; // TH11 DAT_00474c58
extern float g_AsciiObjectScrollX; // TH11 DAT_00470b4c
extern float g_AsciiObjectScrollY; // TH11 DAT_00470b48
extern float g_AsciiBarLeftAnchor; // TH11 DAT_00470cbc
extern float g_AsciiBarTopAnchor; // TH11 DAT_00470cb4
extern float g_AsciiBarTopAdjust; // TH11 DAT_00470be8
extern float g_AsciiHudBarScale; // TH11 DAT_00470cb8
extern float g_AsciiBarHeight; // TH11 DAT_00470b08

inline float ReadFloat(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const float *>(bytes + offset);
}

inline void WriteFloat(u8 *bytes, u32 offset, float value)
{
    *reinterpret_cast<float *>(bytes + offset) = value;
}

// TH11 0x00427c50 forwards its float argument as a double to the CRT floor
// at 0x00452ff0 and returns the truncated result.
float FloorToFloat(float value)
{
    return static_cast<float>(floor(static_cast<double>(value)));
}

} // namespace

// The record callbacks at object+0x3334 live at record+0x94 where each record
// base is object+0x32a0 + index*0x98; the native callee receives the record in
// ECX, so the semantic boundary is the project fastcall convention.
#if defined(_MSC_VER)
typedef void (__fastcall ObjectRecordFn)(void *record);
#else
typedef void __attribute__((fastcall)) ObjectRecordFn(void *record);
#endif

// TH11 0x00426360. Native input is EAX=object; it always returns one. With
// object+0x458 == 2 the body is skipped entirely. Otherwise the object
// position is scrolled into the VM block, the VM is dispatched with the
// global render owner, four record callbacks run, and a title-gated overlay
// bar is drawn as two immediate rectangles keyed on g_AsciiHudBarValue.
i32 RenderAsciiSceneObjectAndBarOverlay(void *object_memory)
{
    u8 *const object = static_cast<u8 *>(object_memory);
    if (*reinterpret_cast<const i32 *>(object + 0x458) == 2)
        return 1;

    WriteFloat(object, 0x354, ReadFloat(object, 0x3c0) + g_AsciiObjectScrollX);
    WriteFloat(object, 0x358, ReadFloat(object, 0x3c4) + g_AsciiObjectScrollY);
    *reinterpret_cast<u32 *>(object + 0x35c) =
        *reinterpret_cast<const u32 *>(object + 0x3c8);
    (void)DispatchAsciiAnimationVmRenderMode(object + 0x14,
        g_MainChainRenderOwner);

    for (u32 index = 0; index != 4; ++index) {
        u8 *const record = object + 0x32a0 + index * 0x98;
        ObjectRecordFn *const callback =
            *reinterpret_cast<ObjectRecordFn *const *>(record + 0x94);
        if (callback != 0)
            callback(record);
    }

    // The gate dereferences DAT_00477830 without a null check and tests
    // DAT_00474c58 last; every failure exits through the shared return.
    if (g_TitleScreen == 0)
        return 1;
    const TitleScreen &ts = *reinterpret_cast<const TitleScreen *>(g_TitleScreen);
    // Native 0x42643b: signed byte at sub_object[0x30] (= object +0x54).
    if (*reinterpret_cast<const signed char *>(&ts.sub_object[0x30]) >= 0)
        return 1;
    if (g_AsciiHudConditionalState == 0 || g_AsciiHudOwner == 0)
        return 1;
    if (*reinterpret_cast<const i32 *>(
            static_cast<u8 *>(g_AsciiHudConditionalState) + 0x10) != 0)
        return 1;
    if (reinterpret_cast<const AsciiHudOwner *>(g_AsciiHudOwner)
            ->result_script_state != 0)
        return 1;
    // Manager mode word at +0x4 (no null check on DAT_00477830).
    const GameStateManager &mgr =
        *reinterpret_cast<const GameStateManager *>(g_GameStateManager);
    if (mgr.mode_0004 != 0)
        return 1;
    const i32 bar_value = static_cast<i32>(g_AsciiHudBarValue);
    if (bar_value == 0)
        return 1;

    const float left = FloorToFloat(ReadFloat(object, 0x3c0) +
        g_AsciiBarLeftAnchor - g_AsciiObjectScrollY);
    const float top = FloorToFloat(ReadFloat(object, 0x3c4) +
        g_AsciiBarTopAnchor - g_AsciiBarTopAdjust);
    const float right = left +
        static_cast<float>(bar_value) * g_AsciiHudBarScale;
    const float bottom = top + g_AsciiBarHeight;

    const float rectangle[] = {left, top, right, bottom};
    DrawImmediateAsciiColoredRectangle(rectangle, 0x80000000U);
    const float inset_rectangle[] = {left - 1.0f, top - 1.0f,
        right - 1.0f, bottom - 1.0f};
    DrawImmediateAsciiColoredRectangle(inset_rectangle, 0xffffffffU);
    return 1;
}

} // namespace th10
