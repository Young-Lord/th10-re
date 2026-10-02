#include "MainChainContext.hpp"
#include "GlobalLifecycleManager.hpp"
#include "TitleScreenObject.hpp"

extern "C" void TH10_STDCALL EnterCriticalSection(void *critical_section);
extern "C" void TH10_STDCALL LeaveCriticalSection(void *critical_section);

namespace th10 {

namespace {

// These declarations describe the lifecycle calls made by 0x004218d0.  Their
// original register conventions are not recovered here; this C++ unit uses
// ordinary semantic interfaces until their owning modules are reconstructed.
extern void DestroyAllMainChainObjects(); // TH10 0x004203f0

extern void *g_TitleScreen; // TH10 DAT_00477810
extern void *g_GameManager; // TH10 DAT_0047784c
extern i32 g_UnknownTransitionStatus; // TH10 DAT_00491c00
extern i32 g_TitleTransitionIndex; // TH10 DAT_00474c80
extern i32 g_LastTitleTransitionIndex; // TH10 DAT_00474c7c
extern u8 g_TitleTransitionRecords[]; // TH10 DAT_00474788, 48-byte entries
extern void *g_TitleTransitionRecord; // TH10 DAT_00477848

extern void DestroyTitleScreen(void *title_screen); // TH10 0x00418150
extern void *CreateTitleScreen(i32 mode); // TH10 0x004180e0
extern void DestroyGameManager(void *game_manager); // TH10 0x0042cdb0
extern void *CreateGameManager(); // TH10 0x0042cd50
extern void DestroyTransitionObject(); // TH10 0x0040b9d0
extern void *CreateTransitionObject(); // TH10 0x0040b940
// The only field read from the title object by 0x004218d0 is +0x5c
// (TitleScreen::mode, the CreateTitleScreen transition argument).

void CreateGameManagerForStateFour(MainChainState previous_state)
{
    // 0x00421967 selects one of four paths with the table at 0x00421b4c.
    // Only predecessor states 1, 2, 7, and 14 reach a game-manager create.
    switch (previous_state) {
    case MainChainState_Unknown1:
    case MainChainState_Unknown2:
        CreateGameManager();
        break;

    case MainChainState_TitleScreen:
        DestroyTitleScreen(g_TitleScreen);
        CreateGameManager();
        break;

    case MainChainState_Unknown14:
        DestroyTransitionObject();
        CreateGameManager();
        break;

    case MainChainState_Init:
    case MainChainState_FailureExit:
    case MainChainState_GameManager:
    case MainChainState_Unknown5:
    case MainChainState_Unknown6:
    case MainChainState_Unknown8:
    case MainChainState_Unknown9:
    case MainChainState_TitleTransition10:
    case MainChainState_TitleTransition11:
    case MainChainState_TitleTransition12:
    case MainChainState_TitleTransition13:
    case MainChainState_StartGameTransition:
        // These selectors map directly to the normal commit at 0x0042198b.
        break;

    default:
        break;
    }
}

void AdvanceStartGameTransition(MainChainContext *context,
                                MainChainState previous_state)
{
    // TH10 0x004219bb handles only these three predecessor states.  Other
    // values deliberately retain requested_state == StartGameTransition.
    switch (previous_state) {
    case MainChainState_Unknown2:
        break;

    case MainChainState_TitleScreen:
        DestroyTitleScreen(g_TitleScreen);
        break;

    case MainChainState_Unknown14:
        DestroyTransitionObject();
        break;

    default:
        return;
    }

    context->requested_state = MainChainState_GameManager;
    g_UnknownTransitionStatus = 3;
    CreateGameManager();
}

} // namespace

// TH10 0x004218e8 through 0x004218f5, with lock index 5.
void MainChainContext::EnterStateLock(i32 lock_index)
{
    EnterCriticalSection(&state_locks[lock_index]);
    state_update_depths[lock_index]++;
}

// TH10 0x00421c50. The original receives the context in EDI and index in ESI.
void MainChainContext::LeaveStateLock(i32 lock_index)
{
    LeaveCriticalSection(&state_locks[lock_index]);
    state_update_depths[lock_index]--;
}

// TH10 0x004218d0.  The original accepts this in EAX; the declaration in the
// header intentionally exposes a normal C++ member interface for now.
MainChainAdvanceResult MainChainContext::AdvanceState()
{
    if (previous_state == requested_state)
        return MainChainAdvance_Continue;

    EnterStateLock(5);
    transition_source_state = previous_state;
    transition_color = 0xff000000;

    switch (requested_state) {
    case MainChainState_Init:
        requested_state = MainChainState_Unknown1;
        field_0768 = CreateGlobalLifecycleManager();
        if (field_0768 == 0) {
            requested_state = MainChainState_FailureExit;
            DestroyAllMainChainObjects();
            LeaveStateLock(5);
            return MainChainAdvance_Failed;
        }
        break;

    case MainChainState_FailureExit:
        DestroyAllMainChainObjects();
        LeaveStateLock(5);
        return MainChainAdvance_Failed;

    case MainChainState_GameManager:
        CreateGameManagerForStateFour(previous_state);
        break;

    case MainChainState_TitleScreen:
        if (previous_state == MainChainState_GameManager)
            DestroyGameManager(g_GameManager);
        transition_flag = 1;
        CreateTitleScreen(0);
        break;

    case MainChainState_TitleTransition10:
        DestroyTitleScreen(g_TitleScreen);
        transition_flag = 1;
        requested_state = MainChainState_TitleScreen;
        g_LastTitleTransitionIndex = g_TitleTransitionIndex;
        g_TitleTransitionRecord =
            g_TitleTransitionRecords + g_TitleTransitionIndex * 48;
        CreateTitleScreen(0);
        break;

    case MainChainState_TitleTransition11: {
        // Native 0x421a51: reads the +0x5c mode dword and hands it to
        // CreateTitleScreen as the transition argument.
        const TitleScreen &ts =
            *reinterpret_cast<const TitleScreen *>(g_TitleScreen);
        const u32 transition_argument = ts.mode;

        transition_flag = 0;
        if (previous_state == MainChainState_TitleScreen)
            DestroyTitleScreen(g_TitleScreen);
        requested_state = MainChainState_TitleScreen;
        CreateTitleScreen(static_cast<i32>(transition_argument));
        break;
    }

    case MainChainState_TitleTransition12:
        if (previous_state == MainChainState_GameManager)
            DestroyGameManager(g_GameManager);
        requested_state = MainChainState_TitleScreen;
        transition_flag = 1;
        CreateTitleScreen(1);
        break;

    case MainChainState_TitleTransition13:
        DestroyTitleScreen(g_TitleScreen);
        transition_flag = 1;
        requested_state = MainChainState_TitleScreen;
        CreateTitleScreen(0);
        break;

    case MainChainState_Unknown14:
        if (previous_state == MainChainState_TitleScreen)
            DestroyTitleScreen(g_TitleScreen);
        CreateTransitionObject();
        break;

    case MainChainState_StartGameTransition:
        AdvanceStartGameTransition(this, previous_state);
        break;

    // States 1, 2, 5, 6, 8, and 9 jump directly to the normal commit in the
    // original.  Values outside the 0..15 jump-table range do the same.
    default:
        break;
    }

    previous_state = requested_state;
    LeaveStateLock(5);
    return MainChainAdvance_Continue;
}

} // namespace th10
