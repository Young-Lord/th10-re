// Spell/bullet base vtable neighborhood (TH10 0x408210 / 0x4084a0 /
// 0x408d60 / 0x409280 / 0x409c00). Every offset below is taken from the
// native disassembly of the corresponding entry.
//
// The spell/bullet base object (the pointer published at DAT_004776f4) is
// the story-mode spell-capture state machine: two ASCII VMs at +0x10 /
// +0x3bc, eight VMs at +0x778 and +0xa7c, five VMs at +0x24d8 (the tally
// cluster +0x24d8/+0x2884/+0x2c30/+0x2fdc/+0x3388), four published entity
// handles at +0x768/+0x76c/+0x770/+0x774, the scaled timer block
// {prev +0x3734, count +0x3738, accumulator +0x373c, rate ptr +0x3740},
// the spell name at +0x3748, the card index +0x3788, the flag word +0x378c,
// the bonus pair +0x3790/+0x3794, the decay parameter +0x3798 and the
// followed vec3 +0x379c. The destructor 0x00408af0 (GameModeTeardown.cpp)
// releases exactly these blocks.
//
// The bullet records live in the DAT_004776f0 effect manager root: 2000
// records of 0x7f0 bytes at +0x60 with the state word at +0x446, the
// position at +0x3b4/+0x3b8, the descriptor pointer at +0x39c, the delete
// bookkeeping dword at +0x450, the half-extent pair at +0x3f0/+0x3f4 and
// the signed difficulty word at +0x7ec.

#include "SpellBulletVtable.hpp"

#include "AsciiAnimationVm.hpp"
#include "BgmRuntime.hpp"
#include "EntityHelpers.hpp"
#include "LargeRenderOwnerLayout.hpp"
#include "PlayerFrameworkHelpers.hpp"
#include "PlayerShotData.hpp"
#include "ResultScreenState.hpp"
#include "StageEffectHelpers.hpp"
#include "TimelineRenderObjectSetup.hpp"
#include "TimelineRenderObjects.hpp"
#include "VmRecord.hpp"

namespace th10 {

namespace {

// ---- globals (absolute addresses; native names preserved) ----------------

extern void *g_MainChainRenderOwner;  // TH10 DAT_00491c10
extern void *g_AsciiManagerHost;      // TH10 DAT_004776e0
extern void *g_SpellBannerFlagOwner;  // TH10 DAT_004776e8 (+0x2a18 flag word)
extern void *g_StageRecordHolder;     // TH10 DAT_004776f0 (effect manager root)
extern void *g_AsciiHudOwner;         // TH10 DAT_0047770c
extern void *g_BossBattleState;       // TH10 DAT_00477704 (pointer holder)
extern void *g_ExplosionManager;      // TH10 DAT_00477818
extern void *g_OptionPositionManager; // TH10 DAT_00477834 (+0x3c4 player Y)
extern void *g_GameModeObject;        // TH10 DAT_00477838 (+0x10 game mode)
extern void *g_TitleScoreSaveRecord;  // TH10 DAT_0047783c
extern i32 g_CurrentScore;            // TH10 DAT_00474c44 (clamp 999999999)
extern i32 g_ScorePool;               // TH10 DAT_00474c4c
extern u32 g_PlayerCharacter;         // TH10 DAT_00474c68
extern u32 g_PlayerShotType;          // TH10 DAT_00474c6c
extern u32 g_StageIndex;              // TH10 DAT_00474c7c (1..7)
extern u32 g_StageSubState;           // TH10 DAT_00474c84
extern TransitionRootPartial g_TransitionRoot; // TH10 DAT_00492590

// ---- boundaries ----------------------------------------------------------

// TH10 0x00447ae0 (the right-aligned tip-text entry; HintTextFile.cpp keeps
// the body a boundary). Native stdcall ret 0xc with the resolved entity in
// EAX and ESI (null allowed); the stack carries {manager, 0xffffff color,
// spell name}. 0x409280 calls it after resolving the +0x76c handle.
void SpellCardTipTextEaxStackAbi(void *entity, void *manager, u32 color,
                                 const char *text);

// TH10 0x004172e0 is implemented in src/ResultScreenState.cpp
// (ApplyResultScreenStateEaxStackAbi); 0x409c00 calls it with mode 0 after a
// captured spell's bonus was added to the score.

// ---- float constants (bit patterns read from .rdata) ---------------------

inline float FloatFromBits(u32 bits)
{
    union {
        u32 u;
        float f;
    } converter;
    converter.u = bits;
    return converter.f;
}

const float kHalf = FloatFromBits(1056964608U);        // flt_470b0c = 0.5f
const float kRateLow = FloatFromBits(1065185444U);     // flt_470b68 = 0.99f
const float kRateHigh = FloatFromBits(1065437102U);    // flt_470b64 = 1.01f
const float kOne = FloatFromBits(1065353216U);         // flt_470afc = 1.0f
const float kPhaseALine = FloatFromBits(1119879168U);  // flt_470c80 = 96.0f
const float kPhaseBLine = FloatFromBits(1124073472U);  // flt_470bf4 = 128.0f
const float kFollowScale = FloatFromBits(1028443341U); // flt_470c84 = 0.05f

// ---- field access helpers ------------------------------------------------

u32 LoadU32At(const void *base, u32 offset)
{
    return *reinterpret_cast<const u32 *>(
        static_cast<const u8 *>(base) + offset);
}

void StoreU32At(void *base, u32 offset, u32 value)
{
    *reinterpret_cast<u32 *>(static_cast<u8 *>(base) + offset) = value;
}

i32 LoadI32At(const void *base, u32 offset)
{
    return static_cast<i32>(LoadU32At(base, offset));
}

u8 LoadU8At(const void *base, u32 offset)
{
    return *reinterpret_cast<const u8 *>(
        static_cast<const u8 *>(base) + offset);
}

u16 LoadU16At(const void *base, u32 offset)
{
    return *reinterpret_cast<const u16 *>(
        static_cast<const u8 *>(base) + offset);
}

void StoreU16At(void *base, u32 offset, u16 value)
{
    *reinterpret_cast<u16 *>(static_cast<u8 *>(base) + offset) = value;
}

float LoadFloatAt(const void *base, u32 offset)
{
    return *reinterpret_cast<const float *>(
        static_cast<const u8 *>(base) + offset);
}

void StoreFloatAt(void *base, u32 offset, float value)
{
    *reinterpret_cast<float *>(static_cast<u8 *>(base) + offset) = value;
}

void *LoadPointerAt(u32 address)
{
    return *reinterpret_cast<void **>(address);
}

// TH10 __ftol2 (0x463b2c): x87 truncating float->int conversion.
i32 FloatToI32Truncate(float value)
{
    return static_cast<i32>(value);
}

// Byte copy including the terminator, exactly like the native loops.
void CopyTerminatedString(char *dst, const char *src)
{
    char byte;
    do {
        byte = *src++;
        *dst++ = byte;
    } while (byte != '\0');
}

// Walks the render-owner list-A/list-B chains (nodes {entity, next}) and
// returns the entity whose u16 script word at +0x38a matches. The native
// inlines this scan twice in 0x409280 (words 0x19f / 0x1a0).
u8 *FindEntityByScriptWord(u16 script_word)
{
    const LargeRenderOwnerLayout &owner =
        *static_cast<const LargeRenderOwnerLayout *>(g_MainChainRenderOwner);
    const OwnerLink *const heads[2] = {owner.first_list_a,
                                       owner.first_list_b};
    for (u32 list_index = 0; list_index != 2; ++list_index) {
        for (const OwnerLink *node = heads[list_index]; node != 0;
             node = node->next) {
            u8 *const entity = static_cast<u8 *>(node->self_node);
            if (entity != 0
                && reinterpret_cast<const VmRecord *>(entity)
                       ->bound_script_id
                       == script_word) {
                return entity;
            }
        }
    }
    return 0;
}

// Pool spawn used by 0x409280: allocate a 0x3ac record from the render
// owner pool, arm flag 0x40000000, set +0x20 = 0xf, bind the script (the
// native passes its resource manager-work in ECX; the semantic body binds
// through the runtime-filled g_EffectScriptContext), then link the record
// into owner list A with the next id. The native stores the id both at
// entity+0 (inside 0x4489d0) and at the caller's handle slot; on the final
// (rank entity) spawn the native additionally writes the id over its
// incoming third stack-argument slot, which the reconstruction models with
// the returned id instead (the clobbered slot is popped by ret 0x10).
u32 SpawnSpellPracticeEntity(void *resource, i32 script_id)
{
    u8 *const record = static_cast<u8 *>(
        AllocatePoolVmEsiAbi(g_MainChainRenderOwner));
    VmRecord &vm = *reinterpret_cast<VmRecord *>(record);
    vm.flags |= 0x40000000U;
    vm.render_kind = 0xfU;
    AssignPoolVmScriptEcxEaxAbi(record, script_id);
    u32 id = 0;
    LinkEntityAndAssignIdEaxEsiAbi(&id, record);
    return id;
}

} // namespace

// TH10 0x00408210. Stdcall ret 4; EBX = manager, stack = explosion flag.
// Walks the 2000 bullet records (0x7f0 stride at manager+0x60) and deletes
// every state-1/2/4 record (the state word at +0x446 skips 0 and 3) whose
// AABB overlaps the manager's delete region built from the center trio
// +0x44/+0x48/+0x4c and half sizes +0x50*0.5/+0x54*0.5/+0x58*0.5. The x
// axis pairs with the +0x44 bound and the y axis with the +0x48 bound; the
// third bound is only forwarded to the effect spawn.
i32 ClearStageRegionBulletsEbxStackAbi(void *manager, i32 explosion_flag)
{
    u8 *const owner = static_cast<u8 *>(manager);

    const float region_x = LoadFloatAt(owner, 0x44U);
    const float region_y = LoadFloatAt(owner, 0x48U);
    const float region_z = LoadFloatAt(owner, 0x4cU);
    const float half_x = LoadFloatAt(owner, 0x50U) * kHalf;
    const float half_y = LoadFloatAt(owner, 0x54U) * kHalf;
    const float half_z = LoadFloatAt(owner, 0x58U) * kHalf;
    const float min_x_bound = region_x - half_x;
    const float max_x_bound = half_x + region_x;
    const float min_y_bound = region_y - half_y;
    const float max_y_bound = half_y + region_y;
    const float max_z_bound = half_z + region_z;

    u8 *record = owner + 0x60U;
    for (u32 index = 0; index != 2000U; ++index, record += 0x7f0U) {
        const u16 state = LoadU16At(record, 0x446U);
        if (state == 0 || state == 3) {
            continue;
        }

        // The record's x is +0x3b4, y is +0x3b8; the half extents come
        // from +0x3f0/+0x3f4. Overlap with the region box deletes the
        // bullet. NaN operands take the delete path in the native (the
        // first/third checks jump only on a strict less-than) and the
        // plain C++ comparisons below reproduce that.
        const float pos_x = LoadFloatAt(record, 0x3b4U);
        const float pos_y = LoadFloatAt(record, 0x3b8U);
        const float half_w = LoadFloatAt(record, 0x3f0U) * kHalf;
        const float half_h = LoadFloatAt(record, 0x3f4U) * kHalf;
        if (half_w + pos_x < min_x_bound) {
            continue;
        }
        if (pos_x - half_w > max_x_bound) {
            continue;
        }
        if (half_h + pos_y < min_y_bound) {
            continue;
        }
        if (pos_y - half_h > max_y_bound) {
            continue;
        }

        // Delete: flag bit 3 on the record, optional explosion, the stage
        // effect at the region's far corner, and the per-difficulty color
        // write onto the spawned effect entity.
        StoreU32At(record, 0U, LoadU32At(record, 0U) | 8U);
        if (explosion_flag != 0) {
            SpawnExplosionParticleEaxEcxEfxAbi(
                g_ExplosionManager, record + 0x3b4U, 8, 0xffffffffU,
                FloatFromBits(0xbfc90fdbU), // -1.25f
                FloatFromBits(0x3f19999aU)); // 0.8f
        }

        // The native passes the {max_x_bound, max_y_bound, max_z_bound}
        // trio as both the effect position and the out-id slot (the id
        // lands in the first dword).
        float out_position[3] = { max_x_bound, max_y_bound, max_z_bound };
        SpawnStageEffectEdxEbxAbi(out_position,
                                    reinterpret_cast<const float *>(record + 0x3b4U),
                                    0x173);
        const u32 effect_id = *reinterpret_cast<const u32 *>(out_position);
        u8 *const effect_entity = static_cast<u8 *>(FindEntityEdxStackAbi(
            g_MainChainRenderOwner, effect_id));

        const u8 *const descriptor = *reinterpret_cast<u8 *const *>(
            record + 0x39cU);
        if (descriptor != 0) {
            const float size = LoadFloatAt(descriptor, 0x34U);
            const i32 difficulty = static_cast<i16>(
                LoadU16At(record, 0x7ecU));
            u32 color;
            // fcomp 16/32 with the parity test: greater-than (or NaN)
            // walks down to the next table. NaN therefore reaches the
            // four-color table.
            if (size > 16.0f || size != size) {
                if (size > 32.0f || size != size) {
                    color = *reinterpret_cast<const u32 *>(
                        0x4743b0U + difficulty * 4U);
                } else {
                    color = *reinterpret_cast<const u32 *>(
                        0x474390U + difficulty * 4U);
                }
            } else {
                color = *reinterpret_cast<const u32 *>(
                    0x474350U + difficulty * 4U);
            }
            // Unchecked entity: the native stores through a null pointer
            // when the id does not resolve (address 0x2fc).
            reinterpret_cast<VmRecord *>(effect_entity)->primary_color = color;
        }
        StoreU32At(record, 0x450U, 0U);
    }
    return 0;
}

// TH10 0x004084a0. Native ECX = manager, EDI = {x, y}, stack = radius
// (squared on entry). For every active bullet record the four corner
// circles (radius = argument) centered on the record's AABB corners are
// tested against the point; a hit adds the record's descriptor-size item
// value to the total: <=8 -> 1, <=16 -> 1, <=32 -> 4, <=64 -> 10, larger
// (or a NaN size) -> 0. Records without a descriptor are skipped.
i32 SumBulletCornerItemValueEcxEdiStackAbi(void *manager,
                                           const float point[2],
                                           float radius)
{
    u8 *const owner = static_cast<u8 *>(manager);
    const float radius_sq = radius * radius;
    const float point_x = point[0];
    const float point_y = point[1];
    i32 total = 0;

    // ecx walks record+0x3f0 (the half-extent pair) with a 0x7f0 stride.
    u8 *record = owner + 0x3f0U;
    for (u32 index = 0; index != 2000U; ++index, record += 0x7f0U) {
        const u16 state = LoadU16At(record, 0x56U); // +0x446
        if (state == 0 || state == 3) {
            continue;
        }

        // Extents, reproducing the native operand order: the two max
        // corners are computed twice (from the center +/- the extent and
        // from min + extent) and can differ in the last ulp.
        const float pos_x = LoadFloatAt(record, -0x3cU); // +0x3b4
        const float pos_y = LoadFloatAt(record, -0x38U); // +0x3b8
        const float half_w = LoadFloatAt(record, 0U) * kHalf;
        const float half_h = LoadFloatAt(record, 4U) * kHalf;
        const float min_x = pos_x - half_w;
        const float max_x = half_w + pos_x;
        const float min_y = pos_y - half_h;          // stored at +0x18
        const float max_y_from_half = half_h + pos_y; // stored at +0xc/+0x30
        const float max_y_from_min = min_y + LoadFloatAt(record, 4U);
        const float min_y_from_max = max_y_from_half
                                         - LoadFloatAt(record, 4U);

        // Corner 1 (min_x, min_y): inside -> descriptor path. The native
        // treats an unordered comparison as a hit ("not greater than"),
        // hence the negated greater-than test.
        float dx = point_x - min_x;
        float dy = point_y - min_y;
        if (!(dy * dy + dx * dx > radius_sq)) {
            // handled below (descriptor/size path)
        } else {
            // Corner 2 (min_x, max_y from min + extent).
            dy = point_y - max_y_from_min;
            if (!(dy * dy + dx * dx > radius_sq)) {
                // handled below
            } else {
                // Corner 3 (max_x, max_y from half + center).
                dx = point_x - max_x;
                dy = point_y - max_y_from_half;
                if (!(dy * dy + dx * dx > radius_sq)) {
                    // handled below
                } else {
                    // Corner 4 (max_x, min_y from max - extent).
                    dy = point_y - min_y_from_max;
                    if (!(dy * dy + dx * dx > radius_sq)) {
                        // handled below
                    } else {
                        // Outside every corner circle: next record.
                        continue;
                    }
                }
            }
        }

        const u8 *const descriptor = *reinterpret_cast<u8 *const *>(
            record - 0x54U); // +0x39c
        if (descriptor == 0) {
            continue;
        }
        const float size = LoadFloatAt(descriptor, 0x34U);
        if (size > 8.0f || size != size) {
            if (size > 16.0f || size != size) {
                if (size > 32.0f || size != size) {
                    if (size > 64.0f || size != size) {
                        // larger than 64: no value (loop continues)
                    } else {
                        total += 10;
                    }
                } else {
                    total += 4;
                }
            } else {
                total += 1;
            }
        } else {
            total += 1;
        }
    }
    return total;
}

// TH10 0x00408d60 (via thunk 0x00409220). Runs while the capture flag
// (base+0x378c bit 0) is set: banner flag, bonus decay, VM ticks, the
// scaled timer, the two-phase defeat state machine, the bonus tally
// digits, and the boss-position follow.
i32 UpdateSpellCardStoryStateEcxAbi(void *base_memory)
{
    u8 *const base = static_cast<u8 *>(base_memory);
    if ((LoadU8At(base, 0x378cU) & 1U) == 0U) {
        return 1;
    }

    if (LoadI32At(base, 0x3738U) >= 60) {
        StoreU32At(LoadPointerAt(0x4776e8U), 0x2a18U,
                   LoadU32At(LoadPointerAt(0x4776e8U), 0x2a18U)
                       & 0xfffffffeU);
    }

    if (LoadI32At(base, 0x3738U) >= 300
        && (LoadU8At(base, 0x378cU) & 8U) == 0U) {
        // Decay the running bonus by 10% per step, scaled against the
        // +0x3798 parameter (which 0x409280 seeded with the payload id).
        // A +0x3798 value of exactly 300 divides by zero (native quirk).
        const i32 max_bonus = LoadI32At(base, 0x3794U);
        const i32 decay_step = (max_bonus - max_bonus / 10)
                                   / (LoadI32At(base, 0x3798U) - 300);
        i32 bonus = LoadI32At(base, 0x3790U) - decay_step;
        bonus -= bonus % 10; // stay on a multiple of ten
        StoreU32At(base, 0x3790U, static_cast<u32>(bonus));
    }

    FinalizeTimelineRenderObjectSetup(base + 0x10U);
    FinalizeTimelineRenderObjectSetup(base + 0x3bcU);

    // Scaled timer block {prev +0x3734, count +0x3738, accum +0x373c,
    // rate pointer +0x3740}: unity-rate window 0.99 < rate < 1.01 ticks
    // count and accumulator by one; otherwise the accumulator absorbs the
    // rate and the count re-derives through __ftol2.
    StoreU32At(base, 0x3734U, LoadU32At(base, 0x3738U));
    const float rate = LoadFloatAt(LoadPointerAt(LoadU32At(base, 0x3740U)),
                                    0U);
    if (rate > kRateLow && rate < kRateHigh) {
        StoreFloatAt(base, 0x373cU,
                     LoadFloatAt(base, 0x373cU) + kOne);
        StoreU32At(base, 0x3738U,
                   LoadU32At(base, 0x3738U) + 1U);
    } else {
        const float accumulated = rate + LoadFloatAt(base, 0x373cU);
        StoreFloatAt(base, 0x373cU, accumulated);
        StoreU32At(base, 0x3738U,
                   static_cast<u32>(FloatToI32Truncate(accumulated)));
    }

    if (LoadI32At(base, 0x3738U) >= 120) {
        const float player_y = LoadFloatAt(
            LoadPointerAt(0x477834U), 0x3c4U);
        if ((LoadU8At(base, 0x378cU) & 4U) == 0U) {
            // Phase A: any ordered comparison against 96 proceeds (only
            // NaN is rejected); mark state 3 and latch bit 2.
            if (player_y == player_y) {
                for (u32 offset = 0x768U; offset <= 0x770U; offset += 4U) {
                    SetEntityStateWordEaxEsiAbi(
                        reinterpret_cast<u32 *>(base + offset), 3);
                }
                for (u32 index = 0; index != 8U; ++index) {
                    reinterpret_cast<VmRecord *>(
                        base + 0xa7cU + index * 0x3acU)
                        ->state_word = 3;
                }
                for (u32 index = 0; index != 5U; ++index) {
                    reinterpret_cast<VmRecord *>(
                        base + 0x27dcU + index * 0x3acU)
                        ->state_word = 3;
                }
                StoreU32At(base, 0x378cU,
                           LoadU32At(base, 0x378cU) | 4U);
            }
        } else if (player_y > kPhaseBLine) {
            // Phase B: player below the 128 line (strictly greater);
            // mark state 2 and clear bit 2.
            for (u32 offset = 0x768U; offset <= 0x770U; offset += 4U) {
                SetEntityStateWordEaxEsiAbi(
                    reinterpret_cast<u32 *>(base + offset), 2);
            }
            for (u32 index = 0; index != 8U; ++index) {
                reinterpret_cast<VmRecord *>(
                    base + 0xa7cU + index * 0x3acU)
                    ->state_word = 2;
            }
            for (u32 index = 0; index != 5U; ++index) {
                reinterpret_cast<VmRecord *>(
                    base + 0x27dcU + index * 0x3acU)
                    ->state_word = 2;
            }
            StoreU32At(base, 0x378cU,
                       LoadU32At(base, 0x378cU) & 0xfffffffbU);
        }
    }

    if ((LoadU32At(base, 0x378cU) & 2U) != 0U) {
        // Bonus tally digits over the eight VMs at +0x778: the first two
        // carry the ten-millions digit and the rest of the value; the
        // native then keeps dividing by one, so VMs 2..7 repeat the low
        // part and VMs 3..7 land on entry 0x1e.
        i32 value = LoadI32At(base, 0x3790U);
        i32 divisor = 10000000;
        for (u32 index = 0; index != 8U; ++index) {
            const i32 quotient = value / divisor;
            const i32 remainder = value % divisor;
            void *const resource = LoadPointerAt(
                reinterpret_cast<u32>(LoadPointerAt(0x47770cU))
                + 0x9ec8U);
            InitializeAsciiAnimationVmEntry(base + 0x778U + index * 0x3acU,
                                            static_cast<u32>(quotient
                                                             + 0x1e),
                                            resource);
            FinalizeTimelineRenderObjectSetup(base + 0x778U
                                              + index * 0x3acU);
            divisor = 1;
            value = remainder;
        }

        // Per-card record digits: fields +0x624 and +0x628 of the current
        // card slot drive the five-VM tally cluster (+0x24d8/+0x2884/
        // +0x2fdc/+0x3388 initialized, +0x2c30 only ticked).
        void *const resource = LoadPointerAt(
            reinterpret_cast<u32>(LoadPointerAt(0x47770cU)) + 0x9ec8U);
        u8 *const score_record = static_cast<u8 *>(
            LoadPointerAt(0x47783cU));
        const u32 card_offset = (g_PlayerShotType + 3U * g_PlayerCharacter)
                                    * 0x437cU
                                + static_cast<u32>(
                                      LoadI32At(base, 0x3788U))
                                      * 0x90U;
        const u8 *card = score_record + card_offset;
        i32 field = LoadI32At(card, 0x624U);
        if (field >= 100) {
            field = 99;
        }
        InitializeAsciiAnimationVmEntry(base + 0x24d8U,
                                        static_cast<u32>(field / 10 + 0x1e),
                                        resource);
        InitializeAsciiAnimationVmEntry(base + 0x2884U,
                                        static_cast<u32>(field % 10 + 0x1e),
                                        resource);
        field = LoadI32At(card, 0x628U);
        if (field >= 100) {
            field = 99;
        }
        InitializeAsciiAnimationVmEntry(base + 0x2fdcU,
                                        static_cast<u32>(field / 10 + 0x1e),
                                        resource);
        InitializeAsciiAnimationVmEntry(base + 0x3388U,
                                        static_cast<u32>(field % 10 + 0x1e),
                                        resource);
        for (u32 index = 0; index != 5U; ++index) {
            FinalizeTimelineRenderObjectSetup(base + 0x24d8U
                                              + index * 0x3acU);
        }

        // Follow the battle record's +0x1068 position at 5% per frame and
        // publish it for the +0x774 entity. The battle-record pointer is
        // dereferenced without a null check (native quirk).
        const float *const battle_position =
            reinterpret_cast<const float *>(
                reinterpret_cast<u32>(
                    *reinterpret_cast<void *const *>(
                        reinterpret_cast<u32>(LoadPointerAt(0x477704U))
                        + 0x10U))
                + 0x1068U);
        for (u32 axis = 0; axis != 3U; ++axis) {
            const float delta = battle_position[axis]
                                - LoadFloatAt(base, 0x379cU + axis * 4U);
            StoreFloatAt(base, 0x379cU + axis * 4U,
                         delta * kFollowScale
                             + LoadFloatAt(base, 0x379cU + axis * 4U));
        }
        SetEntityPositionOffsetEsiAbi(g_MainChainRenderOwner,
                                      LoadU32At(base, 0x774U),
                                      reinterpret_cast<const float *>(
                                          base + 0x379cU));
    }
    return 1;
}

// TH10 0x00409280. Stdcall ret 0x10. Native EAX = base, stack = {base,
// spell card index, spell name pointer, payload id}.
void StartSpellCardPracticeEaxStackAbi(void *base, i32 spell_card_index,
                                       const char *spell_name,
                                       i32 payload_id)
{
    u8 *const state = static_cast<u8 *>(base);

    // First-run timer block init (flag +0x3744 bit 0); note the native
    // immediately overwrites +0x3734 with -1 afterwards.
    if ((LoadU8At(state, 0x3744U) & 1U) == 0U) {
        StoreU32At(state, 0x3738U, 0U);
        StoreU32At(state, 0x3734U, 0xfff0bdc1U);
        StoreU32At(state, 0x373cU, 0U);
        StoreU32At(state, 0x3740U, 0x476f78U); // flt_476f78 = 1.0f
        StoreU32At(state, 0x3744U,
                   LoadU32At(state, 0x3744U) | 1U);
    }
    StoreU32At(state, 0x3738U, 0U);
    StoreU32At(state, 0x373cU, 0U);
    StoreU32At(state, 0x3734U, 0xffffffffU);
    StoreU32At(state, 0x3788U, static_cast<u32>(spell_card_index));
    CopyTerminatedString(reinterpret_cast<char *>(state + 0x3748U),
                         spell_name);
    StoreU32At(state, 0x378cU,
               (LoadU32At(state, 0x378cU) & 0xffffffe7U) | 3U);

    // Register the spell name (and bump the practice counters) in the
    // score-save record unless game mode == 1. The card slot address is
    // (shot + 3*character) * 0x437c + index * 0x90.
    if (LoadI32At(LoadPointerAt(0x477838U), 0x10U) != 1) {
        u8 *const score_record = static_cast<u8 *>(
            LoadPointerAt(0x47783cU));
        const u32 card_offset =
            (g_PlayerShotType + 3U * g_PlayerCharacter) * 0x437cU
            + static_cast<u32>(spell_card_index) * 0x90U;
        char *slot = reinterpret_cast<char *>(score_record + card_offset
                                              + 0x5a4U);
        CopyTerminatedString(slot, spell_name);
        u32 *const counter = reinterpret_cast<u32 *>(slot + 0x84U);
        if (*counter < 0x1869fU) {
            ++*counter;
        }
        char *all_slot = reinterpret_cast<char *>(score_record
                                                  + card_offset
                                                  + 0x19a8cU);
        CopyTerminatedString(all_slot, spell_name);
        u32 *const all_counter = reinterpret_cast<u32 *>(all_slot + 0x84U);
        if (*all_counter < 0x1869fU) {
            ++*all_counter;
        }
    }

    // Rebind the thirteen ASCII VMs: eight at +0x778 with scripts
    // 0x3a..0x41 and five at +0x24d8 with scripts 0x42..0x46.
    void *const hud_resource = LoadPointerAt(
        reinterpret_cast<u32>(LoadPointerAt(0x47770cU)) + 0x9ec8U);
    for (u32 index = 0; index != 8U; ++index) {
        AssignAnmScriptToVmEcxEaxBbxAbi(hud_resource,
                                        state + 0x778U + index * 0x3acU,
                                        static_cast<i32>(0x3aU + index));
    }
    for (u32 index = 0; index != 5U; ++index) {
        AssignAnmScriptToVmEcxEaxBbxAbi(hud_resource,
                                        state + 0x24d8U + index * 0x3acU,
                                        static_cast<i32>(0x42U + index));
    }

    // Three framework entities: script 1 (resource DAT_004776e0+0x8994),
    // script 0x48 (resource +0x899c), script 2 (resource +0x8994). The
    // +0x76c handle is validated right after the spawn.
    u8 *const host = static_cast<u8 *>(LoadPointerAt(0x4776e0U));
    StoreU32At(state, 0x768U,
               SpawnSpellPracticeEntity(host + 0x8994U, 1));
    StoreU32At(state, 0x76cU,
               SpawnSpellPracticeEntity(host + 0x899cU, 0x48));
    void *banner_entity = FindEntityEdxStackAbi(
        g_MainChainRenderOwner, LoadU32At(state, 0x76cU));
    if (banner_entity == 0) {
        StoreU32At(state, 0x76cU, 0U);
    }
    StoreU32At(state, 0x770U,
               SpawnSpellPracticeEntity(host + 0x8994U, 2));

    // Spell name banner through the tip-text entry (native EAX = ESI =
    // the resolved +0x76c entity, possibly null), then sound 0x0e.
    SpellCardTipTextEaxStackAbi(banner_entity, g_MainChainRenderOwner,
                                0xffffffU, spell_name);
    EnqueueBgmSoundValue(&g_TransitionRoot, 0x0eU, 0);

    // Rank entity (script 0x1a1, resource DAT_004776f0+0x3e0b50): seeded
    // with the battle record position and published with the game-area
    // offset, then validated.
    StoreU32At(state, 0x774U,
               SpawnSpellPracticeEntity(
                   LoadPointerAt(reinterpret_cast<u32>(
                                     LoadPointerAt(0x4776f0U))
                                 + 0x3e0b50U),
                   0x1a1));
    const u8 *const battle_record = *reinterpret_cast<u8 *const *>(
        reinterpret_cast<u32>(LoadPointerAt(0x477704U)) + 0x10U);
    for (u32 axis = 0; axis != 3U; ++axis) {
        StoreFloatAt(state, 0x379cU + axis * 4U,
                     LoadFloatAt(battle_record, 0x1068U + axis * 4U));
    }
    SetEntityPositionOffsetEsiAbi(g_MainChainRenderOwner,
                                  LoadU32At(state, 0x774U),
                                  reinterpret_cast<const float *>(
                                      state + 0x379cU));
    if (FindEntityEdxStackAbi(g_MainChainRenderOwner,
                              LoadU32At(state, 0x774U))
        == 0) {
        StoreU32At(state, 0x774U, 0U);
    }

    // Payload id into the 0x19f / 0x1a0 script entities and the decay
    // parameter. The native stores through the found pointer without a
    // null check (a miss writes to absolute 0x314) - quirk preserved.
    u8 *const script_19f = FindEntityByScriptWord(0x19f);
    reinterpret_cast<VmRecord *>(script_19f)->reg_10002 =
        static_cast<u32>(payload_id);
    u8 *const script_1a0 = FindEntityByScriptWord(0x1a0);
    reinterpret_cast<VmRecord *>(script_1a0)->reg_10002 =
        static_cast<u32>(payload_id);
    StoreU32At(state, 0x3798U, static_cast<u32>(payload_id));

    // Bonus seed: (rank*3 + 10) * score * 10, with the +0x3794 companion
    // capped at 99,999,999 (signed comparison).
    u32 bonus = (g_StageIndex * 3U + 10U) * static_cast<u32>(g_ScorePool)
                * 10U;
    StoreU32At(state, 0x3790U, bonus);
    StoreU32At(state, 0x3794U, bonus);
    if (static_cast<i32>(bonus) >= 0x5f5e100) {
        StoreU32At(state, 0x3794U, 0x5f5e0ffU);
    }

    // Fifth entity: script 0x1ab, same stage resource, id not published
    // to a handle slot (the native clobbers its incoming third stack
    // argument slot with it instead).
    SpawnSpellPracticeEntity(LoadPointerAt(reinterpret_cast<u32>(
                                 LoadPointerAt(0x4776f0U)) + 0x3e0b50U),
                             0x1ab);

    // Rank dispatch (rank = DAT_00474c7c - 1, jump table at 0x409be4).
    // Each rank initializes the base VM pair from DAT_00477704+0x38 and
    // spawns its rank entity; rank 6 branches on DAT_00474c84 >= 24 and
    // its low branch skips the second VM entirely (native quirk).
    void *const battle_resource = LoadPointerAt(
        reinterpret_cast<u32>(LoadPointerAt(0x477704U)) + 0x38U);
    const i32 rank = static_cast<i32>(g_StageIndex) - 1;
    switch (rank) {
    case 0: {
        InitializePlayerMainVmEsiStackAbi(state + 0x10U, battle_resource,
                                          0xc);
        InitializePlayerMainVmEsiStackAbi(state + 0x3bcU, battle_resource,
                                          0xb);
        SpawnSpellPracticeEntity(
            battle_resource,
            spell_card_index < 2 ? 0xe : 0xf);
        break;
    }
    case 1: {
        InitializePlayerMainVmEsiStackAbi(state + 0x10U, battle_resource,
                                          0xe);
        InitializePlayerMainVmEsiStackAbi(state + 0x3bcU, battle_resource,
                                          0xf);
        SpawnSpellPracticeEntity(battle_resource, 0x11);
        break;
    }
    case 2: {
        InitializePlayerMainVmEsiStackAbi(state + 0x10U, battle_resource,
                                          0x12);
        InitializePlayerMainVmEsiStackAbi(state + 0x3bcU, battle_resource,
                                          0x13);
        SpawnSpellPracticeEntity(battle_resource, 0x15);
        break;
    }
    case 3: {
        InitializePlayerMainVmEsiStackAbi(state + 0x10U, battle_resource,
                                          0x13);
        InitializePlayerMainVmEsiStackAbi(state + 0x3bcU, battle_resource,
                                          0x14);
        SpawnSpellPracticeEntity(battle_resource, 0x16);
        break;
    }
    case 4: {
        InitializePlayerMainVmEsiStackAbi(state + 0x10U, battle_resource,
                                          0xc);
        InitializePlayerMainVmEsiStackAbi(state + 0x3bcU, battle_resource,
                                          0xd);
        SpawnSpellPracticeEntity(battle_resource, 0xf);
        break;
    }
    case 5: {
        InitializePlayerMainVmEsiStackAbi(state + 0x10U, battle_resource,
                                          0x21);
        InitializePlayerMainVmEsiStackAbi(state + 0x3bcU, battle_resource,
                                          0x22);
        SpawnSpellPracticeEntity(battle_resource, 0x24);
        break;
    }
    case 6: {
        if (static_cast<i32>(g_StageSubState) >= 0x18) {
            InitializePlayerMainVmEsiStackAbi(state + 0x10U,
                                              battle_resource, 0x1b);
            InitializePlayerMainVmEsiStackAbi(state + 0x3bcU,
                                              battle_resource, 0x1c);
            SpawnSpellPracticeEntity(battle_resource, 0x1e);
        } else {
            // Low branch: only the first VM is initialized.
            InitializePlayerMainVmEsiStackAbi(state + 0x10U,
                                              battle_resource, 0x26);
            SpawnSpellPracticeEntity(battle_resource, 0x28);
        }
        break;
    }
    default:
        // Out-of-range rank: the native returns without any work.
        break;
    }
}

// TH10 0x00409c00. Native EAX = base. Clears the capture flag, expires the
// framework handles, releases the rank entity, and resolves the capture
// outcome.
void FinishSpellCardPracticeEaxAbi(void *base)
{
    u8 *const state = static_cast<u8 *>(base);
    if ((LoadU8At(state, 0x378cU) & 1U) == 0U) {
        return;
    }

    StoreU32At(LoadPointerAt(0x4776e8U), 0x2a18U,
               LoadU32At(LoadPointerAt(0x4776e8U), 0x2a18U) | 1U);

    ExpireEntityHandleEaxAbi(reinterpret_cast<u32 *>(state + 0x768U));
    ExpireEntityHandleEaxAbi(reinterpret_cast<u32 *>(state + 0x76cU));
    ExpireEntityHandleEaxAbi(reinterpret_cast<u32 *>(state + 0x770U));
    StoreU32At(state, 0x378cU,
               LoadU32At(state, 0x378cU) & 0xfffffffeU);
    ReleaseEntityById(g_MainChainRenderOwner, LoadU32At(state, 0x774U));
    StoreU32At(state, 0x774U, 0U);

    if ((LoadU32At(state, 0x378cU) & 2U) != 0U) {
        // Captured: add one tenth of the bonus (clamped at 999,999,999),
        // notify the result screen, bump the capture counters, sound 0x2d.
        const i32 bonus = LoadI32At(state, 0x3790U);
        i32 score = g_CurrentScore + bonus / 10;
        if (score >= 0x3b9aca00) {
            score = 0x3b9ac9ff;
        }
        g_CurrentScore = score;

        ApplyResultScreenStateEaxStackAbi(
            0, LoadPointerAt(0x47770cU), LoadI32At(state, 0x3790U));

        if (LoadI32At(LoadPointerAt(0x477838U), 0x10U) != 1) {
            u8 *const score_record = static_cast<u8 *>(
                LoadPointerAt(0x47783cU));
            const u32 card_offset =
                (g_PlayerShotType + 3U * g_PlayerCharacter) * 0x437cU
                + static_cast<u32>(LoadI32At(state, 0x3788U)) * 0x90U;
            u32 *const counter = reinterpret_cast<u32 *>(
                score_record + card_offset + 0x80U);
            if (*counter < 0x1869fU) {
                ++*counter;
            }
            u32 *const all_counter = reinterpret_cast<u32 *>(
                score_record + card_offset + 0x19a8cU + 0x80U);
            if (*all_counter < 0x1869fU) {
                ++*all_counter;
            }
        }
        EnqueueBgmSoundValue(&g_TransitionRoot, 0x2dU, 0);
        return;
    }

    // Not captured: release the HUD conditional's banner handle and respawn
    // its setup VM (script 0x47 in the shared body, script 0x48 here).
    u8 *const hud = static_cast<u8 *>(LoadPointerAt(0x47770cU));
    ReleaseEntityById(g_MainChainRenderOwner, LoadU32At(hud, 0x9e14U));
    StoreU32At(hud, 0x9e14U, 0U);
    i32 *const respawn = SpawnSetupEffectVmListABack(0x48, 0xfU);
    // Unchecked dereference of the spawned record (native quirk).
    StoreU32At(hud, 0x9e14U, static_cast<u32>(*respawn));
}

} // namespace th10
