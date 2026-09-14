#include "SpriteViewDebugText.hpp"

#include "AsciiGlyphRenderer.hpp"
#include "AsciiManager.hpp"

#include <stdio.h>

namespace th10 {

namespace {

extern void *g_AsciiManager;         // TH10 DAT_004776e0
extern void *g_MainChainRenderOwner; // TH10 DAT_00491c10

inline u32 ReadU32(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const u32 *>(bytes + offset);
}

inline i32 ReadI32(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const i32 *>(bytes + offset);
}

inline u8 *ReadPointer(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<u8 *const *>(bytes + offset);
}

// The shared tail reached by the file-browser and position views: restore
// the default text color and, when state+0x684 has bit 1 set, recenter the
// embedded VM record at +0x2c8 to the 320x240 anchor and draw it unscaled.
void FinishSpriteViewOverlay(AsciiManager &manager, u8 *state)
{
    manager.color = 0xffffffffU;
    if ((state[0x684U] & 2U) != 0U) {
        u8 *const anchor = state + 0x5fcU;
        *reinterpret_cast<float *>(anchor) = 320.0f;
        *reinterpret_cast<float *>(anchor + 4U) = 240.0f;
        *reinterpret_cast<float *>(anchor + 8U) = 0.0f;
        DrawAsciiAnimationVmUnscaledToOwner(state + 0x2c8U,
                                            g_MainChainRenderOwner);
    }
}

} // namespace

// TH10 0x0040a940. Sprite-viewer ("SprtView") debug overlay text, native
// EDI = state. Always queues the "SprtView\n" header at (0, 0), then
// switches on the view selector at +0x30:
//   1: file browser — "File %s" / "File not found." at (42, 16), "Ecl %s"
//      (through the DAT_00477704+0x54 -> +0x8c name table indexed by
//      +0x1ec) or "Ecl %d" at (42, 26), "Quit" at (42, 36), and the ">"
//      cursor at (48, row * 10 + 16) with the selected row from +0x3c,
//      then the shared tail;
//   2: "Loading %s" in 0xffff4040 at (48, 16), color reset, no tail;
//   3: "Pos %.3d %.3d" in 0xffa0a0a0 at (48, 16) from the +0x678/+0x674
//      integers, then the shared tail;
//   4: "Enemy %d" at (500, 240) from DAT_00477704+0x60, no color reset
//      and no tail;
//   default: header only.
// Every branch returns 1.
i32 DrawSpriteViewOverlayTextEdiAbi(void *state_memory)
{
    u8 *const state = static_cast<u8 *>(state_memory);
    AsciiManager &manager = *static_cast<AsciiManager *>(g_AsciiManager);

    const Float3 header_position = {0.0f, 0.0f, 0.0f};
    manager.AddFormatTextSelected(&header_position, "SprtView\n");

    switch (ReadU32(state, 0x30)) {
    case 1: {
        Float3 position = {42.0f, 16.0f, 0.0f};
        if (ReadU32(state, 0x38) != 0U) {
            const u8 *const table = ReadPointer(state, 0x34);
            manager.AddFormatTextSelected(
                &position, "File %s",
                *reinterpret_cast<void *const *>(
                    table + ReadU32(state, 0x114U) * 4U));
        } else {
            manager.AddFormatTextSelected(&position, "File not found.");
        }
        position.y = 26.0f;
        u8 *const conditional = *reinterpret_cast<u8 *const *>(0x477704U);
        if (conditional != 0) {
            const u8 *const slot = ReadPointer(conditional, 0x54);
            const u8 *const names = ReadPointer(slot, 0x8c);
            manager.AddFormatTextSelected(
                &position, "Ecl %s",
                *reinterpret_cast<void *const *>(
                    names + ReadU32(state, 0x1ecU) * 8U));
        } else {
            manager.AddFormatTextSelected(&position, "Ecl %d",
                                          ReadU32(state, 0x1ecU));
        }
        position.y = 36.0f;
        manager.AddFormatTextSelected(&position, "Quit");
        position.x = 48.0f;
        position.y = static_cast<float>(ReadI32(state, 0x3c)) * 10.0f +
            16.0f;
        manager.AddFormatTextSelected(&position, ">");
        FinishSpriteViewOverlay(manager, state);
        break;
    }
    case 2: {
        manager.color = 0xffff4040U;
        const Float3 position = {48.0f, 16.0f, 0.0f};
        const u8 *const table = ReadPointer(state, 0x34);
        manager.AddFormatTextSelected(
            &position, "Loading %s",
            *reinterpret_cast<void *const *>(
                table + ReadU32(state, 0x114U) * 4U));
        manager.color = 0xffffffffU;
        return 1;
    }
    case 3: {
        manager.color = 0xffa0a0a0U;
        const Float3 position = {48.0f, 16.0f, 0.0f};
        manager.AddFormatTextSelected(&position, "Pos %.3d %.3d",
                                      ReadI32(state, 0x678U),
                                      ReadI32(state, 0x674U));
        FinishSpriteViewOverlay(manager, state);
        break;
    }
    case 4: {
        const Float3 position = {500.0f, 240.0f, 0.0f};
        const u8 *const conditional = *reinterpret_cast<u8 *const *>(0x477704U);
        manager.AddFormatTextSelected(&position, "Enemy %d",
                                      ReadU32(conditional, 0x60));
        return 1;
    }
    default:
        break;
    }
    return 1;
}

} // namespace th10
