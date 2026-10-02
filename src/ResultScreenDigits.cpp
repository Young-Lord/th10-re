#include "ResultScreenDigits.hpp"

#include "AsciiAnimationVm.hpp"
#include "EntityHelpers.hpp"
#include "GameManagerObject.hpp"

namespace th10 {

namespace {

extern void *g_MainChainRenderOwner; // TH10 DAT_00491c10

u32 LoadU32(const u8 *address)
{
    return static_cast<u32>(address[0]) | (static_cast<u32>(address[1]) << 8)
         | (static_cast<u32>(address[2]) << 16)
         | (static_cast<u32>(address[3]) << 24);
}

void StoreU32(u8 *address, u32 value)
{
    address[0] = static_cast<u8>(value);
    address[1] = static_cast<u8>(value >> 8);
    address[2] = static_cast<u8>(value >> 16);
    address[3] = static_cast<u8>(value >> 24);
}

u16 LoadU16(const u8 *address)
{
    return static_cast<u16>(static_cast<u16>(address[0])
                            | (static_cast<u16>(address[1]) << 8));
}

// Digit entry index base shared by every glyph VM init below ('3' + value,
// matching the native +0x33 constant).
const i32 k_digit_entry_base = 51;

// The twenty stat-glyph blocks in native order: child kind, state u16
// field offset, and whether the block shows the tens digit. Kinds 77..86
// repeat the tens/units of the same five fields (second display column).
struct StatDigitBlock {
    u16 kind;
    u32 field_offset;
    bool tens;
};

const StatDigitBlock k_blocks[20] = {
    {67, 0x59ccU, true},  {68, 0x59ccU, false},
    {77, 0x59ccU, true},  {78, 0x59ccU, false},
    {69, 0x59ceU, true},  {70, 0x59ceU, false},
    {79, 0x59ceU, true},  {80, 0x59ceU, false},
    {71, 0x59d0U, true},  {72, 0x59d0U, false},
    {81, 0x59d0U, true},  {82, 0x59d0U, false},
    {73, 0x59d2U, true},  {74, 0x59d2U, false},
    {83, 0x59d2U, true},  {84, 0x59d2U, false},
    {75, 0x59d4U, true},  {76, 0x59d4U, false},
    {85, 0x59d4U, true},  {86, 0x59d4U, false},
};

// Native walk: nodes start at parent+0x10 ({child, next} links); the child
// record's u16 kind lives at +0x38a. Mirrors the binary including the
// missing parent null check (a zero parent walks the node at 0x10).
u32 ResolveChildIdByKind(u8 *parent, u16 kind)
{
    u32 *node = reinterpret_cast<u32 *>(parent + 0x10);
    while (node != 0) {
        u8 *child = *reinterpret_cast<u8 **>(node);
        if (LoadU16(child + 0x38aU) == kind) {
            return LoadU32(child);
        }
        node = *reinterpret_cast<u32 **>(node + 4U);
    }
    return 0U;
}

} // namespace

void *UpdateResultScreenStatDigitsEaxAbi(void *state)
{
    GameManager &mgr = *reinterpret_cast<GameManager *>(state);
    void *const manager = g_MainChainRenderOwner;
    void *result = 0;

    const u32 block_count = sizeof(k_blocks) / sizeof(k_blocks[0]);
    for (u32 i = 0; i < block_count; ++i) {
        const u32 parent_id = mgr.script_entity_handles[2];
        u8 *const parent = FindEntityEdxStackAbi(manager, parent_id);
        if (parent == 0) {
            mgr.script_entity_handles[2] = 0U;
        }

        const u32 child_id = ResolveChildIdByKind(parent, k_blocks[i].kind);
        u8 *const child = FindEntityEdxStackAbi(manager, child_id);
        if (child != 0) {
            // k_blocks stores the native state u16 offsets 0x59cc..0x59d4,
            // i.e. the result_stats[0..4] words.
            const i32 value = static_cast<signed short>(
                mgr.result_stats[(k_blocks[i].field_offset - 0x59ccU) / 2U]);
            const i32 digit = k_blocks[i].tens ? value / 10 : value % 10;
            result = reinterpret_cast<void *>(InitializeAsciiAnimationVmEntry(
                child, static_cast<u32>(digit + k_digit_entry_base),
                *reinterpret_cast<void **>(child + 0x308U)));
        } else {
            result = child;
        }
    }
    return result;
}

} // namespace th10
