// Effect pool per-frame update (TH10 0x0041b8e0) and the effect-node list
// ticker (TH10 0x0041c330). Analysis was done offline with objdump; see
// docs/evidence/effect-pool-entity-update.md.
//
// The 0x3f0-byte slot array walked here (first slot at +0x14, 0x896 slots)
// belongs to the bullet manager DAT_00477818, NOT to the effect manager
// root DAT_004776f0: the bullet manager in-place dtor 0x41adf0 runs the eh
// vector destructor iterator over 2198 x 0x3f0 records starting at
// manager+0x14. The effect manager root contributes only its +0x3e0b50
// resource pointer (EffectManagerRoot::bullet_resource_3e0b50), which
// 0x41b8e0 reads from DAT_004776f0 and passes as the VM-init context (3rd
// argument of InitializeAsciiAnimationVmEntry 0x43e5a0; sites 0x41b998 /
// 0x41b9c7). Each slot starts with a 0x3ac-byte animation VM record
// followed by the effect payload. The native small wrappers around
// 0x0041b8e0 (0x41ba00/0x41ba30) and around 0x0041c330
// (0x41c450/0x41c480/0x41c4e0) only gate on the DAT_00477810 +0x58 state
// flags (and 0x41c480 zeroes/restores the global frame-time scale around
// the tick when bit 0x2 is set); they remain thunk boundaries.
#include <cmath>

#include "AsciiAnimationVm.hpp"
#include "AsciiRenderModeDispatcher.hpp"
#include "EffectManagerRoot.hpp"
#include "EffectPoolEntityUpdate.hpp"
#include "VmRecord.hpp"

namespace th10 {

namespace {

extern void *g_MainChainRenderOwner; // TH10 DAT_00491c10
extern void *g_EffectManagerRoot;    // TH10 DAT_004776f0

extern void FreeMainChainObject(void *object); // TH10 0x4524a1

inline u32 LoadU32At(const void *base, u32 offset)
{
    return *reinterpret_cast<const u32 *>(
        static_cast<const u8 *>(base) + offset);
}

inline void StoreU32At(void *base, u32 offset, u32 value)
{
    *reinterpret_cast<u32 *>(static_cast<u8 *>(base) + offset) = value;
}

inline float LoadF32At(const void *base, u32 offset)
{
    return *reinterpret_cast<const float *>(
        static_cast<const u8 *>(base) + offset);
}

inline void StoreF32At(void *base, u32 offset, float value)
{
    *reinterpret_cast<float *>(static_cast<u8 *>(base) + offset) = value;
}

// TH10 0x463b2c: round half away from zero (x87 conversion).
i32 FloatToI32RoundHalfAway(float value)
{
    return value >= 0.0f
        ? static_cast<i32>(std::floor(static_cast<double>(value) + 0.5))
        : static_cast<i32>(std::ceil(static_cast<double>(value) - 0.5));
}

const float k_playfield_x_offset = 224.0f; // 0x470b4c
const float k_playfield_y_offset = 16.0f;  // 0x470b48
const float k_bottom_fade_y = 8.0f;        // 0x470bd0
const float k_bottom_fade_span = 32.0f;    // 0x470bcc
const float k_fade_scale_a = 0.03125f;     // 0x470d20
const float k_fade_scale_b = 255.0f;       // 0x470bf8

const u32 k_slot_count = 0x896U;
const u32 k_slot_stride = 0x3f0U;
const u32 k_slot_base = 0x14U;

// VM record offsets inside each slot.
const u32 k_vm_alpha_byte = 0x2ffU;  // high byte of the +0x2fc timer dword
const u32 k_vm_position = 0x334U;    // {x, y, z} world floats
const u32 k_vm_script_word = 0x384U; // bound script id word

// Effect payload offsets after the 0x3ac-byte VM record.
const u32 k_effect_script_id = 0x3e4U; // u32
const u32 k_effect_position = 0x3acU;  // {x, y, z} floats — native republish
                                       //   reads slot+0x3ac/+0x3b0/+0x3b4
                                       //   (TH10 0x41b917/0x41b923/0x41b928)
const u32 k_effect_live_flag = 0x3dcU; // u32

// VM-init script bases: entry 0x43e5a0 receives script_id + 0x157/0x160,
// while the slot+0x384 bound-script word is compared against script_id +
// 0x158/0x161 (init value + 1) in both branches (TH10 0x41b9b1/0x41b982).
const u32 k_script_id_base_lower = 0x157U;  // y >= 8 slots (init argument)
const u32 k_script_id_base_upper = 0x160U;  // faded bottom slots (init arg)
const u32 k_script_id_bound_lower = 0x158U; // y >= 8 slots (compare base)
const u32 k_script_id_bound_upper = 0x161U; // faded bottom slots (compare)

} // namespace

// TH10 0x0041b8e0. Native EAX = the bullet manager slot-array base; the
// VM-init context is the effect manager root's +0x3e0b50 resource pointer,
// which the native reads from DAT_004776f0 at each spawn site.
i32 TickEffectPoolSlots(void *pool)
{
    EffectManagerRoot &root = *static_cast<EffectManagerRoot *>(
        g_EffectManagerRoot);
    u8 *slot = static_cast<u8 *>(pool) + k_slot_base;

    for (u32 i = 0; i != k_slot_count; ++i) {
        VmRecord &vm = *reinterpret_cast<VmRecord *>(slot);
        if (LoadU32At(slot, k_effect_live_flag) != 0U) {
            // Republish the scripted position with the playfield offset.
            vm.base_pos_x = LoadF32At(slot, k_effect_position + 0U)
                + k_playfield_x_offset;
            vm.base_pos_y = LoadF32At(slot, k_effect_position + 4U)
                + k_playfield_y_offset;
            vm.base_pos_z = LoadF32At(slot, k_effect_position + 8U);

            const u32 script_id = LoadU32At(slot, k_effect_script_id);
            const i32 bound_script = static_cast<i32>(
                static_cast<short>(vm.sprite_entry_id));
            const float scripted_y =
                LoadF32At(slot, k_effect_position + 4U)
                    + k_playfield_y_offset;

            if (scripted_y < k_bottom_fade_y) { // ordered less only
                // TH10 0x41b93c: the VM base_pos_y (slot+0x338) is clamped
                // to 24.0f before the fade alpha is computed.
                vm.base_pos_y = 24.0f;
                const float drop = scripted_y - k_bottom_fade_y;
                u8 alpha;
                if (drop < k_bottom_fade_span) { // C0 set (or unordered)
                    alpha = static_cast<u8>(FloatToI32RoundHalfAway(
                        drop * k_fade_scale_a * k_fade_scale_b));
                } else {
                    alpha = 0xffU;
                }
                *(slot + k_vm_alpha_byte) = alpha;

                if (bound_script !=
                    static_cast<i32>(script_id) + k_script_id_bound_upper) {
                    (void)InitializeAsciiAnimationVmEntry(
                        slot, script_id + k_script_id_base_upper,
                        root.bullet_resource_3e0b50);
                }
            } else {
                if (bound_script !=
                    static_cast<i32>(script_id) + k_script_id_bound_lower) {
                    (void)InitializeAsciiAnimationVmEntry(
                        slot, script_id + k_script_id_base_lower,
                        root.bullet_resource_3e0b50);
                    *(slot + k_vm_alpha_byte) = 0xffU;
                }
            }

            (void)DispatchAsciiAnimationVmRenderMode(
                slot, g_MainChainRenderOwner);
        }

        slot += k_slot_stride;
    }
    return 1;
}

namespace {

const float k_rate_window_low = 0.99f;  // 0x470b68
const float k_rate_window_high = 1.01f; // 0x470b64
const float k_rate_step = 1.0f;         // 0x470afc

// Node layout.
const u32 k_node_previous = 4U;
const u32 k_node_next = 8U;
const u32 k_node_kind = 0xcU;             // 1 = always finish
const u32 k_node_timer_previous = 0x10U;  // published previous timer
const u32 k_node_timer = 0x14U;           // integer part
const u32 k_node_accumulator = 0x18U;
const u32 k_node_rate_pointer = 0x1cU;
const u32 k_node_finish_latch = 0x50U;    // byte

// Container layout.
const u32 k_list_head = 0x18U;
const u32 k_list_tail = 0x434U;
const u32 k_list_count = 0x438U;

typedef i32 (*NodeVirtualFn)(void *node);

NodeVirtualFn GetNodeSlot(void *node, u32 byte_offset)
{
    void **const vtable = *reinterpret_cast<void ***>(node);
    return reinterpret_cast<NodeVirtualFn>(
        *reinterpret_cast<void **>(
            reinterpret_cast<u8 *>(vtable) + byte_offset));
}

// Shared finish + unlink + free tail of the removal paths.
void FinishAndRemoveNode(void *container, void *node)
{
    u8 *const owner = static_cast<u8 *>(container);
    u8 *const bytes = static_cast<u8 *>(node);

    GetNodeSlot(node, 0x10U)(node); // finish vtable slot
    StoreU32At(owner, k_list_count,
               LoadU32At(owner, k_list_count) - 1U);

    u8 *const next =
        reinterpret_cast<u8 *>(LoadU32At(bytes, k_node_next));
    u8 *const previous =
        reinterpret_cast<u8 *>(LoadU32At(bytes, k_node_previous));
    if (previous != 0)
        StoreU32At(previous, k_node_next,
                   reinterpret_cast<u32>(next));
    if (next != 0)
        StoreU32At(next, k_node_previous,
                   reinterpret_cast<u32>(previous));
    if (reinterpret_cast<u8 *>(LoadU32At(owner, k_list_tail)) == bytes)
        StoreU32At(owner, k_list_tail,
                   reinterpret_cast<u32>(previous));
    FreeMainChainObject(node);
}

} // namespace

// TH10 0x0041c330.
i32 TickEffectNodeList(void *container)
{
    u8 *owner = static_cast<u8 *>(container);
    u8 *node = reinterpret_cast<u8 *>(LoadU32At(owner, k_list_head));
    if (node == 0)
        return 1;

    for (;;) {
        u8 latch = node[k_node_finish_latch];
        u8 *next = reinterpret_cast<u8 *>(LoadU32At(node, k_node_next));
        bool finished = false;

        if (latch != 0U) {
            // The native byte increment wraps without influencing the
            // comparison, so a 0xff latch falls through to the kind
            // check exactly like the original.
            ++latch;
            node[k_node_finish_latch] = latch;
            if (latch >= 2U)
                finished = true;
        }

        if (!finished) {
            if (LoadU32At(node, k_node_kind) == 1U) {
                finished = true;
            } else if (GetNodeSlot(node, 0x8U)(node) != 0) {
                finished = true;
            } else {
                // Publish the previous timer, then advance the
                // {timer, accumulator, rate} record.
                StoreU32At(node, k_node_timer_previous,
                           LoadU32At(node, k_node_timer));
                const float rate = LoadF32At(
                    reinterpret_cast<const void *>(
                        LoadU32At(node, k_node_rate_pointer)), 0U);
                if (rate > k_rate_window_low
                    && rate < k_rate_window_high) {
                    StoreU32At(node, k_node_timer,
                               LoadU32At(node, k_node_timer) + 1U);
                    StoreF32At(node, k_node_accumulator,
                               LoadF32At(node, k_node_accumulator)
                                   + k_rate_step);
                } else {
                    const float advanced =
                        LoadF32At(node, k_node_accumulator) + rate;
                    StoreF32At(node, k_node_accumulator, advanced);
                    StoreU32At(node, k_node_timer,
                               static_cast<u32>(
                                   FloatToI32RoundHalfAway(advanced)));
                }
            }
        }

        if (finished)
            FinishAndRemoveNode(owner, node);

        if (next == 0)
            break;
        node = next;
    }
    return 1;
}

} // namespace th10
