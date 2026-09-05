#include "MainChainPlatformCleanup.hpp"

#include "BgmRuntime.hpp"
#include "MainChainRender.hpp"
#include "MainChainResourceThread.hpp"
#include "MainChainRuntime.hpp"

namespace th10 {

namespace {

typedef i32 (TH10_STDCALL *D3DResetFn)(D3D9Device *device,
                                        void *presentation_parameters);
typedef u32 (TH10_STDCALL *ComReleaseFn)(void *object);

extern volatile u32 g_MainChainResourceGate; // TH10 DAT_004977b4
extern TransitionRootPartial g_TransitionRoot; // TH10 DAT_00492590
extern MainChainRenderOwnerFrameState *g_MainChainRenderOwner; // 0x491c10
extern void *g_Direct3D9; // TH10 DAT_00491c2c
extern void *g_D3D9PresentationParameters; // TH10 DAT_00491d0c
extern void *g_MainChainWindow; // TH10 DAT_004924f0

extern void FreeMainChainPlatformObject(void *object); // TH10 0x004524a1
extern void ShowMainChainWindow(void *window, i32 command);
extern void MoveMainChainWindow(void *window, i32 x, i32 y, i32 width,
                                i32 height, i32 repaint);
extern void DestroyMainChainWindow(void *window);
extern i32 ShowMainChainCursor(i32 show);

void *GetComSlot(void *object, u32 index)
{
    return static_cast<void **>(*static_cast<void **>(object))[index];
}

void ReleaseComObject(void *object)
{
    (void)reinterpret_cast<ComReleaseFn>(GetComSlot(object, 2))(object);
}

} // namespace

void ShutdownMainChainPlatformResources(MainChainContext *context)
{
    g_MainChainResourceGate = 2;
    StopBgmWorkerControls(&g_TransitionRoot);
    DestroyTransitionRootSoundResources(&g_TransitionRoot);

    if (g_MainChainRenderOwner != 0) {
        DestroyLargeRenderOwnerInPlace(g_MainChainRenderOwner);
        FreeMainChainPlatformObject(g_MainChainRenderOwner);
    }
    g_MainChainRenderOwner = 0;

    if (context->draw_target != 0) {
        (void)reinterpret_cast<D3DResetFn>(GetComSlot(context->draw_target, 16))(
            context->draw_target, g_D3D9PresentationParameters);
        if (context->draw_target != 0) {
            ReleaseComObject(context->draw_target);
            context->draw_target = 0;
        }
    }

    if (g_Direct3D9 != 0) {
        ReleaseComObject(g_Direct3D9);
        g_Direct3D9 = 0;
    }

    if (g_MainChainWindow != 0) {
        ShowMainChainWindow(g_MainChainWindow, 0);
        MoveMainChainWindow(g_MainChainWindow, 0, 0, 0, 0, 0);
        DestroyMainChainWindow(g_MainChainWindow);
        g_MainChainWindow = 0;
    }
    (void)ShowMainChainCursor(1);
}

} // namespace th10
