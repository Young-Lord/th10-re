// Title-screen background scene script stream (0x403c80) and its calc-body
// caller (0x402720). The state block is the title-screen state whose
// destructor/teardown lives in TitleGameManagerLifecycle.cpp.
#include "TitleBackgroundScript.hpp"

#include <math.h>

#include "PlayerTimerHelpers.hpp"
#include "ManagerWork.hpp"
#include "TimelineRenderObjectSetup.hpp"
#include "VmLeafHelpers.hpp"

namespace th10 {

namespace {

extern void *g_MainChainRenderOwner; // TH10 DAT_00491c10 (manager-work owner)

// ---- globals -----------------------------------------------------------

extern float g_SharedRateFloat;        // TH10 flt_476f78 (default 1.0)
extern u32 g_PauseSnapshot[0x46];      // TH10 0x491d7c snapshot block
extern u32 g_TitleScriptCase13;        // TH10 dword_4923a8
extern char g_TitleScriptPath[];       // TH10 0x497c38 script path buffer

// Boundaries --------------------------------------------------------------

// TH10 0x452706: CRT malloc through the heap handle at 0x477364.
extern void *AllocateResourceBuffer(u32 bytes);

// TH10 0x44b360: whole-file load (EAX = path, stack = {&size, mode 0}).
extern void *LoadMainChainFile(const char *path, u32 *file_size,
                               i32 filesystem_mode);

// TH10 0x44b810: append a load-error line to the 0x474f70 error context.
extern void AppendSoundFileLoadError(const char *name);

inline u32 LoadU32(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const u32 *>(bytes + offset);
}

inline void StoreU32(u8 *bytes, u32 offset, u32 value)
{
    *reinterpret_cast<u32 *>(bytes + offset) = value;
}

inline i32 LoadI32(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const i32 *>(bytes + offset);
}

inline i16 LoadI16(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const i16 *>(bytes + offset);
}

inline float LoadFloat(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const float *>(bytes + offset);
}

inline void StoreFloat(u8 *bytes, u32 offset, float value)
{
    *reinterpret_cast<float *>(bytes + offset) = value;
}

inline void CopyVec3(u8 *dst, u32 dst_off, const u8 *src, u32 src_off)
{
    for (u32 i = 0; i != 3; ++i)
        StoreFloat(dst, dst_off + 4U * i, LoadFloat(src, src_off + 4U * i));
}

// Lazy timer-block initialization for the pointer-rate records
// {prev, count, acc, rate ptr, flags} guarded by flag bit 0.
void LazyInitPointerRateTimer(u8 *base, u32 offset)
{
    const u32 flags = LoadU32(base, offset + 0x10U);
    if ((flags & 1U) == 0U) {
        StoreU32(base, offset, 0xFFF0BDC1U);
        StoreU32(base, offset + 4U, 0U);
        StoreU32(base, offset + 8U, 0U);
        StoreU32(base, offset + 0xcU, 0x476F78U);
        StoreU32(base, offset + 0x10U, flags | 1U);
    }
}

// Shared scaled-timer epilogue (native inlines at 0x40426b / 0x415797 /
// 0x415154): in-window rates (0.99..1.01, NaN excluded by the fcomp parity
// chain) add 1.0 to the accumulator and one to the count; anything else
// adds the rate and truncates. Returns the new count.
i32 TickPointerRateTimer(u8 *base, u32 offset)
{
    const u32 count_off = offset + 4U;
    const u32 acc_off = offset + 8U;
    const float rate =
        *reinterpret_cast<const float *>(LoadU32(base, offset + 0xcU));
    i32 count = LoadI32(base, count_off);
    const bool in_window =
        !(rate < 0.99f || rate != rate) && !(rate > 1.01f || rate != rate);
    if (in_window) {
        StoreFloat(base, acc_off, LoadFloat(base, acc_off) + 1.0f);
        ++count;
    } else {
        StoreFloat(base, acc_off, LoadFloat(base, acc_off) + rate);
        count = static_cast<i32>(LoadFloat(base, acc_off));
    }
    StoreU32(base, count_off, static_cast<u32>(count));
    return count;
}

// Boundaries ---------------------------------------------------------------

// TH10 0x403990: title background pre-update (mode/palette fixups); the
// state stays live in EAX at the call site.
extern void TitleBackgroundPreUpdateAbi(void *state);

// TH10 0x4049a0 is implemented as TickColorTrack in VmLeafHelpers.cpp.

// TH10 0x405040: ramp/palette helper. Native inputs: the four record
// bytes as integer-converted floats on the stack plus the two record
// dwords; it fills the 7-dword scratch the caller copies to +0x104.
extern void TitleBackgroundRampAbi(float byte0, float byte1, float byte2,
                                   float byte3, i32 arg_a, i32 arg_b,
                                   u32 out_scratch[7]);

// TH10 0x404f30 variant used here: one stack argument (the VM manager
// bound at state+0x178); the native ECX input is the trashed multiply
// result. ESI = VM record.
extern void BindTitleScriptVmEsiStackAbi(void *vm, void *manager);

// TH10 D3DX import: D3DXVec3Normalize(out, in).
extern void *D3DXVec3NormalizeAbi(float out_vec3[3], const float in_vec3[3]);

} // namespace

// TH10 0x403c80.
i32 RunTitleBackgroundScriptStackAbi(void *state)
{
    u8 *const st = static_cast<u8 *>(state);

    {
        u8 *cur = *reinterpret_cast<u8 **>(st + 0x4cU);
        if (LoadI32(cur, 0) > LoadI32(st, 0x3cU))
            goto epilogue;
    }

    for (;;) {
        u8 *cur = *reinterpret_cast<u8 **>(st + 0x4cU);
        if (LoadI32(cur, 0) > LoadI32(st, 0x3cU))
            break;

        const i32 opcode = LoadI16(cur, 4U);
        bool advanced = false;
        switch (opcode) {
        case 1: {
            // Wait-N: arm the frame-count timer, advance by the payload
            // size (+8) and skip the default +6 step.
            LazyInitPointerRateTimer(st, 0x38U);
            const i32 frames = LoadI32(cur, 0xcU);
            StoreU32(st, 0x3cU, static_cast<u32>(frames));
            StoreU32(st, 0x38U, static_cast<u32>(frames - 1));
            StoreFloat(st, 0x40U, static_cast<float>(frames));
            *reinterpret_cast<u8 **>(st + 0x4cU) = cur + LoadU32(cur, 8U);
            advanced = true;
            break;
        }
        case 2: {
            // Position A: the old +0x2a4c vector moves to the +0x2b3c
            // slot, the record vector becomes the new target, and the
            // delta (new - old) is stored over the old slot.
            CopyVec3(st, 0x2b3cU, st, 0x2a4cU);
            const float old_x = LoadFloat(st, 0x2a4cU);
            const float old_y = LoadFloat(st, 0x2a50U);
            const float old_z = LoadFloat(st, 0x2a54U);
            CopyVec3(st, 0x2a4cU, cur, 8U);
            StoreFloat(st, 0x2b3cU, LoadFloat(st, 0x2a4cU) - old_x);
            StoreFloat(st, 0x2b40U, LoadFloat(st, 0x2a50U) - old_y);
            StoreFloat(st, 0x2b44U, LoadFloat(st, 0x2a54U) - old_z);
            break;
        }
        case 3: {
            // Position B target: record vec2 into +0xe0, record vec3 into
            // +0xa8, live +0x2a4c into +0x9c; arm the +0xcc timer.
            StoreU32(st, 0xe0U, LoadU32(cur, 8U));
            StoreU32(st, 0xe4U, LoadU32(cur, 0xcU));
            CopyVec3(st, 0x9cU, st, 0x2a4cU);
            CopyVec3(st, 0xa8U, cur, 0x10U);
            LazyInitPointerRateTimer(st, 0xccU);
            StoreU32(st, 0xd0U, 0U);
            StoreU32(st, 0xd4U, 0U);
            StoreU32(st, 0xccU, static_cast<u32>(-1));
            break;
        }
        case 4:
            CopyVec3(st, 0x2a58U, cur, 8U);
            break;
        case 5: {
            // Start interpolator A (+0x50): from the live +0x2a58 to the
            // record vec3; flags +0x94/+0x98 from the record.
            StoreU32(st, 0x94U, LoadU32(cur, 8U));
            StoreU32(st, 0x98U, LoadU32(cur, 0xcU));
            CopyVec3(st, 0x50U, st, 0x2a58U);
            CopyVec3(st, 0x5cU, cur, 0x10U);
            LazyInitPointerRateTimer(st, 0x80U);
            StoreU32(st, 0x84U, 0U);
            StoreU32(st, 0x88U, 0U);
            StoreU32(st, 0x80U, static_cast<u32>(-1));
            break;
        }
        case 6:
            CopyVec3(st, 0x2a64U, cur, 8U);
            break;
        case 7:
            StoreU32(st, 0x2a94U, LoadU32(cur, 8U));
            break;
        case 8: {
            // Color: dword payload plus its four bytes as floats.
            const u32 color = LoadU32(cur, 8U);
            StoreU32(st, 0x2b60U, color);
            StoreFloat(st, 0x2b50U, static_cast<float>(color & 0xffU));
            StoreFloat(st, 0x2b54U,
                       static_cast<float>((color >> 8) & 0xffU));
            StoreFloat(st, 0x2b58U,
                       static_cast<float>((color >> 16) & 0xffU));
            StoreFloat(st, 0x2b5cU,
                       static_cast<float>((color >> 24) & 0xffU));
            StoreU32(st, 0x2b48U, LoadU32(cur, 0xcU));
            StoreU32(st, 0x2b4cU, LoadU32(cur, 0x10U));
            break;
        }
        case 9: {
            // Color track: four record bytes as floats and the two record
            // dwords into the ramp helper, the 7-dword state copy into
            // +0xe8 and the ramp scratch into +0x104, flag +0x16c, timer
            // +0x158 armed and reset.
            u32 scratch[7];
            TitleBackgroundRampAbi(
                static_cast<float>(LoadU32(cur, 0x10U) & 0xffU),
                static_cast<float>(LoadU32(cur, 0x11U) & 0xffU),
                static_cast<float>(LoadU32(cur, 0x12U) & 0xffU),
                static_cast<float>(LoadU32(cur, 0x13U) & 0xffU),
                static_cast<i32>(LoadU32(cur, 0x14U)),
                static_cast<i32>(LoadU32(cur, 0x18U)), scratch);
            StoreU32(st, 0x16cU, LoadU32(cur, 8U));
            for (u32 i = 0; i != 7U; ++i)
                StoreU32(st, 0xe8U + 4U * i, LoadU32(st, 0x2b48U + 4U * i));
            for (u32 i = 0; i != 7U; ++i)
                StoreU32(st, 0x104U + 4U * i, scratch[i]);
            LazyInitPointerRateTimer(st, 0x158U);
            StoreU32(st, 0x15cU, 0U);
            StoreU32(st, 0x160U, 0U);
            StoreU32(st, 0x158U, static_cast<u32>(-1));
            StoreU32(st, 0x170U, LoadU32(cur, 0xcU));
            break;
        }
        case 10: {
            // Start interpolator B (+0x9c): from the live +0x2a4c with the
            // record's three vec3 targets (+0xb4 gate, +0xa8, +0xc0).
            StoreU32(st, 0x94U, LoadU32(cur, 8U));
            StoreU32(st, 0xe0U, LoadU32(cur, 8U));
            CopyVec3(st, 0x9cU, st, 0x2a4cU);
            CopyVec3(st, 0xb4U, cur, 0x10U);
            CopyVec3(st, 0xa8U, cur, 0x1cU);
            CopyVec3(st, 0xc0U, cur, 0x28U);
            StoreU32(st, 0xe4U, 8U);
            LazyInitPointerRateTimer(st, 0xccU);
            StoreU32(st, 0xd0U, 0U);
            StoreU32(st, 0xd4U, 0U);
            StoreU32(st, 0xccU, static_cast<u32>(-1));
            break;
        }
        case 11: {
            // Start interpolator A variant: extra vec3s into +0x68/+0x74.
            StoreU32(st, 0x94U, LoadU32(cur, 8U));
            CopyVec3(st, 0x50U, st, 0x2a58U);
            CopyVec3(st, 0x68U, cur, 0x10U);
            CopyVec3(st, 0x5cU, cur, 0x1cU);
            CopyVec3(st, 0x74U, cur, 0x28U);
            StoreU32(st, 0x98U, 8U);
            LazyInitPointerRateTimer(st, 0x80U);
            StoreU32(st, 0x84U, 0U);
            StoreU32(st, 0x88U, 0U);
            StoreU32(st, 0x80U, static_cast<u32>(-1));
            break;
        }
        case 12: {
            // Wave mode: byte payload into +0x20, timer +0x24 armed and
            // reset.
            StoreU32(st, 0x20U, LoadU32(cur, 8U) & 0xffU);
            LazyInitPointerRateTimer(st, 0x24U);
            StoreU32(st, 0x28U, 0U);
            StoreU32(st, 0x2cU, 0U);
            StoreU32(st, 0x24U, static_cast<u32>(-1));
            break;
        }
        case 13:
            g_TitleScriptCase13 = LoadU32(cur, 8U);
            break;
        case 14: {
            // VM bind: a non-negative payload re-initializes the indexed
            // +0x180 VM record; a negative payload clears the VM's stop
            // flag (bit 0 of +0x35c).
            const i32 index = static_cast<i32>(LoadU32(cur, 0xcU));
            u8 *vm = st + 0x180U
                + static_cast<u32>(
                      static_cast<i32>(LoadU32(cur, 8U)) * 0x3acU);
            if (index >= 0) {
                BindTitleScriptVmEsiStackAbi(
                    vm, *reinterpret_cast<void **>(st + 0x178U));
            } else {
                StoreU32(vm, 0x35cU, LoadU32(vm, 0x35cU) & 0xfffffffeU);
            }
            break;
        }
        default:
            break;
        }

        if (!advanced) {
            // Default advance: signed 16-bit delta at record+6.
            cur = *reinterpret_cast<u8 **>(st + 0x4cU);
            *reinterpret_cast<u8 **>(st + 0x4cU) =
                cur + static_cast<u32>(LoadI16(cur, 6U));
        }
    }

epilogue:
    // Frame-count timer tick, then the active-interpolator application.
    TickPointerRateTimer(st, 0x38U);

    float scratch[4];
    if (LoadU32(st, 0x94U) != 0U) {
        TickVec3Interpolator(st + 0x50U, scratch);
        CopyVec3(st, 0x2a58U, reinterpret_cast<const u8 *>(scratch), 0);
    }
    if (LoadU32(st, 0xe0U) != 0U) {
        TickVec3Interpolator(st + 0x9cU, scratch);
        CopyVec3(st, 0x2a4cU, reinterpret_cast<const u8 *>(scratch), 0);
    }
    if (LoadU32(st, 0x16cU) != 0U) {
        u32 color_track_out[7];
        TickColorTrack(st + 0xe8U, color_track_out);
        for (u32 i = 0; i != 7U; ++i)
            StoreU32(st, 0x2b48U + 4U * i, color_track_out[i]);
    }
    if (LoadU32(st, 0x20U) != 0U) {
        const u32 mode = LoadU32(st, 0x20U) & 0xffU;
        if (mode == 1U) {
            // Wave: sin(acc * -100 - pi) * 1.5 published to +0x2a88, then
            // the timer forward tick; past 512 frames the timer re-arms.
            StoreFloat(st, 0x2a88U,
                       1.5f * static_cast<float>(sin(
                           static_cast<double>(LoadFloat(st, 0x2cU))
                               * -100.0
                           - 3.14159265358979323846)));
            TickTimerForwardEsiAbi(st + 0x24U);
            if (LoadI32(st, 0x28U) >= 0x200)
                TickPlayerTimerEaxStackAbi(st + 0x24U, 0);
        }
    }
    return 0;
}

// TH10 0x403850. Native EBX = title-screen state block, stack = filename
// pointer. First entry zeroes the global path buffer and appends the
// caller's filename, loads the script file through the shared loader, then
// maps the manager-work resource, rebases the script's pointer tables and
// allocates the VM-record array. Returns 0 on success, -1 on failure.
i32 LoadTitleBackgroundScriptEbxStackAbi(void *state, const char *filename)
{
    u8 *const st = static_cast<u8 *>(state);

    if (LoadU32(st, 0x2a44U) == 0U) {
        // Native string append: the first byte of 0x497c38 is cleared, the
        // buffer's current terminator located, and the filename (with its
        // terminator) copied over it.
        g_TitleScriptPath[0] = '\0';
        char *append = g_TitleScriptPath;
        while (*append != '\0')
            ++append;
        const char *src = filename;
        while (*src != '\0')
            *append++ = *src++;
        *append = '\0';

        void *const data = LoadMainChainFile(g_TitleScriptPath,
            reinterpret_cast<u32 *>(st + 0x2a48U), 0);
        StoreU32(st, 0x2a44U, reinterpret_cast<u32>(data));
        if (data == 0)
            return -1;
    }

    // Copy the loaded image into a private buffer.
    u8 *const data = reinterpret_cast<u8 *>(LoadU32(st, 0x2a44U));
    const u32 size = LoadU32(st, 0x2a48U);
    u8 *const buffer = static_cast<u8 *>(AllocateResourceBuffer(size));
    for (u32 i = 0; i != size; ++i)
        buffer[i] = data[i];
    StoreU32(st, 0x10U, reinterpret_cast<u32>(buffer));

    // Manager-work request: slot = (state+0x2a30 & 1) + 4, resource name
    // inside the buffer at +0x10, owner = DAT_00491c10.
    const i32 slot = static_cast<i32>((LoadU32(st, 0x2a30U) & 1U) + 4U);
    ManagerWorkPartial *const work = RequestManagerWork(
        static_cast<ManagerWorkOwnerPartial *>(g_MainChainRenderOwner),
        slot, reinterpret_cast<const char *>(buffer + 0x10U));
    StoreU32(st, 0x178U, reinterpret_cast<u32>(work));
    if (work == 0) {
        AppendSoundFileLoadError(reinterpret_cast<const char *>(0x46cbc0U));
        return -1;
    }

    // Rebase the script header: the pointer table at +0x14 counts
    // (i16)base[0] dword offsets relative to the buffer base; +0x18/+0x1c
    // are the base-relative dwords at +4/+8.
    StoreU32(st, 0x14U, reinterpret_cast<u32>(buffer + 0x90U));
    StoreU32(st, 0x18U,
             reinterpret_cast<u32>(buffer + LoadU32(buffer, 4U)));
    StoreU32(st, 0x1cU,
             reinterpret_cast<u32>(buffer + LoadU32(buffer, 8U)));
    u32 *const table = reinterpret_cast<u32 *>(
        LoadU32(st, 0x14U));
    const i32 table_count = static_cast<i16>(LoadU32(buffer, 0));
    for (i32 i = 0; i < table_count; ++i)
        table[i] = table[i] + reinterpret_cast<u32>(buffer);

    // VM-record array sized (i16)base[2] * 0x3ac.
    const i32 vm_count = static_cast<i16>(LoadU32(buffer, 2U));
    StoreU32(st, 0x17cU,
             reinterpret_cast<u32>(AllocateResourceBuffer(
                 static_cast<u32>(vm_count * 0x3ac))));
    return 0;
}

// TH10 0x402720.
i32 UpdateTitleBackgroundCalcBodyEaxAbi(void *state)
{
    u8 *const st = static_cast<u8 *>(state);

    const u32 gate = LoadU32(st, 0x2a18U);
    if ((gate & 8U) != 0U)
        return 1;
    if ((gate & 4U) != 0U && LoadI32(st, 0x2a20U) >= 60)
        return 1;

    // Zero the +0x2b3c vec3 (and the two dwords at +0x2b34/+0x2b38 the
    // native clears alongside it), then normalize the +0x2a58 vector into
    // +0x2a70.
    StoreU32(st, 0x2b3cU, 0U);
    StoreU32(st, 0x2b40U, 0U);
    StoreU32(st, 0x2b44U, 0U);
    StoreU32(st, 0x2b34U, 0U);
    StoreU32(st, 0x2b38U, 0U);
    {
        const float in[3] = {LoadFloat(st, 0x2a58U), LoadFloat(st, 0x2a5cU),
                             LoadFloat(st, 0x2a60U)};
        D3DXVec3NormalizeAbi(reinterpret_cast<float *>(st + 0x2a70U), in);
    }
    StoreU32(st, 0x1ee8U, 0x808080U);
    TitleBackgroundPreUpdateAbi(st);

    (void)RunTitleBackgroundScriptStackAbi(st);

    // The eight VM records at +0x180.
    for (u32 i = 0; i != 8U; ++i)
        (void)FinalizeTimelineRenderObjectSetup(st + 0x180U + i * 0x3acU);

    // The three aux VMs run under a forced 1.0 rate when armed.
    if (LoadU32(st, 0x1eecU) != 0U) {
        const float saved_rate = g_SharedRateFloat;
        g_SharedRateFloat = 1.0f;
        (void)FinalizeTimelineRenderObjectSetup(st + 0x1f08U);
        (void)FinalizeTimelineRenderObjectSetup(st + 0x22b4U);
        (void)FinalizeTimelineRenderObjectSetup(st + 0x2660U);
        g_SharedRateFloat = saved_rate;
    }

    // Pause-state snapshot: 0x46 dwords from +0x2a4c into the global
    // snapshot block, then the frame counter advance and latch clear.
    for (u32 i = 0; i != 0x46U; ++i)
        g_PauseSnapshot[i] = LoadU32(st, 0x2a4cU + 4U * i);
    StoreU32(st, 0x1eecU, 0U);
    StoreU32(st, 0x2a34U, LoadU32(st, 0x2a34U) + 1U);
    return 1;
}

} // namespace th10
