// Semantic reconstruction of the two title-screen state draw scheduler
// records (0x00402850 and 0x00402ca0) plus their shared 0x00405300 draw-work
// selector. The two draw bodies are registered by the title-screen state
// constructor 0x00402230 (calc record 0x403050 -> 0x00402720, draw records
// 0x403060 -> 0x00402850 and 0x403070 -> 0x00402ca0, each receiving the
// 0x2b64-byte state through a `push ecx` adapter). Every fixed global address
// and state offset below is taken from the native disassembly; see
// docs/evidence/title-screen-draw-passes.md.
#include <string.h>

#include "AsciiOverlayFactory.hpp"
#include "AsciiOwnerTraversal.hpp"
#include "AsciiRenderModeDispatcher.hpp"
#include "LargeRenderOwnerFrameLoop.hpp"
#include "LargeRenderOwnerLayout.hpp"
#include "MainChainContext.hpp"
#include "MainChainRender.hpp"
#include "PlayerTimerHelpers.hpp"
#include "Th10Platform.hpp"
#include "Th10Types.hpp"
#include "TitleScreenDrawPasses.hpp"
#include "TitleScreenState.hpp"

namespace th10 {

namespace {

extern MainChainRenderOwnerFrameState *g_MainChainRenderOwner; // TH10 DAT_00491c10
extern MainChainCameraWork g_AsciiCameraWork;                  // TH10 DAT_00491d7c
extern void *g_MainChainActiveCameraWork;                      // TH10 DAT_00491fac
extern D3D9Device *g_MainChainD3DDevice;                       // TH10 DAT_00491c30
extern u32 g_AsciiActiveViewIsDefault;                         // TH10 DAT_00491fb0 (also referenced as g_MainChainActiveView)
extern u32 g_AsciiFogEnableCache;                              // TH10 DAT_00492378
extern float g_AsciiOverlayRenderOffsetX;                      // TH10 DAT_00491e64
extern float g_AsciiOverlayRenderOffsetY;                      // TH10 DAT_00491e68
extern MainChainContext g_MainChainContext;                    // TH10 DAT_00491c28

typedef i32 (TH10_STDCALL *D3DSetViewportFn)(D3D9Device *, const void *);
typedef i32 (TH10_STDCALL *D3DSetRenderStateFn)(D3D9Device *, u32, u32);
typedef i32 (TH10_STDCALL *D3DClearFn)(D3D9Device *, u32, const void *, u32,
                                        u32, float, u32);

u32 LoadU32At(const void *base, u32 offset)
{
    return *reinterpret_cast<const u32 *>(
        static_cast<const u8 *>(base) + offset);
}

void FlushOwnerVertices()
{
    FlushRenderOwnerPendingVertices(
        reinterpret_cast<RenderOwnerPartial *>(g_MainChainRenderOwner));
}

// TH10 device wrapper slots used by the title passes: 0xbc = SetViewport,
// 0xe4 = SetRenderState, 0xac = Clear (same slots as MainChainD3DDevice.cpp).
void SetDeviceViewportOn(D3D9Device *device, const void *viewport)
{
    (void)reinterpret_cast<D3DSetViewportFn>(device->vtable[0xbc / 4])(
        device, viewport);
}

void SetDeviceViewport(const void *viewport)
{
    SetDeviceViewportOn(g_MainChainD3DDevice, viewport);
}

void SetDeviceRenderState(u32 state, u32 value)
{
    (void)reinterpret_cast<D3DSetRenderStateFn>(
        g_MainChainD3DDevice->vtable[0xe4 / 4])(g_MainChainD3DDevice, state,
                                                 value);
}

void ClearDeviceTarget(u32 rect_count, const void *rects, u32 flags,
                       u32 color)
{
    (void)reinterpret_cast<D3DClearFn>(g_MainChainD3DDevice->vtable[0xac / 4])(
        g_MainChainD3DDevice, rect_count, rects, flags, color, 1.0f, 0);
}

// Publishes g_AsciiCameraWork (DAT_00491d7c) as the active camera work,
// refreshes its matrices through 0x004215a0 and installs its viewport.
void PublishActiveCameraWork()
{
    g_MainChainActiveCameraWork = &g_AsciiCameraWork;
    UpdateMainChainCameraWorkEdiAbi(&g_AsciiCameraWork);
    SetDeviceViewport(
        static_cast<const u8 *>(static_cast<const void *>(&g_AsciiCameraWork))
        + 0xcc);
}

// Copies the state's 0x46-dword camera-work snapshot at +0x2a4c over the
// DAT_00491d7c block and republishes it.
void RestoreCameraWorkSnapshot(const TitleScreenState &state)
{
    memcpy(&g_AsciiCameraWork, &state.camera_snapshot,
           sizeof(MainChainCameraWork));
    PublishActiveCameraWork();
}

// Native inlined fog gate: when the DAT_00492378 cache differs from the
// wanted value, flush first, cache the value, then issue render state 0x1c.
void SyncFogEnable(u32 wanted)
{
    if (g_AsciiFogEnableCache == wanted)
        return;
    FlushOwnerVertices();
    g_AsciiFogEnableCache = wanted;
    SetDeviceRenderState(0x1c, wanted);
}

// Publishes the owner color-modulation words (+0x73245c flag and +0x732458
// packed color, defaults restored to 0 / 0x80808080 between passes).
void PublishOwnerModulation(u32 enabled, u32 color)
{
    LargeRenderOwnerLayout &owner =
        *reinterpret_cast<LargeRenderOwnerLayout *>(g_MainChainRenderOwner);
    owner.custom_color_gate = enabled;
    owner.clear_color = color;
}

struct ScreenRect {
    i32 left;
    i32 top;
    i32 right;
    i32 bottom;
};

} // namespace

// TH10 0x00405300.
void SelectMainChainDrawWork(MainChainContext *context, u32 index)
{
    MainChainCameraWork *work = &context->camera_work_bank[index];
    context->draw_work_pointer = work;
    (void)UpdateMainChainD3DFrameStateEdiAbi(work);
    SetDeviceViewportOn(context->draw_target, &work->viewport);
    context->draw_initialized = static_cast<i32>(index);
}

// TH10 0x00402850. Base/menu draw pass.
i32 RunTitleScreenDrawPass0StackAbi(void *state_argument)
{
    TitleScreenState &state =
        *reinterpret_cast<TitleScreenState *>(state_argument);
    u8 *const st = static_cast<u8 *>(state_argument);
    u32 flags = state.master_flags;
    if ((flags & 8U) != 0U)
        return 1;

    // Front block: skipped entirely once the fade timer has passed 60.
    if ((flags & 4U) == 0U || state.score_anim_timer.count < 0x3c) {
        FlushOwnerVertices();
        state.camera_snapshot.owner_value_00e8 =
            LoadU32At(&g_AsciiOverlayRenderOffsetX, 0); // +0x2b34
        state.camera_snapshot.owner_value_00ec =
            LoadU32At(&g_AsciiOverlayRenderOffsetY, 0); // +0x2b38
        RestoreCameraWorkSnapshot(state);
        g_AsciiActiveViewIsDefault = 0;
        FlushOwnerVertices();
        SetDeviceRenderState(0xe, 1); // z-write enable
        FlushOwnerVertices();
        SetDeviceRenderState(0x17, 4); // z compare func
        FlushOwnerVertices();
        // (+0x2b60/+0x2b48/+0x2b4c sit in the camera snapshot's unnamed
        // tail and stay raw byte accesses.)
        SetDeviceRenderState(0x22, LoadU32At(st, 0x2b60));
        FlushOwnerVertices();
        SetDeviceRenderState(0x24, LoadU32At(st, 0x2b48));
        FlushOwnerVertices();
        SetDeviceRenderState(0x25, LoadU32At(st, 0x2b4c));

        // Whole-target z-buffer clear (no preceding flush in the native).
        ClearDeviceTarget(0, 0, 2, 0);

        flags = state.master_flags;
        const ScreenRect menu_region = {0x20, 0x10, 0x1a0, 0x1d0};
        if ((flags & 4U) != 0U &&
            static_cast<i32>(state.intro_counter) < 0x22) {
            // Menu area clears to black while the intro counter is below 34.
            ClearDeviceTarget(1, &menu_region, 1, 0);
        } else {
            ClearDeviceTarget(1, &menu_region, 1,
                              LoadU32At(st, 0x2b60) & 0xffffffU);
        }
    }

    // Fade-in overlay arm and timer state machine (always runs).
    flags = state.master_flags;
    if ((flags & 4U) != 0U) {
        if (state.score_anim_timer.count < 0x1e) {
            // A fresh kind-3 overlay context is allocated every frame while
            // the fade timer counts up; the native discards the result.
            (void)CreateAsciiOverlayContext(3, 0x1e, 0, 0, 0, 0xf);
            state.master_flags |= 1U;
            TickPlayerTimerEaxStackAbi(&state.score_anim_timer, 1);
        } else {
            state.master_flags = flags & ~1U;
            reinterpret_cast<u8 *>(&state.modulation_color)[3] = 0; // +0x1eeb
        }
    }

    if (reinterpret_cast<const u8 *>(&state.modulation_color)[3] != 0)
        PublishOwnerModulation(1, state.modulation_color);

    // Scene counters are cleared unconditionally.
    state.scene_counter = 0;
    state.scene_counter_clear = 0;
    state.op_counter = 0;

    flags = state.master_flags;
    if ((flags & 1U) != 0U) {
        // The native gates the whole background-VM block on the second
        // record's +0x394 resource pointer (state+0x514), not on a dedicated
        // flag; preserved as-is.
        if (state.background_vms[1].anim_entry != 0) {
            SelectMainChainDrawWork(&g_MainChainContext, 0);
            (void)DisableMainChainFogIfNeeded(&g_MainChainContext);
            FlushOwnerVertices();
            FlushOwnerVertices();
            SetDeviceRenderState(0xe, 0);
            for (u32 index = 0; index != 8; ++index) {
                if (state.background_vms[index].anim_entry != 0)
                    (void)DispatchAsciiAnimationVmRenderMode(
                        &state.background_vms[index], g_MainChainRenderOwner);
            }
            FlushOwnerVertices();
            SetDeviceRenderState(0xe, 1);
            PublishActiveCameraWork();
            g_AsciiActiveViewIsDefault = 0;
        }
    }

    SyncFogEnable(1);

    for (i32 channel = 0; channel != 8; ++channel)
        (void)RenderAsciiSceneChannel(st, channel);
    FlushOwnerVertices();

    // Idle-frame latch: only increments while already nonzero.
    if (state.idle_frame_latch != 0U)
        state.idle_frame_latch = state.idle_frame_latch + 1U;

    PublishOwnerModulation(0, 0x80808080U);
    if (state.aux_vm_arm_latch != 0U)
        PublishOwnerModulation(1, 0xff404040U);

    FlushOwnerVertices();
    SetDeviceRenderState(0xe, 0);
    FlushOwnerVertices();
    SetDeviceRenderState(0x17, 8);
    return 1;
}

// TH10 0x00402ca0. Kind-chain / overlay draw pass with the fade-out epilogue.
i32 RunTitleScreenDrawPass1StackAbi(void *state_argument)
{
    TitleScreenState &state =
        *reinterpret_cast<TitleScreenState *>(state_argument);
    u8 *const st = static_cast<u8 *>(state_argument);
    u32 flags = state.master_flags;
    if ((flags & 8U) != 0U)
        return 1;

    // Front block: skipped entirely once the fade timer has passed 60.
    if ((flags & 4U) == 0U || state.score_anim_timer.count < 0x3c) {
        FlushOwnerVertices();
        state.camera_snapshot.owner_value_00e8 =
            LoadU32At(&g_AsciiOverlayRenderOffsetX, 0); // +0x2b34
        state.camera_snapshot.owner_value_00ec =
            LoadU32At(&g_AsciiOverlayRenderOffsetY, 0); // +0x2b38
        RestoreCameraWorkSnapshot(state);

        SyncFogEnable(1);
        g_AsciiActiveViewIsDefault = 0;
        FlushOwnerVertices();
        SetDeviceRenderState(0xe, 0);
        FlushOwnerVertices();
        SetDeviceRenderState(0x17, 8);

        (void)DrawLargeRenderOwnerKindChain(g_MainChainRenderOwner, 0x11);
        FlushOwnerVertices();
        SetDeviceRenderState(0x17, 4);
        SyncFogEnable(0);
        (void)DrawLargeRenderOwnerKindChain(g_MainChainRenderOwner, 0x12);
        FlushOwnerVertices();
        SetDeviceRenderState(0x17, 4);
        FlushOwnerVertices();
        // (+0x2b60/+0x2b48/+0x2b4c sit in the camera snapshot's unnamed
        // tail and stay raw byte accesses.)
        SetDeviceRenderState(0x22, LoadU32At(st, 0x2b60));
        FlushOwnerVertices();
        SetDeviceRenderState(0x24, LoadU32At(st, 0x2b48));
        FlushOwnerVertices();
        SetDeviceRenderState(0x25, LoadU32At(st, 0x2b4c));
    }

    // Fade completion latches the modulation byte while the timer is done.
    flags = state.master_flags;
    if ((flags & 4U) != 0U && state.score_anim_timer.count >= 0x1e)
        reinterpret_cast<u8 *>(&state.modulation_color)[3] = 0; // +0x1eeb

    if ((flags & 1U) != 0U) {
        FlushOwnerVertices();
        SetDeviceRenderState(0xe, 0);
        SyncFogEnable(1);
        for (i32 channel = 8; channel != 12; ++channel)
            (void)RenderAsciiSceneChannel(st, channel);
        FlushOwnerVertices();
    }

    PublishOwnerModulation(0, 0x80808080U);
    if (state.aux_vm_arm_latch != 0U)
        PublishOwnerModulation(1, 0xff404040U);

    FlushOwnerVertices();
    SetDeviceRenderState(0xe, 0);
    FlushOwnerVertices();
    SetDeviceRenderState(0x17, 8);

    // Drain the fade timer by one frame while positive; when it reaches zero
    // the fade ends (bits 1/2 cleared, white modulation latched, and bit 3
    // set when bit 1 requested the draw shutdown).
    if (state.score_anim_timer.count > 0) {
        ShiftTimerByEsiStackAbi(&state.score_anim_timer, -1.0f);
        if (state.score_anim_timer.count <= 0) {
            u32 end_flags = state.master_flags;
            reinterpret_cast<u8 *>(&state.modulation_color)[3] = 0xff;
            if ((end_flags & 2U) != 0U) {
                end_flags |= 8U;
                state.master_flags = end_flags;
            }
            end_flags = state.master_flags;
            state.modulation_color = 0xffffffU;
            state.master_flags = end_flags & ~6U;
        }
    }

    FlushOwnerVertices();
    SetDeviceRenderState(0xe, 0);
    FlushOwnerVertices();
    SetDeviceRenderState(0x17, 8);
    SyncFogEnable(0);
    return 1;
}

} // namespace th10
