// Stage text effects (TH10 0x0042b430 / 0x0042b4d0 / 0x0042b660 /
// 0x0042b6f0 / 0x0042b780 / 0x0042b9c0). The owner at DAT_00477840 renders
// the in-game floating digit counters (item-value popups): a ring of 0x2d0
// 0x40-byte records behind the owner's ASCII animation VM record, drawn
// through DrawAsciiAnimationVmUnscaledToOwner with a camera-distance glyph
// scale.
#include "StageTextEffects.hpp"

#include "AsciiAnimationVm.hpp"
#include "AsciiGlyphRenderer.hpp"
#include "CallbackScheduler.hpp"
#include "MainChainRuntime.hpp"
#include "ManagerReleaseWrappers.hpp"
#include "PlayerRecord.hpp"

namespace th10 {

namespace {

// TH10 0x452493 / 0x4524a1: operator new / operator delete.
u8 *AllocateHeapBlock(u32 bytes);
void FreeHeapBlock(void *pointer);

// Shared globals (addresses resolved against the binary).
extern void *g_MainChainRenderOwner;   // TH10 DAT_00491c10
extern void *g_ScreenTargetBlock;      // TH10 dword_477834 (camera host)
extern void *g_AsciiManagerHost;       // TH10 DAT_004776e0
extern CallbackScheduler *g_CallbackScheduler; // TH10 DAT_00491be4
extern u32 g_MainChainRuntimeOptions;  // TH10 DAT_00491d78
extern u32 g_AsciiFogEnableCache;      // TH10 DAT_00492378
extern void *g_TextEffectOwnerSlot;    // TH10 DAT_00477840

u32 LoadU32From(const void *address)
{
    return *static_cast<const u32 *>(address);
}

void StoreU32To(void *address, u32 value)
{
    *static_cast<u32 *>(address) = value;
}

float LoadF32From(const void *address)
{
    return *static_cast<const float *>(address);
}

void StoreF32To(void *address, float value)
{
    *static_cast<float *>(address) = value;
}

// TH10 0x463b2c (_ftol2): x87 float-to-int conversion.
i32 FloatToI32(float value)
{
    return static_cast<i32>(value);
}

const float k_glyph_scale_step = 8.0f;        // TH10 flt_00470bd0
const float k_y_spacing_half = 0.5f;          // TH10 flt_00470b0c
const float k_y_base_offset = 224.0f;         // TH10 flt_00470b4c
const float k_x_base_offset = 16.0f;          // TH10 flt_00470b48
const float k_smooth_rate_low = 0.99f;        // TH10 flt_00470b68
const float k_smooth_rate_high = 1.01f;       // TH10 flt_00470b64
const float k_smooth_rate_step = 1.0f;        // TH10 flt_00470afc
const float k_default_rate_table = 1.0f;      // TH10 float at 0x476f78

const u32 k_record_count = 0x2d3U; // scanned by tick/draw (3 past the ring!)
const u32 k_ring_slots = 0x2d0U;   // addressed by the +0x14 ring counter
const u32 k_record_stride = 0x40U;
const u32 k_record_base = 0x3c4U;

// Draw chain callback thunk 0x0042b9a0: mov eax,ecx; jmp 0x0042b6f0.
i32 TH10_FASTCALL TextEffectTickThunk(void *arg)
{
    return TickTextEffectRecordsEaxAbi(arg);
}

// Draw chain callback thunk 0x0042b9b0: push edi; mov edi,ecx; call
// 0x0042b780; pop edi; ret.
i32 TH10_FASTCALL TextEffectDrawThunk(void *arg)
{
    DrawTextEffectRecordsEdiAbi(arg);
    return 1;
}

} // namespace

// TH10 0x0042b430 semantic body.
void InitializeTextEffectOwnerEdxAbi(void *owner)
{
    u8 *const bytes = static_cast<u8 *>(owner);

    // The native body first clears VM flag bits at +0x84/+0xc8/+0x114/+0x140
    // (the "and 0xfffffffe" sweep over owner+0x18+...), resets the VM record,
    // stores 0xffff at VM +0x384, clears the +0x2c flag word of all 0x2d3
    // records and finally zeroes 0x2e21 dwords from the owner base. That
    // final rep stos erases every earlier store — dead stores preserved by
    // implementing only the observable tail.
    for (u32 i = 0; i < 0xb884U; ++i) {
        bytes[i] = 0U;
    }
    StoreU32To(bytes, LoadU32From(bytes) | 2U);
    g_TextEffectOwnerSlot = owner;
}

// TH10 0x0042b4d0 semantic body.
i32 RegisterTextEffectOwnerSchedulerRecordsEaxAbi(void *owner)
{
    u8 *const bytes = static_cast<u8 *>(owner);
    const u32 resource =
        LoadU32From(static_cast<const u8 *>(g_AsciiManagerHost) + 0x8994U);
    StoreU32To(bytes + 0x10U, resource);

    // Tick record: disabled (flag bit 1 cleared) when added, priority 15.
    ChainElem *const tick = CallbackSchedulerApi::Create(TextEffectTickThunk);
    tick->flags &= static_cast<u32>(~2U);
    tick->arg = owner;
    (void)CallbackSchedulerApi::AddToCalculationChain(g_CallbackScheduler,
                                                      tick, 0xf);
    StoreU32To(bytes + 0x8U, reinterpret_cast<u32>(tick));

    // Draw record: same disabled flag treatment, priority 0x27.
    ChainElem *const draw = CallbackSchedulerApi::Create(TextEffectDrawThunk);
    draw->flags &= static_cast<u32>(~2U);
    draw->arg = owner;
    (void)CallbackSchedulerApi::AddToDrawChain(g_CallbackScheduler, draw,
                                               0x27);
    StoreU32To(bytes + 0xcU, reinterpret_cast<u32>(draw));

    // VM reset and glyph entry 0xc4 rebind against the owner resource.
    u8 *const vm = bytes + 0x18U;
    ResetAsciiAnimationVmRecord(vm);
    StoreU32To(vm + 0x308U, resource);
    (void)InitializeAsciiAnimationVmEntry(vm, 0xc4U,
                                          reinterpret_cast<void *>(resource));
    return 0;
}

// TH10 0x0042b6f0 semantic body.
i32 TickTextEffectRecordsEaxAbi(void *owner)
{
    u8 *const bytes = static_cast<u8 *>(owner);
    for (u32 i = 0; i < k_record_count; ++i) {
        u8 *const record = bytes + k_record_base + i * k_record_stride;
        if (record[0x38U] == 0U) {
            continue;
        }

        // y rises by the constant 0.5 per frame.
        StoreF32To(record + 0x10U,
                   LoadF32From(record + 0x10U)
                       - k_default_rate_table * k_y_spacing_half);

        i32 frames = static_cast<i32>(LoadU32From(record + 0x20U));
        StoreU32To(record + 0x1cU, static_cast<u32>(frames));

        const float rate = LoadF32From(
            reinterpret_cast<const void *>(LoadU32From(record + 0x28U)));
        if (rate > k_smooth_rate_low && rate >= k_smooth_rate_high) {
            // Smooth path: constant one-frame step, integer frame counter.
            ++frames;
            StoreU32To(record + 0x20U, static_cast<u32>(frames));
            StoreF32To(record + 0x24U,
                       LoadF32From(record + 0x24U) + k_smooth_rate_step);
        } else {
            // Accumulator path (also taken for NaN rates via the native
            // fcomp/unordered tests): frames mirrors trunc(accumulator).
            const float updated = LoadF32From(record + 0x24U) + rate;
            StoreF32To(record + 0x24U, updated);
            StoreU32To(record + 0x20U,
                       static_cast<u32>(FloatToI32(updated)));
        }

        if (static_cast<i32>(LoadU32From(record + 0x20U)) > 0x3c) {
            record[0x38U] = 0U;
        }
    }
    return 1;
}

// TH10 0x0042b780 semantic body.
void DrawTextEffectRecordsEdiAbi(void *owner)
{
    u8 *const bytes = static_cast<u8 *>(owner);
    u8 *const vm = bytes + 0x18U;

    // Fog teardown: with runtime option bit 2 clear and the fog cache set,
    // flush the render owner's pending vertices, clear the cache and issue
    // device command 0x1c with parameter 0 through vtable slot 0x39.
    if ((g_MainChainRuntimeOptions & 4U) == 0U
        && g_AsciiFogEnableCache != 0U) {
        FlushRenderOwnerPendingVerticesEsiAbi(
            static_cast<RenderOwnerPartial *>(g_MainChainRenderOwner));
        g_AsciiFogEnableCache = 0U;
        void *const device = *reinterpret_cast<void *const *>(0x491c30U);
        if (device != 0) {
            void **const device_vtable = *static_cast<void ***>(device);
            typedef i32 (*DeviceCommand)(void *self, i32 command,
                                         i32 parameter);
            reinterpret_cast<DeviceCommand>(device_vtable[0x39])(
                device, 0x1c, 0);
        }
    }

    for (u32 i = 0; i < k_record_count; ++i) {
        u8 *const record = bytes + k_record_base + i * k_record_stride;
        if (record[0x38U] == 0U) {
            continue;
        }

        const i32 frames = static_cast<i32>(LoadU32From(record + 0x20U));
        float y_step;
        if (frames < 8) {
            y_step = k_glyph_scale_step / LoadF32From(record + 0x24U);
        } else {
            y_step = k_glyph_scale_step;
        }

        const u32 count = record[0x39U];
        const float start_y =
            LoadF32From(record + 0x0cU)
            - static_cast<float>(count) * y_step * k_y_spacing_half
            + k_y_base_offset;
        StoreF32To(bytes + 0x358U, start_y);
        StoreF32To(bytes + 0x35cU,
                   LoadF32From(record + 0x10U) + k_x_base_offset);
        StoreU32To(bytes + 0x314U, LoadU32From(record + 0x18U));

        // Glyph size from the squared camera distance (NaN distances fall
        // through the native signed compares into the "small" 80 branch).
        PlayerRecord &player =
            *reinterpret_cast<PlayerRecord *>(g_ScreenTargetBlock);
        const float dx = player.position_x
                       - LoadF32From(record + 0x0cU);
        const float dy = player.position_y
                       - LoadF32From(record + 0x10U);
        const i32 distance = FloatToI32(dx * dx + dy * dy);

        u32 glyph_size;
        if (distance > 0x1000) {
            glyph_size = 0xd0U;
        } else if (distance > 0x400) {
            // Native magic-constant division: ((distance - 0x400) << 7) *
            // 0x2aaaaaab >> 41, i.e. (distance - 0x400) / 24 truncated
            // toward zero, plus the 0x50 base — continuous with both
            // neighbors (0x50 at 0x400, 0xd0 at 0x1000).
            const i32 scaled = (distance - 0x400) / 24;
            glyph_size = static_cast<u32>(scaled) + 0x50U;
        } else {
            glyph_size = 0x50U;
        }
        const u8 size_byte = static_cast<u8>(glyph_size);

        u32 remaining = count;
        const u8 *cursor = record + count - 1U; // walks the text backwards
        while (remaining != 0U) {
            const u8 ch = *cursor;
            u32 glyph_index;
            if (frames >= 0x34 && ch != 0x0aU) {
                // Grown/tinted glyph rows once the effect has aged; the
                // second switch happens at frame 0x38.
                glyph_index = static_cast<u32>(ch)
                            + ((frames < 0x38) ? 0xcfU : 0xd9U);
            } else {
                glyph_index = static_cast<u32>(ch) + 0xc4U;
            }
            const u8 *const resource =
                reinterpret_cast<const u8 *>(LoadU32From(bytes + 0x10U));
            u8 *const glyph =
                reinterpret_cast<u8 *>(LoadU32From(resource + 0x118U))
                + glyph_index * 0x44U;
            StoreU32To(bytes + 0x3acU, reinterpret_cast<u32>(glyph));
            bytes[0x317U] = size_byte;
            StoreU32To(vm + 0x4cU, LoadU32From(glyph + 0x34U));
            StoreU32To(vm + 0x35cU, LoadU32From(vm + 0x35cU) | 8U);
            DrawAsciiAnimationVmUnscaledToOwner(vm, g_MainChainRenderOwner);

            StoreF32To(bytes + 0x358U,
                       LoadF32From(bytes + 0x358U) + y_step);
            --cursor;
            --remaining;
        }
    }
}

// TH10 0x0042b9c0 semantic body.
void AddTextEffectNumberEcxEaxEdiStackAbi(void *owner, i32 value, u32 param,
                                          const void *position)
{
    u8 *const bytes = static_cast<u8 *>(owner);
    u32 slot = LoadU32From(bytes + 0x14U);
    if (static_cast<i32>(slot) >= static_cast<i32>(k_ring_slots)) {
        slot = 0U; // native wrap: reset to 0 rather than modulo
    }
    u8 *const record = bytes + k_record_base + slot * k_record_stride;
    record[0x38U] = 1U;

    u32 count;
    if (value < 0) {
        // Negative values render the single byte '\n' (a blank glyph row).
        record[0x0U] = 0x0aU;
        count = 1U;
    } else if (value == 0) {
        record[0x0U] = 0U;
        count = 1U;
    } else {
        // Decimal digits, low digit first at record +0.
        count = 0U;
        i32 rest = value;
        do {
            const i32 digit = rest % 10;
            rest /= 10;
            record[count] = static_cast<u8>(digit);
            ++count;
        } while (rest != 0);
    }

    StoreU32To(record + 0x18U, param);
    record[0x39U] = static_cast<u8>(count);

    // First-use seeding of the timing block. The native's 0xfff0bdc1 store
    // at +0x1c is immediately overwritten with -1 below (dead store quirk).
    const u32 flags = LoadU32From(record + 0x2cU);
    if ((flags & 1U) == 0U) {
        StoreU32To(record + 0x2cU, flags | 1U);
        StoreU32To(record + 0x20U, 0U);
        StoreU32To(record + 0x1cU, 0xfff0bdc1U);
        StoreU32To(record + 0x24U, 0U);
        StoreU32To(record + 0x28U, 0x476f78U);
    }
    StoreU32To(record + 0x20U, 0U);
    StoreU32To(record + 0x24U, 0U);
    StoreU32To(record + 0x1cU, static_cast<u32>(-1));

    const u8 *const pos = static_cast<const u8 *>(position);
    StoreU32To(record + 0x0cU, LoadU32From(pos));
    StoreU32To(record + 0x10U, LoadU32From(pos + 4U));
    StoreU32To(record + 0x14U, LoadU32From(pos + 8U));

    StoreU32To(bytes + 0x14U, slot + 1U);
}

// TH10 0x0042b660. Allocates the 0xb884-byte text-effect owner, runs the
// in-place initializer (0x0042b430, EBX = owner, returning the owner) and
// the scheduler registration (0x0042b4d0) over it. Registration never
// fails in practice (0x0042b4d0 is a constant-0 return), but the native
// teardown path (in-place destructor 0x0042b570 + operator delete) is
// preserved for the nonzero branch. Returns the owner or 0.
void *CreateTextEffectOwner()
{
    u8 *owner = AllocateHeapBlock(0xb884U);
    if (owner != 0) {
        InitializeTextEffectOwnerEdxAbi(owner);
        if (RegisterTextEffectOwnerSchedulerRecordsEaxAbi(owner) != 0) {
            DestroyMainChainObject840InPlace(owner);
            FreeHeapBlock(owner);
            return 0;
        }
    }
    return owner;
}

} // namespace th10
