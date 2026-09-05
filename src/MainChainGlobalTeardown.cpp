#include "GlobalLifecycleManager.hpp"

namespace th10 {

namespace {

extern void *g_TitleScreen; // TH10 DAT_00477810
extern void *g_GameManager; // TH10 DAT_0047784c
extern GlobalLifecycleManager *g_GlobalLifecycleManager; // DAT_00477820
extern void *g_TransitionObject; // TH10 DAT_00477700
extern void *g_UnknownMainChainObject; // TH10 DAT_00477838

i32 TH10_STDCALL TeardownTitleScreenStackAbi(void *title_screen); // TH10 0x00417c80 semantic body (src/TitleGameManagerLifecycle.cpp)
extern void DestroyGameManagerInPlace(void *object); // TH10 0x0042cb60
extern void DestroyTransitionObjectInPlace(void *object); // TH10 0x0040b7b0
extern void DestroyUnknownMainChainObjectInPlace(void *object); // 0x4294a0
extern void FreeMainChainObject(void *object); // TH10 0x004524a1

void DestroyAndFree(void *object, void (*destroy_in_place)(void *))
{
    if (object == 0)
        return;
    destroy_in_place(object);
    FreeMainChainObject(object);
}

// TH10 0x00417c80 is stdcall ret 4 and returns the clear color; the shared
// teardown helper discards it, so this thin adapter matches the helper's
// plain calling convention.
void DestroyTitleScreenInPlaceAdapter(void *object)
{
    TeardownTitleScreenStackAbi(object);
}

} // namespace

// TH10 0x004203f0. This function does not itself clear any global pointer;
// each in-place destructor owns any such side effect, matching the original.
void DestroyAllMainChainObjects()
{
    DestroyAndFree(g_TitleScreen, DestroyTitleScreenInPlaceAdapter);
    DestroyAndFree(g_GameManager, DestroyGameManagerInPlace);

    if (g_GlobalLifecycleManager != 0)
        DestroyGlobalLifecycleManager(g_GlobalLifecycleManager);

    DestroyAndFree(g_TransitionObject, DestroyTransitionObjectInPlace);
    DestroyAndFree(g_UnknownMainChainObject,
                   DestroyUnknownMainChainObjectInPlace);
}

} // namespace th10
