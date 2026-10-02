// TH10 0x00422ab0 — pause-entry setup (mode 0 of the 0x004223f0 scheduler
// record dispatcher). Every fixed address and offset below is from the raw
// disassembly; see docs/evidence/pause-enter-setup.md.
#include <string.h>

#include "AsciiHudOwner.hpp"
#include "BgmRuntime.hpp"
#include "EntityHelpers.hpp"
#include "GameManagerState.hpp"
#include "GameStateManagerObject.hpp"
#include "StageEffectHelpers.hpp"
#include "Th10Types.hpp"
#include "TitleScreenObject.hpp"
#include "VmRecord.hpp"
#include "PauseEnterSetup.hpp"

namespace th10 {

namespace {

// ------------------------------------------------------------- globals

extern void *g_TitleScreen;          // TH10 DAT_00477810 (pause/game state)
extern void *g_AsciiManagerHost;     // TH10 DAT_004776e0 (+0x8998 ANM work)
extern void *g_AsciiHudOverlayState; // TH10 DAT_0047770c (+0x9ec8 ANM work)
extern void *g_MainChainRenderOwner; // TH10 DAT_00491c10
extern float g_FrameTimeScale;       // TH10 DAT_00476f78

const u32 kMenuSoundContext = 0x492590U; // BGM/sound command root

} // namespace

// TH10 0x00422ab0 (native `ret 4`, record as the stack argument).
void RunPauseEnterSetupStackAbi(void *record_arg)
{
    GameStateManager &mgr = *reinterpret_cast<GameStateManager *>(record_arg);
    TitleScreen &ts = *reinterpret_cast<TitleScreen *>(g_TitleScreen);

    mgr.mode_0004 = 1; // mode = 1

    // Timer family init at +0x10..+0x20 (the reduced sequence shared with
    // 0x00423510: the NaN-sentinel branch stores are immediately overwritten
    // by the unconditional tail, order preserved).
    if ((mgr.frame_timer.flags & 1U) == 0U) {
        mgr.frame_timer.count = 0;
        *reinterpret_cast<u32 *>(&mgr.frame_timer.prev) = 0xFFF0BDC1U; // NaN sentinel
        mgr.frame_timer.accum = 0;
        mgr.frame_timer.rate = &g_FrameTimeScale;
        mgr.frame_timer.flags |= 1U;
    }
    mgr.frame_timer.count = 0;
    mgr.frame_timer.accum = 0;
    *reinterpret_cast<u32 *>(&mgr.frame_timer.prev) = 0xFFFFFFFFU; // stopped (-1)

    // Latch the pause bit into the game state object.
    ts.flags |= 0x10U;

    // Menu effect VM (script 0) from the ASCII manager's ANM manager-work.
    // Native binds the script with ECX = [DAT_004776e0]+0x8998; the semantic
    // 0x00449870 body feeds its bind context internally.
    u8 *vm = static_cast<u8 *>(AllocatePoolVmEsiAbi(g_MainChainRenderOwner));
    VmRecord &menu_vm = *reinterpret_cast<VmRecord *>(vm);
    menu_vm.flags |= 0x40000000U;
    menu_vm.render_kind = 0xfU;
    AssignPoolVmScriptEcxEaxAbi(vm, 0);
    u32 menu_vm_id = 0;
    AttachEffectVmToListB(&menu_vm_id, vm, g_MainChainRenderOwner);
    mgr.handle_b_01d8 = menu_vm_id; // handle B

    // Full-screen pause overlay target (native ESI = 0x491c10 across the
    // call, ret 0x14; the result is ignored).
    (void)CreateGameOverOverlay(g_MainChainRenderOwner,
                                static_cast<i32>(menu_vm_id), 0x20, 0x10,
                                0x180, 0x1c0);

    // HUD watch VM (script 0x79) from the HUD owner's manager-work
    // (native ECX = [DAT_0047770c]+0x9ec8, mirrored into record+0x2c4).
    u32 *const handle_a = &mgr.handle_a_01d4;
    mgr.front_anm_work_02c4 =
        reinterpret_cast<AsciiHudOwner *>(g_AsciiHudOverlayState)
            ->front_anm_work;
    vm = static_cast<u8 *>(AllocatePoolVmEsiAbi(g_MainChainRenderOwner));
    VmRecord &watch_vm = *reinterpret_cast<VmRecord *>(vm);
    watch_vm.flags |= 0x40000000U;
    watch_vm.render_kind = 0xfU;
    AssignPoolVmScriptEcxEaxAbi(vm, 0x79);
    AttachEffectVmToListB(handle_a, vm, g_MainChainRenderOwner);
    FireEntityHandleEaxAbi(handle_a); // state word 3 (fire/consume)

    // Pause sound (native EDI = 0x20) and the "Pause" BGM command
    // (string at TH10 0x46e0c0, opcode 6).
    (void)ReserveContextChannel(reinterpret_cast<void *>(kMenuSoundContext),
                                0x20U, 0U);
    QueueBgmCommand(reinterpret_cast<TransitionRootPartial *>(
                        kMenuSoundContext),
                    "Pause", 6, 0);

    // When [DAT_00477810]+0x5c is set, release the kind-0x75 child of
    // handle A. The native releases unconditionally through the (possibly
    // zero) resolved handle.
    if (ts.mode != 0U) {
        u32 child_handle = 0;
        u32 *const out =
            ResolveChildEntityByKind(handle_a, 0x75, &child_handle);
        ReleaseEntityById(g_MainChainRenderOwner, *out);
        *out = 0;
    }

    // Park the frame-time scale: record+0x2c0 = DAT_00476f78, then reset the
    // global to 1.0 (raw dword copies in the native).
    mgr.saved_time_scale_02c0 = g_FrameTimeScale;
    g_FrameTimeScale = 1.0f;
}

} // namespace th10
