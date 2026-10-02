#include "SpriteViewDebugText.hpp"

#include "AsciiGlyphRenderer.hpp"
#include "AsciiManager.hpp"
#include "ConditionalStateObject.hpp"
#include "StageHostObject.hpp"

#include <stdio.h>

namespace th10 {

namespace {

extern void *g_AsciiManager;         // TH10 DAT_004776e0
extern void *g_MainChainRenderOwner; // TH10 DAT_00491c10

// The shared tail reached by the file-browser and position views: restore
// the default text color and, when flags_0684 has bit 1 set, recenter the
// embedded animation record's base position (+0x2c8+0x334) to the 320x240
// anchor and draw it unscaled.
void FinishSpriteViewOverlay(AsciiManager &manager, StageHostObject &host)
{
    manager.color = 0xffffffffU;
    if ((host.flags_0684 & 2U) != 0U) {
        host.animation_vm.base_pos_x = 320.0f;
        host.animation_vm.base_pos_y = 240.0f;
        host.animation_vm.base_pos_z = 0.0f;
        DrawAsciiAnimationVmUnscaledToOwner(&host.animation_vm,
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
    StageHostObject &host = *static_cast<StageHostObject *>(state_memory);
    AsciiManager &manager = *static_cast<AsciiManager *>(g_AsciiManager);

    const Float3 header_position = {0.0f, 0.0f, 0.0f};
    manager.AddFormatTextSelected(&header_position, "SprtView\n");

    switch (host.state_machine) {
    case 1: {
        Float3 position = {42.0f, 16.0f, 0.0f};
        if (host.stage_table_count != 0U) {
            const char *const *const table = host.stage_table;
            manager.AddFormatTextSelected(
                &position, "File %s",
                table[host.cursor_b.value]);
        } else {
            manager.AddFormatTextSelected(&position, "File not found.");
        }
        position.y = 26.0f;
        // Typed view of the conditional state; +0x54 is the modeled
        // name-registry pointer and +0x8c its name-table pointer. The
        // entry index (state+0x1ec) is unbounded natively, so the 8-byte
        // entry read itself stays RAW (no bounds check in the native).
        ConditionalState *const conditional =
            *reinterpret_cast<ConditionalState *const *>(0x477704U);
        if (conditional != 0) {
            const ConditionalNameRegistry *const registry =
                static_cast<const ConditionalNameRegistry *>(
                    conditional->name_registry_0054);
            const void *const names = registry->name_table_008c;
            manager.AddFormatTextSelected(
                &position, "Ecl %s",
                *reinterpret_cast<void *const *>(
                    static_cast<const u8 *>(names) +
                    host.cursor_c.value * 8U));
        } else {
            manager.AddFormatTextSelected(&position, "Ecl %d",
                                          host.cursor_c.value);
        }
        position.y = 36.0f;
        manager.AddFormatTextSelected(&position, "Quit");
        position.x = 48.0f;
        position.y = static_cast<float>(host.cursor_a.value) * 10.0f +
            16.0f;
        manager.AddFormatTextSelected(&position, ">");
        FinishSpriteViewOverlay(manager, host);
        break;
    }
    case 2: {
        manager.color = 0xffff4040U;
        const Float3 position = {48.0f, 16.0f, 0.0f};
        const char *const *const table = host.stage_table;
        manager.AddFormatTextSelected(
            &position, "Loading %s", table[host.cursor_b.value]);
        manager.color = 0xffffffffU;
        return 1;
    }
    case 3: {
        manager.color = 0xffa0a0a0U;
        const Float3 position = {48.0f, 16.0f, 0.0f};
        manager.AddFormatTextSelected(&position, "Pos %.3d %.3d",
                                      *reinterpret_cast<const i32 *>(
                                          &host.unknown_0678),
                                      *reinterpret_cast<const i32 *>(
                                          &host.position_display_x));
        FinishSpriteViewOverlay(manager, host);
        break;
    }
    case 4: {
        const Float3 position = {500.0f, 240.0f, 0.0f};
        // Typed view; the gate word is script_count_0060 (+0x60). The
        // native dereferences the DAT_00477704 holder without a null check
        // on this path.
        const ConditionalState *const conditional =
            *reinterpret_cast<ConditionalState *const *>(0x477704U);
        manager.AddFormatTextSelected(&position, "Enemy %d",
                                      conditional->script_count_0060);
        return 1;
    }
    default:
        break;
    }
    return 1;
}

} // namespace th10
