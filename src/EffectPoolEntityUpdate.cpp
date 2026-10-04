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
#include "BulletManager.hpp"
#include "EffectManagerRoot.hpp"
#include "EffectPoolEntityUpdate.hpp"
#include "StageObjectManagerObject.hpp"
#include "VmRecord.hpp"

namespace th10 {

namespace {

extern void *g_MainChainRenderOwner; // TH10 DAT_00491c10
extern void *g_EffectManagerRoot;    // TH10 DAT_004776f0

extern void FreeMainChainObject(void *object); // TH10 0x4524a1

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

// Slot field map (BulletSlot): live flag state_03dc (+0x3dc), scripted
// position position_x/y/z_03ac/3b0/3b4 — native republish reads
// slot+0x3ac/+0x3b0/+0x3b4 (TH10 0x41b917/0x41b923/0x41b928) — effect
// script id bound_kind_03e4 (+0x3e4). VM fields ride slot.vm: alpha
// byte vm+0x2ff (high byte of the +0x2fc primary_color dword), world
// position vm.base_pos_x/y/z (+0x334/+0x338/+0x33c), bound script id
// word vm.sprite_entry_id (+0x384).

// VM-init script bases: entry 0x43e5a0 receives script_id + 0x157/0x160,
// while the slot+0x384 bound-script word is compared against script_id +
// 0x158/0x161 (init value + 1) in both branches (TH10 0x41b9b1/0x41b982).
const u32 k_script_id_base_lower = 0x157U;  // y >= 8 slots (init argument)
const u32 k_script_id_base_upper = 0x160U;  // faded bottom slots (init arg)
const u32 k_script_id_bound_lower = 0x158U; // y >= 8 slots (compare base)
const u32 k_script_id_bound_upper = 0x161U; // faded bottom slots (compare)

} // namespace

// TH10 0x0041b8e0. Native EAX = the bullet manager base; the walked
// 0x896 x 0x3f0 slot array starts at manager+0x14 (BulletManager::
// slots). The VM-init context is the effect manager root's +0x3e0b50
// resource pointer, which the native reads from DAT_004776f0 at each
// spawn site.
i32 TickEffectPoolSlots(void *pool)
{
    EffectManagerRoot &root = *static_cast<EffectManagerRoot *>(
        g_EffectManagerRoot);
    BulletManager &mgr = *reinterpret_cast<BulletManager *>(pool);

    for (u32 i = 0; i != k_slot_count; ++i) {
        BulletSlot &slot = mgr.slots[i];
        VmRecord &vm = slot.vm;
        if (slot.state_03dc != 0) {
            // Republish the scripted position with the playfield offset;
            // the native reads slot+0x3ac/+0x3b0/+0x3b4
            // (TH10 0x41b917/0x41b923/0x41b928).
            vm.base_pos_x = slot.position_x_03ac + k_playfield_x_offset;
            vm.base_pos_y = slot.position_y_03b0 + k_playfield_y_offset;
            vm.base_pos_z = slot.position_z_03b4;

            const u32 script_id = slot.bound_kind_03e4;
            const i32 bound_script = static_cast<i32>(
                static_cast<short>(vm.sprite_entry_id));
            const float scripted_y =
                slot.position_y_03b0 + k_playfield_y_offset;

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
                // vm+0x2ff: the high byte of the +0x2fc primary_color
                // dword; the native writes the single byte.
                *(reinterpret_cast<u8 *>(&vm.primary_color) + 3) = alpha;

                if (bound_script !=
                    static_cast<i32>(script_id) + k_script_id_bound_upper) {
                    (void)InitializeAsciiAnimationVmEntry(
                        &slot, script_id + k_script_id_base_upper,
                        root.bullet_resource_3e0b50);
                }
            } else {
                if (bound_script !=
                    static_cast<i32>(script_id) + k_script_id_bound_lower) {
                    (void)InitializeAsciiAnimationVmEntry(
                        &slot, script_id + k_script_id_base_lower,
                        root.bullet_resource_3e0b50);
                    *(reinterpret_cast<u8 *>(&vm.primary_color) + 3) = 0xffU;
                }
            }

            (void)DispatchAsciiAnimationVmRenderMode(
                &slot, g_MainChainRenderOwner);
        }
    }
    return 1;
}

namespace {

const float k_rate_window_low = 0.99f;  // 0x470b68
const float k_rate_window_high = 1.01f; // 0x470b64
const float k_rate_step = 1.0f;         // 0x470afc

// Node field map (StageObjectHeader): links list_prev_0004 (+0x004) /
// list_next_0008 (+0x008), kind state_000c (+0x00c; 1 = always
// finish), timer record timer_0010 (+0x010: published previous timer
// +0x010, integer part +0x014, accumulator +0x018, rate pointer
// +0x01c), finish latch done_latch_0050 (+0x050, byte). Container
// (StageObjectManager): first node list_sentinel_0010.list_next_0008
// (+0x18), tail list_head_0434 (+0x434), count node_count_0438
// (+0x438).

typedef i32 (*NodeVirtualFn)(void *node);

NodeVirtualFn GetNodeSlot(void *node, u32 byte_offset)
{
    void **const vtable = *reinterpret_cast<void ***>(node);
    return reinterpret_cast<NodeVirtualFn>(
        *reinterpret_cast<void **>(
            reinterpret_cast<u8 *>(vtable) + byte_offset));
}

// Shared finish + unlink + free tail of the removal paths.
void FinishAndRemoveNode(StageObjectManager &owner, StageObjectHeader &node)
{
    GetNodeSlot(&node, 0x10U)(&node); // finish vtable slot
    --owner.node_count_0438;

    StageObjectHeader *const next =
        static_cast<StageObjectHeader *>(node.list_next_0008);
    StageObjectHeader *const previous =
        static_cast<StageObjectHeader *>(node.list_prev_0004);
    if (previous != 0)
        previous->list_next_0008 = next;
    if (next != 0)
        next->list_prev_0004 = previous;
    if (owner.list_head_0434 == &node)
        owner.list_head_0434 = previous;
    FreeMainChainObject(&node);
}

} // namespace

// TH10 0x0041c330. The container is the stage-object manager
// (DAT_0047781c): the first node is list_sentinel_0010.list_next_0008
// (manager+0x18), the tail is list_head_0434 (+0x434) and the count is
// node_count_0438 (+0x438).
i32 TickEffectNodeList(void *container)
{
    StageObjectManager &owner =
        *reinterpret_cast<StageObjectManager *>(container);
    StageObjectHeader *node = static_cast<StageObjectHeader *>(
        owner.list_sentinel_0010.list_next_0008);
    if (node == 0)
        return 1;

    for (;;) {
        u8 latch = node->done_latch_0050;
        StageObjectHeader *next =
            static_cast<StageObjectHeader *>(node->list_next_0008);
        bool finished = false;

        if (latch != 0U) {
            // The native byte increment wraps without influencing the
            // comparison, so a 0xff latch falls through to the kind
            // check exactly like the original.
            ++latch;
            node->done_latch_0050 = latch;
            if (latch >= 2U)
                finished = true;
        }

        if (!finished) {
            if (node->state_000c == 1) { // kind 1 = always finish
                finished = true;
            } else if (GetNodeSlot(node, 0x8U)(node) != 0) {
                finished = true;
            } else {
                // Publish the previous timer, then advance the
                // {timer, accumulator, rate} record.
                node->timer_0010.prev = node->timer_0010.count;
                const float rate = *node->timer_0010.rate;
                if (rate > k_rate_window_low
                    && rate < k_rate_window_high) {
                    node->timer_0010.count = node->timer_0010.count + 1;
                    *reinterpret_cast<float *>(&node->timer_0010.accum) =
                        *reinterpret_cast<const float *>(
                            &node->timer_0010.accum) + k_rate_step;
                } else {
                    const float advanced =
                        *reinterpret_cast<const float *>(
                            &node->timer_0010.accum) + rate;
                    *reinterpret_cast<float *>(&node->timer_0010.accum) =
                        advanced;
                    // TH10 0x41c42d: the __ftol2 (0x463b2c) result is
                    // stored as a plain dword into the +0x14 count; the
                    // +0x18 float accumulator keeps the unrounded sum.
                    node->timer_0010.count =
                        FloatToI32RoundHalfAway(advanced);
                }
            }
        }

        if (finished)
            FinishAndRemoveNode(owner, *node);

        if (next == 0)
            break;
        node = next;
    }
    return 1;
}

} // namespace th10
