// TH10 0x419650 (+ thunk 0x4198a0) - scheduled callback that fades the
// render alpha byte of ten tracked render-owner entities relative to the
// player's on-field position.
//
// The callback is registered through the scheduler (0x449ed0) by the
// manager factory at 0x418e30 (`push 0x4198a0`), so the scheduler hands the
// record in ECX and the thunk moves it into EBX; the body itself is a
// usercall over EBX.
//
// Manager record fields used here:
//   +0x0d8 stage gate dword (a mismatch with dword_474c84 resets +0x14)
//   +0x0dc + i*4   tracked entity ids (10 slots)
//   +0x104 + i*0xc tracked position pairs (float x, float y, 4 pad)
//   +0x17c + i*4   tracked half widths (floats; compared halved)
//   +0x10/+0x14    frame counters incremented on success
//   +0x1c + shotmode*0xc  player-attached object passed to 0x419120
//
// Entity fields touched: +0x40/+0x50 size floats, +0x12c skip flag,
// +0x310 scalar (i32; low byte drives the alpha), +0x2ff alpha byte.
#include <cmath>

#include "Th10Platform.hpp"
#include "Th10Types.hpp"
#include "LargeRenderOwnerLayout.hpp"
#include "PlayerRecord.hpp"

namespace th10 {

namespace {

// TH10 dword_00474c84 - stage progress gate.
extern u32 g_StageProgress84; // TH10 dword_00474c84
// TH10 dword_00474c7c - player shot mode (selects the +0x1c slot triplet).
extern u32 g_PlayerShotMode7c; // TH10 dword_00474c7c
// TH10 DAT_00477834 - player state block; +0x3c0/+0x3c4 are the player's
// field-space position pair (relative to the field centre).
extern u8 *g_PlayerStateBlock834; // TH10 DAT_00477834
// TH10 DAT_00491c10 - render owner; the entity lookup walks the two list
// heads at +0x72dad4 and +0x72dadc.
extern u8 *g_RenderOwner910; // TH10 DAT_00491c10

// Field-centre offsets added to the player position (0x470b4c = 224.0,
// 0x470b48 = 16.0) and the fade constants (0x470bcc = 32.0 halo,
// 0x470d20 = 1/32 fade step, 0x470b0c = 0.5).
const float k_field_center_x = 224.0f; // TH10 0x470b4c
const float k_field_center_y = 16.0f;  // TH10 0x470b48
const float k_halo = 32.0f;            // TH10 0x470bcc
const float k_fade_step = 0.03125f;    // TH10 0x470d20
const float k_half = 0.5f;             // TH10 0x470b0c

// TH10 0x00419120 (larger helper, not reconstructed here): advances the
// player-attached object passed in EAX, expiring tracked ids on the record
// (EBX, stack argument 1) and integrating its position.
void UpdatePlayerAttachedObjectEaxStackAbi(void *object, void *record,
                                           i32 argument);

// TH10 0x419650 body (defined below; the 0x4198a0 thunk forwards to it).
i32 UpdateTrackedEntityProximityFadeEdiAbi(void *record);

// TH10 __ftol2 (0x463b2c): x87 truncating float->int conversion.
i32 FloatToI32Truncate(float value)
{
    return static_cast<i32>(value);
}

// Signed divide-by-4 idiom of the native (lea *3 / cdq / and 3 / sar 2).
i32 TimesThreeDivFour(i32 value)
{
    return (value * 3 + (value < 0 ? 3 : 0)) / 4;
}

u32 LoadU32At(u32 address)
{
    return *reinterpret_cast<const u32 *>(address);
}

float LoadFloatAt(u32 address)
{
    return *reinterpret_cast<const float *>(address);
}

// Walk the two render-owner lists looking for the entity whose first dword
// equals `id`; returns the entity or null.
u8 *FindRenderEntity(u32 id)
{
    LargeRenderOwnerLayout &owner =
        *reinterpret_cast<LargeRenderOwnerLayout *>(g_RenderOwner910);
    for (OwnerLink *node = owner.first_list_a; node != 0U;
         node = node->next) {
        u8 *const entity = static_cast<u8 *>(node->self_node);
        if (*reinterpret_cast<const u32 *>(entity) == id)
            return entity;
    }
    for (OwnerLink *node = owner.first_list_b; node != 0U;
         node = node->next) {
        u8 *const entity = static_cast<u8 *>(node->self_node);
        if (*reinterpret_cast<const u32 *>(entity) == id)
            return entity;
    }
    return 0;
}

} // namespace

// TH10 0x4198a0 - scheduler thunk: ECX = record -> EBX. The native thunk
// does not touch the return value (it returns whatever eax held, i.e. the
// scheduler's prior eax); the reconstruction returns the body's 1.
i32 TH10_FASTCALL TimelineVmProximityFadeThunkEcxAbi(void *record)
{
    return UpdateTrackedEntityProximityFadeEdiAbi(record);
}

// TH10 0x419650. Native EBX = the manager record; always returns 1.
//
// For each of the ten slots the tracked entity's alpha byte (+0x2ff) is
// derived from its scalar (+0x310) and the player position relative to the
// slot rectangle (half width from +0x17c * 0.5, half height from the
// entity's +0x40 * +0x50 * 0.5 size, both extended by the 32-pixel halo).
// With d1 = |dx|, d2 = |dy|, h = half width, e = half height, s = scalar
// and step = s * 3 / 4 (scaled by 1/32 when applied):
//   d1 <  h, any y              -> alpha = low byte of s
//   d1 >= h, d2 <  e            -> alpha = (low byte of s) >> 2
//   d1 >= h, d2 <  e+32         -> alpha = low byte of s
//   d1 >= h, d2 >= e+32         -> alpha = s - (e+32-d2) * step / 32
// The native also carries an x-halo fade (s - (h+32-d1) * step / 32 for
// d1 >= h+32 && d2 < e) that is unreachable behind the earlier d1 >= h
// branch; it is preserved below for fidelity.
i32 UpdateTrackedEntityProximityFadeEdiAbi(void *record_raw)
{
    u8 *const record = static_cast<u8 *>(record_raw);

    // Stage change resets the secondary counter.
    if (LoadU32At(reinterpret_cast<u32>(record) + 0xd8U) != g_StageProgress84)
        *reinterpret_cast<u32 *>(record + 0x14U) = 0U;

    // Advance the player-attached object of the current shot mode.
    const u32 mode = g_PlayerShotMode7c;
    UpdatePlayerAttachedObjectEaxStackAbi(
        *reinterpret_cast<void **>(reinterpret_cast<u32>(record) + 0x1cU
                                   + mode * 0xcU),
        record, 1);

    const PlayerRecord &player_rec =
        *reinterpret_cast<const PlayerRecord *>(g_PlayerStateBlock834);
    const float player_x = player_rec.position_x + k_field_center_x;
    const float player_y = player_rec.position_y + k_field_center_y;

    u32 slot_offset = 0U; // over +0xdc ids (4 bytes each)
    u32 pair_offset = 0U; // over +0x104 pairs (0xc bytes each)
    for (i32 remaining = 10; remaining != 0; --remaining) {
        u8 *entity = 0;
        const u32 id
            = LoadU32At(reinterpret_cast<u32>(record) + 0xdcU + slot_offset);
        if (id != 0U)
            entity = FindRenderEntity(id);
        if (entity == 0U) {
            // Lost entity: drop the id and leave the pair untouched.
            *reinterpret_cast<u32 *>(record + 0xdcU + slot_offset) = 0U;
        } else if (LoadU32At(reinterpret_cast<u32>(entity) + 0x12cU) == 0U) {
            // Native computes |player_y - pair.y| first (stored to the
            // stack), then |player_x - pair.x| stays on the FPU stack.
            const float distance_y
                = std::fabs(player_y
                            - LoadFloatAt(reinterpret_cast<u32>(record)
                                          + 0x104U + pair_offset));
            const float distance_x
                = std::fabs(player_x
                            - LoadFloatAt(reinterpret_cast<u32>(record)
                                          + 0x108U + pair_offset));

            const float half_width
                = LoadFloatAt(reinterpret_cast<u32>(record) + 0x17cU
                              + slot_offset)
                  * k_half;
            const float half_height
                = LoadFloatAt(reinterpret_cast<u32>(entity) + 0x40U)
                  * LoadFloatAt(reinterpret_cast<u32>(entity) + 0x50U)
                  * k_half;
            const i32 scalar = static_cast<i32>(
                LoadU32At(reinterpret_cast<u32>(entity) + 0x310U));
            const float step = static_cast<float>(TimesThreeDivFour(scalar));

            u8 alpha;
            // Native decision tree (fcom chains; unordered follows the
            // branch target exactly as the jp/jne masks do):
            //   A: d1 >= h && e > d2            -> alpha = low byte >> 2
            //   B1: d1 >= h+32 && e > d2        -> x-penetration fade
            //      (unreachable: A already consumed d1 >= h with e > d2)
            //   C: d1 < h                       -> keep
            //   C1: d1 >= h && d2 < e+32        -> keep
            //   C1: d1 >= h && d2 >= e+32       -> y-penetration fade
            if (distance_x >= half_width && half_height > distance_y) {
                // Block 1 accepted: inside the x range, inside the size.
                alpha = static_cast<u8>(static_cast<u8>(scalar & 0xffU) >> 2);
            } else if (distance_x < half_width) {
                // Blocks 1/3: d1 < h reaches the keep tail both ways.
                alpha = static_cast<u8>(scalar & 0xffU);
            } else if (half_height > distance_y) {
                // Block B1 body: d1 >= h+32 with e > d2 fades up by the x
                // penetration beyond the halo. Unreachable in practice
                // because block 1 (d1 >= h && e > d2) is checked first;
                // preserved for fidelity.
                alpha = static_cast<u8>(FloatToI32Truncate(
                    static_cast<float>(scalar)
                    - (half_width + k_halo - distance_x) * step
                          * k_fade_step));
            } else if (distance_y < half_height + k_halo) {
                // Block C1 keep: y inside the extended size.
                alpha = static_cast<u8>(scalar & 0xffU);
            } else {
                // Block C1 compute: y beyond the extended size fades up by
                // the y penetration beyond the halo.
                alpha = static_cast<u8>(FloatToI32Truncate(
                    static_cast<float>(scalar)
                    - (half_height + k_halo - distance_y) * step
                          * k_fade_step));
            }

            *reinterpret_cast<u8 *>(entity + 0x2ffU) = alpha;
        }

        slot_offset += 4U;
        pair_offset += 0xcU;
    }

    ++*reinterpret_cast<u32 *>(record + 0x10U);
    ++*reinterpret_cast<u32 *>(record + 0x14U);
    return 1;
}

} // namespace th10
