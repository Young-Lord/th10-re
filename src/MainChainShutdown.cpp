#include "MainChainShutdown.hpp"

#include "BgmRuntime.hpp"
#include "GeneratedFontTable.hpp"
#include "LargeRenderOwnerLayout.hpp"
#include "MainChainDirectInputAdapter.hpp"
#include "MainChainRuntime.hpp"
#include "MidiTimer.hpp"
#include "PackedArchive.hpp"
#include "RegistrationDrawOwner.hpp"
#include "ThreadControl.hpp"
#include "VersionData.hpp"

namespace th10 {

namespace {

extern ThreadControl g_MainChainBackgroundControl; // TH10 DAT_00474dd4
extern volatile u32 g_MainChainResourceGate; // TH10 DAT_004977b4
extern ThreadControl g_MainChainSecondaryControl; // TH10 DAT_00492254
extern RegistrationDrawOwner *g_RegistrationDrawOwner; // TH10 DAT_00447708
extern void *g_MainChainRenderOwner; // TH10 DAT_00491c10
extern TransitionRootPartial g_TransitionRoot; // TH10 DAT_00492590
extern MidiTimerPartial *g_MainChainTimerObject; // TH10 DAT_00491d44
extern PackedArchive g_PackedArchive; // TH10 DAT_00497990

extern void FreeMainChainObject(void *object); // TH10 0x004524a1
extern void DestroyAllMainChainObjects(); // TH10 0x004203f0

void ReleaseAndClear(void **object)
{
    ReleaseMainChainInputInterfaceAdapter(*object);
    *object = 0;
}

void UnacquireReleaseAndClear(void **object)
{
    UnacquireMainChainInputInterfaceAdapter(*object);
    ReleaseAndClear(object);
}

} // namespace

i32 ShutdownMainChainRuntime(MainChainContext *context)
{
    StopThreadControl(&g_MainChainBackgroundControl);
    g_MainChainResourceGate = 2;
    StopThreadControl(&g_MainChainSecondaryControl);

    ReleaseVersionData();

    DestroyAllMainChainObjects();
    DestroyRegistrationDrawOwner(g_RegistrationDrawOwner);

    LargeRenderOwnerLayout &render_owner =
        *static_cast<LargeRenderOwnerLayout *>(g_MainChainRenderOwner);
    if (render_owner.com_viewport_interface != 0)
        ReleaseAndClear(&render_owner.com_viewport_interface);

    QueueBgmCommand(&g_TransitionRoot, "dummy", 4, 0);
    DestroyGeneratedTableAndFonts();

    if (context->input_keyboard_0010 != 0)
        UnacquireReleaseAndClear(&context->input_keyboard_0010);
    if (context->input_controller_0014 != 0)
        UnacquireReleaseAndClear(&context->input_controller_0014);
    if (context->input_root_000c != 0)
        ReleaseAndClear(&context->input_root_000c);

    if (g_MainChainTimerObject != 0) {
        StopMidiTimer(g_MainChainTimerObject);
        DestroyMidiTimerInPlace(g_MainChainTimerObject);
        FreeMainChainObject(g_MainChainTimerObject);
        g_MainChainTimerObject = 0;
    }

    g_PackedArchive.Clear();
    return 0;
}

} // namespace th10
