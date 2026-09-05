#include <string.h>

#include "CallbackScheduler.hpp"
#include "EntityHelpers.hpp"
#include "GameManagerState.hpp"
#include "GlobalLifecycleManager.hpp"
#include "ManagerWork.hpp"
#include "Th10Platform.hpp"
#include "Th10Types.hpp"
#include "ThreadControl.hpp"

namespace th10 {

namespace {

typedef u32 (TH10_STDCALL *CrtThreadStartFn)(void *);

// Native operator new / operator delete pair (TH10 0x00452493 / 0x004524a1).
// Title and game-manager objects are allocated with this pair; their outer
// release is the same delete the global-teardown order uses.
extern void *AllocateMainChainObject(u32 bytes); // TH10 0x00452493
extern void FreeMainChainObject(void *object); // TH10 0x004524a1
extern void *AllocateResourceBuffer(u32 bytes); // TH10 0x00452706
extern void ReleaseResourceBuffer(void *pointer); // TH10 0x00452422

extern "C" u32 TH10_CDECL _beginthreadex(void *security_attributes,
                                         u32 stack_size,
                                         CrtThreadStartFn start_routine,
                                         void *argument,
                                         u32 creation_flags,
                                         u32 *thread_id);
extern "C" void TH10_STDCALL EnterCriticalSectionInternal(
    void *critical_section);
extern "C" void TH10_STDCALL LeaveCriticalSectionInternal(
    void *critical_section);

extern void *g_TitleScreen; // TH10 DAT_00477810
extern void *g_GameManager; // TH10 DAT_0047784c
extern void *g_TitleOwnerHookTarget; // TH10 DAT_00491c30

// (**(code **)(*DAT_00491c30 + 0x14))(DAT_00491c30) from 0x004180e0.
void InvokeTitleOwnerHook()
{
    typedef void (*OwnerHookFn)(void *);
    void *const owner = g_TitleOwnerHookTarget;
    if (owner != 0) {
        OwnerHookFn *const vtable =
            *reinterpret_cast<OwnerHookFn *const *>(owner);
        if (vtable != 0)
            vtable[0x14 / 4](owner);
    }
}

// TH10 0x0042c920. Game-manager in-place construction. The native body first
// plants a destructor vtable (0x0046ecf0) plus default seeds (including the
// thread-control destructor marker at +0x5ab0) and then unconditionally
// zeroes the whole 0x5acc-byte allocation (verified at byte level: objdump
// shows REP STOSD ECX=0x16b3 with EDI=this at 0x0042c9d6), so none of those
// early stores survive. The observable net result is a blank manager with
// dword +4 bit 1 set, published to DAT_0047784c, and returned.
void *ConstructGameManagerInPlace(void *game_manager)
{
    u8 *const manager = static_cast<u8 *>(game_manager);
    memset(manager, 0, 0x5acc);
    reinterpret_cast<u32 *>(manager)[4 / 4] |= 2U;
    g_GameManager = manager; // DAT_0047784c
    return manager;
}

// Game-manager teardown and worker shared state (TH10 0x0042cb60 / 0x0042c9f0).
extern void *g_MainChainRenderOwner; // TH10 DAT_00491c10
extern CallbackScheduler *g_CallbackScheduler; // TH10 DAT_00491be4
extern GlobalLifecycleManager *g_GlobalLifecycleManager; // TH10 DAT_00477820
extern u32 g_MainChainRuntimeFlags; // TH10 DAT_00491ff4
extern i32 g_MainChainSharedStatus; // TH10 DAT_00491fb8
extern u32 g_MainChainManagerGate; // TH10 DAT_004918a4; 1 = releasing, 0 = game manager booted
extern void *g_MainChainErrorReceiver; // TH10 DAT_00474f70
// The native worker yields while its loading wait is active by calling
// through the IAT slot 0x004660ac, which survey confirms is KERNEL32!Sleep
// (so the 0x10 argument means a 16 ms sleep).
extern u32 (TH10_STDCALL *g_LoadWaitFrameYield)(u32); // TH10 0x004660ac = Sleep
extern void ReleaseLargeRenderOwnerSlot(); // TH10 0x00447810 render-owner register state
extern void DestroyOpaqueMainChainManagerInPlace(void *object); // TH10 0x004294a0
extern void ReportMainChainErrorText(void *receiver,
                                     const char *text); // TH10 0x0044b810, ECX receiver
// Title-screen startup body run by the title callback worker (0x00417870);
// it waits for the main-chain owner, then initializes the whole game-mode
// subsystem set and returns 0 on success / -1 on failure.
extern i32 RunTitleScreenStartupBody(void *title_screen); // TH10 0x00417870
// Title-screen per-frame calc controller (0x00418190), forwarded to by the
// title calculation-record callback at 0x004187c0.
extern i32 RunTitleScreenCalcBody(void *title_screen); // TH10 0x00418190
extern void TeardownTitleScreenInPlace(void *title_screen); // TH10 0x00417c80
// Shared frame/state block rooted at DAT_00474c40 and the float whose address
// the block stores (flt_00476f78); both are fixed globals used by the title
// and game-manager controllers.
extern u32 g_MainChainFrameStateBlock[]; // TH10 DAT_00474c40 region
extern float g_MainChainTimeScaleTarget; // TH10 flt_00476f78
// Game-manager channel controllers (TH10 0x0042cdf0 / 0x0042d260). The two
// 0x0042d2e0/0x0042d2f0 channel adapters just move the ECX manager pointer to
// EAX and tail-jump here, so the controllers are modeled directly.
extern i32 RunGameManagerCalculationBody(void *game_manager); // TH10 0x0042cdf0
extern i32 RunGameManagerDrawBody(void *game_manager); // TH10 0x0042d260

// Game-manager calc controller (0x0042cdf0) shared globals.
extern u32 g_MainChainPresentDiagnosticOptions; // TH10 DAT_00474e30
extern u32 g_MainChainManagerFlow; // TH10 DAT_00491c00
extern u32 g_DemoWaitCounter; // TH10 DAT_00474ca4
extern u32 g_DemoNameIndex; // TH10 DAT_00474ca8
extern u32 g_GlobalModeFlags; // TH10 DAT_00474ca0
extern u32 g_StageTextSprites[2]; // TH10 DAT_00474c68/6c
extern u32 g_StageScoreSelector[2]; // TH10 DAT_00474c74/78
extern u32 g_StageSelectorIndex; // TH10 DAT_00474c7c
extern u32 g_StageSelectorCurrent; // TH10 DAT_00474c80
extern u32 g_ModeRecordTablePointer; // TH10 DAT_00477848
extern const char *const g_DemoNameTable[4]; // TH10 off_00476ff8
extern void *g_AsciiManagerHost; // TH10 DAT_004776e0

// Leaf boundaries invoked by the calc controller (register ABIs).
extern void *ParseDemoRecord(char *source); // TH10 0x004296f0
extern void DestroyDemoParseObject(void *parsed); // TH10 0x004294a0
extern void ReleaseOwnerTrackedValue(void *owner, void *value); // TH10 0x004493e0
extern void ReleaseHandleTargetByPointer(void *target); // TH10 0x00409e50 (EAX target)
extern void QueueTransitionBgmGate(); // TH10 0x0044be20
extern void RunModeStopGate(); // TH10 0x00420c00
extern void StartBgmQueue(u32 channel, const char *name); // TH10 0x00420a90
extern void ResetBgmQueue(u32 channel, u32 value); // TH10 0x00420b10
extern u32 GetTickTimeSource(); // TH10 0x00463b2c
// Manager per-state calc bodies (each its own target).
extern i32 RunManagerStateBody1(void *m); // TH10 0x0042d300
extern i32 RunManagerStateBody2(void *m); // TH10 0x0042d420
extern i32 RunManagerStateBody4(void *m); // TH10 0x0042d920
extern i32 RunManagerStateBody5(void *m); // TH10 0x0042f540
extern i32 RunManagerStateBody6(void *m); // TH10 0x00430320
extern i32 RunManagerStateBody7(void *m); // TH10 0x004306a0
extern i32 RunManagerStateBody8(void *m); // TH10 0x00430a60
extern i32 RunManagerStateBody9(void *m); // TH10 0x00430ff0
extern i32 RunManagerStateBodyB(void *m); // TH10 0x00431ee0
extern i32 RunManagerStateBodyC(void *m); // TH10 0x004315c0
extern i32 RunManagerStateBodyE(void *m); // TH10 0x00433ef0
extern i32 RunManagerStateBodyF(void *m); // TH10 0x00432cb0
extern i32 RunManagerStateBody10(void *m); // TH10 0x00433570
// Manager per-state draw bodies (each its own target).
extern i32 RunManagerDrawBody9(void *m); // TH10 0x00431410
extern i32 RunManagerDrawBodyB(void *m); // TH10 0x004329f0
extern i32 RunManagerDrawBodyC(void *m); // TH10 0x00431ba0
extern i32 RunManagerDrawBodyF(void *m); // TH10 0x00433230
extern i32 RunManagerDrawBody10(void *m); // TH10 0x00433b30
// Geometry defaults copied into the title screen +0x24 sub-object.
extern u32 g_TitleGeometryDefaults[4]; // TH10 DAT_00474e88
extern u16 g_TitleGeometryWord; // TH10 DAT_00474e98

// TH10 0x00420ea0. Framework callback-thread bootstrap used by the title
// screen. The native caller supplies the context owner (0x00491c28) in EDI,
// the worker entry (0x00417c70) and a zero argument. Under the context state
// lock at +0x6dc it stops the embedded thread control at +0x630, seeds the
// entry +0x644 and flags +0x63c/+0x638, then starts the CRT thread.
void StartMainChainCallbackThread(void *context, void *entry, void *argument)
{
    u8 *const bytes = static_cast<u8 *>(context);
    EnterCriticalSectionInternal(bytes + 0x6dc);
    ++*reinterpret_cast<u8 *>(bytes + 0x6fa);

    ThreadControl *const control =
        reinterpret_cast<ThreadControl *>(bytes + 0x630);
    StopThreadControl(control);
    *reinterpret_cast<void **>(bytes + 0x644) = entry;
    *reinterpret_cast<u32 *>(bytes + 0x63c) = 1;
    *reinterpret_cast<u32 *>(bytes + 0x638) = 0;
    control->thread_handle = reinterpret_cast<void *>(_beginthreadex(
        0, 0, reinterpret_cast<CrtThreadStartFn>(entry), argument, 0,
        &control->thread_id));

    LeaveCriticalSectionInternal(bytes + 0x6dc);
    --*reinterpret_cast<u8 *>(bytes + 0x6fa);
}

// TH10 0x0042d2e0. Game-manager calculation-channel adapter. The native body
// is `mov ecx, eax; jmp 0x0042cdf0`; the manager passed in ECX is forwarded
// to the calculation controller.
i32 GameManagerCalculationChannel(void *game_manager)
{
    return RunGameManagerCalculationBody(game_manager);
}

// TH10 0x0042d2f0. Game-manager draw-channel adapter. The native body is
// `mov ecx, eax; jmp 0x0042d260`; the manager passed in ECX is forwarded to
// the draw controller.
i32 GameManagerDrawChannel(void *game_manager)
{
    return RunGameManagerDrawBody(game_manager);
}

// TH10 0x42caa0. Game-manager worker channel installation, run by the worker
// thread before its loading wait. Registers two ChainElem records bound to
// the manager (calculation priority 6 at +0x0c, draw priority 3 at +0x10),
// requests the title data work slots 0x19/0x1a ("title.anm"/"title_v.anm")
// into +0x14/+0x18, then flags +0xf4 and clears the boot gate. A null work
// result reports through the error receiver and returns -1 without undoing
// the already-published words (matching the original partial state).
i32 InstallGameManagerWorkerChannels(void *game_manager)
{
    u32 *const words = static_cast<u32 *>(game_manager);

    ChainElem *const calculation =
        CallbackSchedulerApi::Create(
            reinterpret_cast<ChainCallback>(&GameManagerCalculationChannel));
    calculation->flags &= ~ChainElemFlag_Enabled;
    calculation->arg = game_manager;
    CallbackSchedulerApi::AddToCalculationChain(g_CallbackScheduler,
                                                calculation, 6);
    words[0x0c / 4] = reinterpret_cast<u32>(calculation);

    ChainElem *const draw =
        CallbackSchedulerApi::Create(
            reinterpret_cast<ChainCallback>(&GameManagerDrawChannel));
    draw->flags &= ~ChainElemFlag_Enabled;
    draw->arg = game_manager;
    CallbackSchedulerApi::AddToDrawChain(g_CallbackScheduler, draw, 3);
    words[0x10 / 4] = reinterpret_cast<u32>(draw);

    ManagerWorkOwnerPartial *const owner =
        reinterpret_cast<ManagerWorkOwnerPartial *>(g_MainChainRenderOwner);
    ManagerWorkPartial *const title_work =
        RequestManagerWork(owner, 0x19, "title.anm");
    words[0x14 / 4] = reinterpret_cast<u32>(title_work);
    if (title_work == 0) {
        ReportMainChainErrorText(
            g_MainChainErrorReceiver,
            reinterpret_cast<const char *>(0x0046cb68));
        return -1;
    }

    ManagerWorkPartial *const title_v_work =
        RequestManagerWork(owner, 0x1a, "title_v.anm");
    words[0x18 / 4] = reinterpret_cast<u32>(title_v_work);
    if (title_v_work == 0) {
        ReportMainChainErrorText(
            g_MainChainErrorReceiver,
            reinterpret_cast<const char *>(0x0046cb68));
        return -1;
    }

    words[0xf4 / 4] = 1;
    g_MainChainManagerGate = 0; // DAT_004918a4
    return 0;
}

// TH10 0x0042c9f0. Game-manager worker thread (CRT entry, argument ignored).
// Installs the manager's scheduler records and title work slots; on failure
// it selects a fallback shared status from runtime-flag bit 12 and returns.
// On success it sleeps 16 ms per iteration (KERNEL32!Sleep through the IAT
// slot 0x004660ac) until the global lifecycle manager's frame counter
// reaches 0x12c or runtime flag bit 0x80 requests an early exit, then
// releases the loading slot at the render owner + 0x3ad070, enables the
// calculation record at manager +0x0c, and returns zero.
u32 TH10_CDECL GameManagerWorkerThread(void *unused)
{
    (void)unused;
    u32 *const manager = static_cast<u32 *>(g_GameManager); // DAT_0047784c
    if (InstallGameManagerWorkerChannels(manager) != 0) {
        g_MainChainSharedStatus = // DAT_00491fb8 fallback mode
            ((~(g_MainChainRuntimeFlags >> 12) & 1U) | 2U);
        return 0;
    }

    if (g_GlobalLifecycleManager != 0) {
        while (g_GlobalLifecycleManager->draw_frame_count < 0x12c &&
               (g_MainChainRuntimeFlags & 0x80U) == 0)
            g_LoadWaitFrameYield(0x10);

        u32 *const loading_slot = reinterpret_cast<u32 *>(
            static_cast<u8 *>(g_MainChainRenderOwner) + 0x3ad070);
        if (*loading_slot != 0) {
            ReleaseLargeRenderOwnerSlot();
            FreeMainChainObject(reinterpret_cast<void *>(*loading_slot));
            *loading_slot = 0;
        }
    }

    ChainElem *const calculation =
        reinterpret_cast<ChainElem *>(manager[0x0c / 4]);
    calculation->flags |= ChainElemFlag_Enabled;
    return 0;
}

// TH10 0x00418c40. Title-screen +0x24 sub-object initializer. The native
// body clears 0x34 bytes, seeds two 600-unit extents, one-byte flags, dword
// +0 = 0x100003 and bit 0x100 at +0x30, then copies five geometry defaults
// from DAT_00474e88/DAT_00474e8c/DAT_00474e90/DAT_00474e94/DAT_00474e98
// and stores the final byte fields. In CreateTitleScreen this runs before
// the whole 0x60-byte allocation is zeroed, so its writes do not survive
// there; the function is still modeled as the standalone reusable initializer.
void InitializeTitleScreenSubObject(void *sub_object)
{
    u8 *const bytes = static_cast<u8 *>(sub_object);
    u32 *const words = reinterpret_cast<u32 *>(bytes);
    memset(bytes, 0, 0x34);
    *reinterpret_cast<u16 *>(bytes + 0x16) = 600;
    *reinterpret_cast<u16 *>(bytes + 0x18) = 600;
    bytes[0x1a] = 0;
    bytes[0x1b] = 1;
    bytes[0x1c] = 1;
    bytes[0x1d] = 0;
    bytes[0x1e] = 0;
    bytes[0x1f] = 2;
    bytes[0x20] = 100;
    bytes[0x21] = 0x50;
    bytes[0x22] = 0;
    words[0] = 0x100003U;
    words[1] = g_TitleGeometryDefaults[0];
    words[2] = g_TitleGeometryDefaults[1];
    words[3] = g_TitleGeometryDefaults[2];
    words[4] = g_TitleGeometryDefaults[3];
    *reinterpret_cast<u16 *>(bytes + 0x14) = g_TitleGeometryWord;
    words[0x30 / 4] |= 0x100U;
}

// TH10 0x00418b80. Shared frame/state block reset used by the title and
// game-manager controllers. The native body is __thiscall with this = ECX =
// &DAT_00474c40 plus one stack value; the owner is the fixed global block,
// so this is modeled with an explicit value argument.
void ResetMainChainFrameStateBlock(u32 value)
{
    u32 *const words = g_MainChainFrameStateBlock; // DAT_00474c40
    words[0x0c / 4] = value / 10;
    const u32 flags = words[0x24 / 4];
    if ((flags & 1U) == 0) {
        words[0x24 / 4] = flags | 1U;
        words[0x18 / 4] = 0;
        words[0x14 / 4] = static_cast<u32>(-999999);
        words[0x1c / 4] = 0;
        words[0x20 / 4] =
            reinterpret_cast<u32>(&g_MainChainTimeScaleTarget);
    }
    words[0x18 / 4] = 0;
    words[0x1c / 4] = 0;
    words[0x14 / 4] = static_cast<u32>(-1);
}

// TH10 0x0042c5c0 / 0x0042c670 / 0x0042c770 helpers were moved to
// GameManagerState.cpp (exported via GameManagerState.hpp).

// TH10 0x0042cdf0. Game-manager per-frame calculation controller (manager in
// EAX). State machine over manager + 0x1c with the attract/demo preamble,
// per-state dispatch, and the disasm-arbitrated rate-tracker tail.
i32 RunGameManagerCalculationBody(void *game_manager)
{
    u8 *const bytes = static_cast<u8 *>(game_manager);
    u32 *const words = reinterpret_cast<u32 *>(bytes);

    u32 state = words[0x1c / 4];
    u32 flow;
    bool flow_loaded = false;
    if (state == 1 || state == 2) {
        const u32 waited = g_DemoWaitCounter + 1;
        g_DemoWaitCounter = waited;
        if ((g_MainChainPresentDiagnosticOptions & 0x160bU) == 0) {
            if (waited >= 900) {
                g_GlobalModeFlags =
                    (g_GlobalModeFlags & ~0x40U) | 0x20U;
                const char *source = g_DemoNameTable[g_DemoNameIndex];
                char *scratch = reinterpret_cast<char *>(0x00477710);
                char copied;
                do {
                    copied = *source;
                    *scratch = copied;
                    ++source;
                    ++scratch;
                } while (copied != '\0');

                void *const parsed =
                    ParseDemoRecord(reinterpret_cast<char *>(0x00477710));
                g_DemoNameIndex = (g_DemoNameIndex + 1) & 3U;

                u32 slot = 0;
                u32 *scan = reinterpret_cast<u32 *>(
                    static_cast<u8 *>(parsed) + 0xb0);
                while (slot < 8 && *scan == 0) {
                    ++slot;
                    scan += 0x24 / 4;
                }
                g_StageSelectorIndex = slot;
                g_StageSelectorCurrent = slot;
                g_MainChainSharedStatus = 12;

                u8 *const cfg = *reinterpret_cast<u8 **>(
                    static_cast<u8 *>(parsed) + 0x18);
                g_StageTextSprites[0] = *reinterpret_cast<u32 *>(cfg + 0x50);
                g_ModeRecordTablePointer = 0x00474788U + slot * 0x30;
                g_StageTextSprites[1] = *reinterpret_cast<u32 *>(cfg + 0x54);
                g_StageScoreSelector[1] = g_StageScoreSelector[0];
                g_StageScoreSelector[0] =
                    *reinterpret_cast<u32 *>(cfg + 0x58);

                DestroyDemoParseObject(parsed);
                FreeMainChainObject(parsed);

                g_DemoWaitCounter = 0;
                flow = (state != 1) ? 1U : 0U;
                g_MainChainManagerFlow = flow;
                flow_loaded = true;
            }
        } else {
            g_DemoWaitCounter = 0;
        }
    }
    if (!flow_loaded)
        flow = g_MainChainManagerFlow;

    switch (state) {
    case 0: {
        u8 *const owner_bytes = static_cast<u8 *>(g_MainChainRenderOwner);
        ReleaseOwnerTrackedValue(
            owner_bytes, *reinterpret_cast<void **>(owner_bytes + 0x3ad084));
        ReleaseOwnerTrackedValue(
            owner_bytes, *reinterpret_cast<void **>(owner_bytes + 0x3ad088));
        ReleaseOwnerTrackedValue(
            owner_bytes, *reinterpret_cast<void **>(owner_bytes + 0x3ad074));
        ReleaseOwnerTrackedValue(
            owner_bytes, *reinterpret_cast<void **>(owner_bytes + 0x3ad078));
        ReleaseOwnerTrackedValue(
            owner_bytes, *reinterpret_cast<void **>(owner_bytes + 0x3ad06c));

        u8 *const ascii_target =
            static_cast<u8 *>(g_AsciiManagerHost) + 0x89a4;
        ReleaseHandleTargetByPointer(ascii_target);
        *reinterpret_cast<u32 *>(ascii_target) = 0;

        if (flow == 3) {
            words[0x2c / 4] = 10;
            const i32 t = static_cast<i32>(words[0x2c / 4]);
            words[0x24 / 4] = (t >= 0) ? 0U : static_cast<u32>(t - 1);
            QueueTransitionBgmGate();
            SetGameManagerState(game_manager, 0xf);
            SpawnManagerEntityFromScript(game_manager, 0x5b);
            SetManagerSlotEntityStopWord(game_manager, 0x5b, 9);
            reinterpret_cast<ChainElem *>(words[0x10 / 4])->flags |=
                ChainElemFlag_Enabled;
            g_MainChainManagerFlow = 1;
            RunManagerStateBodyF(game_manager);
            break;
        }

        if ((g_GlobalModeFlags & 0x20U) == 0) {
            StartBgmQueue(0, "bgm/th10_02.wav");
            ResetBgmQueue(0, 0);
            flow = g_MainChainManagerFlow;
        } else {
            g_StageScoreSelector[0] = g_StageScoreSelector[1];
        }
        g_GlobalModeFlags &= ~0x20U;

        if (flow != 0) {
            if (flow == 1) {
                if (g_StageScoreSelector[0] == 4) {
                    const i32 t = static_cast<i32>(words[0x2c / 4]);
                    if (t == 0 || t > 1)
                        words[0x24 / 4] = 1;
                    else
                        words[0x24 / 4] = static_cast<u32>(t - 1);
                }
                SetGameManagerState(game_manager, 2);
                SpawnManagerEntityFromScript(game_manager, 0x5b);
                reinterpret_cast<ChainElem *>(words[0x10 / 4])->flags |=
                    ChainElemFlag_Enabled;
                RunManagerStateBody2(game_manager);
                break;
            }
            if (flow == 2) {
                words[0x2c / 4] = 10;
                const i32 t = static_cast<i32>(words[0x2c / 4]);
                if (t == 0 || t > 3)
                    words[0x24 / 4] = 3;
                else
                    words[0x24 / 4] = static_cast<u32>(t - 1);
                QueueTransitionBgmGate();
                SetGameManagerState(game_manager, 0xc);
                SpawnManagerEntityFromScript(game_manager, 0x5b);
                SetManagerSlotEntityStopWord(game_manager, 0x5b, 9);
                reinterpret_cast<ChainElem *>(words[0x10 / 4])->flags |=
                    ChainElemFlag_Enabled;
                g_MainChainManagerFlow = 1;
                RunManagerStateBodyC(game_manager);
                break;
            }
            RunManagerStateBody1(game_manager);
            break;
        }
        SetGameManagerState(game_manager, 1);
        g_MainChainManagerFlow = 1;
        RunManagerStateBody1(game_manager);
        break;
    }
    case 1:
        RunManagerStateBody1(game_manager);
        break;
    case 2:
        reinterpret_cast<ChainElem *>(words[0x10 / 4])->flags |=
            ChainElemFlag_Enabled;
        RunManagerStateBody2(game_manager);
        break;
    case 3:
        g_MainChainSharedStatus =
            ((~(g_MainChainRuntimeFlags >> 12) & 1U) | 2U);
    case 0xa:
    case 0xd:
        RunModeStopGate();
        break;
    case 4:
        RunManagerStateBody4(game_manager);
        break;
    case 5:
        RunManagerStateBody5(game_manager);
        break;
    case 6:
        RunManagerStateBody6(game_manager);
        break;
    case 7:
        RunManagerStateBody7(game_manager);
        break;
    case 8:
        RunManagerStateBody8(game_manager);
        break;
    case 9:
        RunManagerStateBody9(game_manager);
        break;
    case 0xb:
        RunManagerStateBodyB(game_manager);
        break;
    case 0xc:
        RunManagerStateBodyC(game_manager);
        break;
    case 0xe:
        RunManagerStateBodyE(game_manager);
        break;
    case 0xf:
        RunManagerStateBodyF(game_manager);
        break;
    case 0x10:
        RunManagerStateBody10(game_manager);
        break;
    default:
        break;
    }

    words[0x2b0 / 4] = words[0x2b4 / 4];
    float *const rate_ptr = *reinterpret_cast<float **>(bytes + 0x2bc);
    const float rate = *rate_ptr;
    const float rate_low = *reinterpret_cast<const float *>(0x00470b68);
    const float rate_high = *reinterpret_cast<const float *>(0x00470b64);
    if (rate_low < rate && rate < rate_high) {
        *reinterpret_cast<float *>(bytes + 0x2b8) +=
            *reinterpret_cast<const float *>(0x00470afc);
        words[0x2b4 / 4] = words[0x2b4 / 4] + 1;
    } else {
        *reinterpret_cast<float *>(bytes + 0x2b8) += rate;
        words[0x2b4 / 4] = GetTickTimeSource();
    }
    return 1;
}

// TH10 0x0042d260. Game-manager draw-channel controller (manager in ECX).
// Only the draw-heavy states dispatch to a per-state draw body; all others
// simply return one.
i32 RunGameManagerDrawBody(void *game_manager)
{
    const u32 state =
        *reinterpret_cast<const u32 *>(
            static_cast<const u8 *>(game_manager) + 0x1c);
    switch (state) {
    case 0x9:
        RunManagerDrawBody9(game_manager);
        break;
    case 0xb:
        RunManagerDrawBodyB(game_manager);
        break;
    case 0xc:
        RunManagerDrawBodyC(game_manager);
        break;
    case 0xf:
        RunManagerDrawBodyF(game_manager);
        break;
    case 0x10:
        RunManagerDrawBody10(game_manager);
        break;
    default:
        break;
    }
    return 1;
}

// TH10 0x00417c70. Title callback-thread worker (CRT entry, argument
// ignored). The native body is a one-call wrapper that forwards the
// published title screen to the startup/init orchestration at 0x00417870.
u32 TH10_CDECL TitleScreenCallbackWorker(void *unused)
{
    (void)unused;
    return static_cast<u32>(RunTitleScreenStartupBody(g_TitleScreen));
}

// TH10 0x004187d0. Title-screen draw-record callback (record registered by
// the startup orchestration with the manager's arg field set to the title
// screen). Unless the title boot flag bit 2 at +0x58 is set, it zeroes the
// four render-owner words at +0x4c/+0x50/+0x54/+0x58, then returns one.
i32 TitleScreenDrawCallback(void *title_screen)
{
    u8 *const bytes = static_cast<u8 *>(title_screen);
    if ((*reinterpret_cast<u32 *>(bytes + 0x58) & 4U) == 0) {
        u32 *const owner = static_cast<u32 *>(g_MainChainRenderOwner);
        owner[0x4c / 4] = 0;
        owner[0x50 / 4] = 0;
        owner[0x54 / 4] = 0;
        owner[0x58 / 4] = 0;
    }
    return 1;
}

// TH10 0x004187c0. Title-screen calculation-record callback. The native body
// is a one-instruction wrapper that forwards the title screen (passed in
// ECX) to the title calc controller at 0x00418190.
i32 TitleScreenCalcCallback(void *title_screen)
{
    return RunTitleScreenCalcBody(title_screen);
}

} // namespace

// TH10 0x004180e0. Allocates the 0x60-byte title screen, builds its +0x24
// sub-object through 0x00418c40 and then zeroes the whole allocation (the
// observed binary order), invokes the render-owner callback, stores the
// caller mode argument at +0x5c, sets flag bit 2 at +0x58, publishes
// DAT_00477810 and starts the callback thread. Native ABI is ret 4.
void *CreateTitleScreen(u32 mode)
{
    u32 *title = static_cast<u32 *>(AllocateMainChainObject(0x60));
    if (title != 0) {
        title[0x20 / 4] &= ~1U;
        InitializeTitleScreenSubObject(
            reinterpret_cast<u8 *>(title) + 0x24);
        for (u32 word = 0; word != 0x18; ++word)
            title[word] = 0;
    }
    InvokeTitleOwnerHook();
    // The native code writes +0x58/+0x5c even after an allocation failure
    // (through a null pointer); the reconstruction guards them instead.
    if (title != 0) {
        title[0x5c / 4] = mode;
        title[0x58 / 4] |= 4U;
    }
    g_TitleScreen = title;
    StartMainChainCallbackThread(
        reinterpret_cast<void *>(0x00491c28),
        reinterpret_cast<void *>(&TitleScreenCallbackWorker),
        reinterpret_cast<void *>(0));
    return title;
}

// TH10 0x00418150. Null-checks the object, runs the in-place teardown at
// 0x00417c80 (which itself clears DAT_00477810) and releases the outer
// allocation. The wrapper does not clear the global.
void DestroyTitleScreen(void *title_screen)
{
    if (title_screen != 0) {
        TeardownTitleScreenInPlace(title_screen);
        FreeMainChainObject(title_screen);
    }
}

// TH10 0x0042cd50. Allocates the 0x5acc-byte game manager, runs the
// in-place constructor at 0x0042c920 (which blanks the whole object, sets
// flag bit 1 at +4 and publishes DAT_0047784c), stops any previously seeded
// thread control at +0x5ab0, then seeds the worker entry +0x5ac8 / active
// flag +0x5ac0 / stop flag +0x5abc and starts GameManagerWorkerThread
// (0x0042c9f0) with the manager as its argument.
void *CreateGameManager()
{
    u8 *manager = static_cast<u8 *>(AllocateMainChainObject(0x5acc));
    if (manager != 0) {
        manager = static_cast<u8 *>(ConstructGameManagerInPlace(manager));
        if (manager != 0) {
            ThreadControl *const control =
                reinterpret_cast<ThreadControl *>(manager + 0x5ab0);
            StopThreadControl(control);
            control->thread_entry =
                reinterpret_cast<void *>(&GameManagerWorkerThread);
            control->field_0010 = 1;
            control->stop_requested = 0;
            control->thread_handle = reinterpret_cast<void *>(_beginthreadex(
                0, 0,
                reinterpret_cast<CrtThreadStartFn>(&GameManagerWorkerThread),
                manager, 0, &control->thread_id));
        }
    }
    return manager;
}

// TH10 0x0042cb60. Game-manager in-place teardown. Destructor vtable first,
// then under the callback lock the two callback records (+0x0c/+0x10) are
// removed, the large-owner tracked slots at DAT_00491c10 + 0x3ad0d0 /
// 0x3ad0d4 are released, the 0x32 subordinate manager objects from +0x59e4
// are destroyed, a stored entity-wide stop is cancelled when the entity id
// is found in either entity list, the owned buffer at +0x5aac is freed,
// DAT_0047784c is cleared and the thread-control marker at +0x5ab0 is
// restored before the periodic hook runs again.
void TeardownGameManagerInPlace(void *game_manager)
{
    u32 *const manager = reinterpret_cast<u32 *>(game_manager);
    ThreadControl *const control = reinterpret_cast<ThreadControl *>(
        reinterpret_cast<u8 *>(game_manager) + 0x5ab0);
    manager[0] = 0x0046ecf0U; // destructor vtable
    StopThreadControl(control);

    for (u32 record_index = 3; record_index <= 4; ++record_index) {
        ChainElem *const record = reinterpret_cast<ChainElem *>(
            manager[record_index]);
        if (record != 0)
            CallbackSchedulerApi::RemoveSynchronized(g_CallbackScheduler,
                                                     record);
    }

    u8 *const owner = static_cast<u8 *>(g_MainChainRenderOwner);
    for (u32 slot = 0x3ad0d0; slot <= 0x3ad0d4; slot += 4) {
        u32 *const tracked = reinterpret_cast<u32 *>(owner + slot);
        if (*tracked != 0) {
            ReleaseLargeRenderOwnerSlot();
            ReleaseResourceBuffer(reinterpret_cast<void *>(*tracked));
            *tracked = 0;
        }
    }

    for (u32 index = 0; index != 0x32; ++index) {
        void *const subordinate =
            reinterpret_cast<void *>(manager[0x1679 + index]);
        if (subordinate != 0) {
            DestroyOpaqueMainChainManagerInPlace(subordinate);
            ReleaseResourceBuffer(subordinate);
        }
    }

    const i32 entity_id = static_cast<i32>(manager[0x174]);
    u32 **node = 0;
    if (entity_id != 0) {
        const u32 list_head_a = *reinterpret_cast<u32 *>(owner + 0x72dad4);
        u32 **cursor = reinterpret_cast<u32 **>(list_head_a);
        for (; cursor != 0 && node == 0;
                cursor = reinterpret_cast<u32 **>(cursor[1])) {
            if (static_cast<i32>(**cursor) == entity_id)
                node = cursor;
        }
        if (node == 0) {
            const u32 list_head_b =
                *reinterpret_cast<u32 *>(owner + 0x72dadc);
            cursor = reinterpret_cast<u32 **>(list_head_b);
            for (; cursor != 0 && node == 0;
                    cursor = reinterpret_cast<u32 **>(cursor[1])) {
                if (static_cast<i32>(**cursor) == entity_id)
                    node = cursor;
            }
        }
    }
    if (node != 0) {
        void *const entity = reinterpret_cast<void *>(*node);
        if (entity != 0) {
            u8 *const entity_bytes = static_cast<u8 *>(entity);
            *reinterpret_cast<u16 *>(entity_bytes + 0x304) = 1;
            if (*reinterpret_cast<u32 *>(entity_bytes + 0x18) == 0) {
                u32 **child = reinterpret_cast<u32 **>(
                    *reinterpret_cast<u32 *>(entity_bytes + 0x14));
                for (; child != 0;
                        child = reinterpret_cast<u32 **>(child[1])) {
                    *reinterpret_cast<u16 *>(
                        reinterpret_cast<u8 *>(*child) + 0x304) = 1;
                }
            }
        }
    }

    if (manager[0x16ab] != 0) {
        ReleaseResourceBuffer(reinterpret_cast<void *>(manager[0x16ab]));
        manager[0x16ab] = 0;
    }
    g_GameManager = 0; // DAT_0047784c
    manager[0x16ac] = 0x004703e4U; // thread-control destructor marker
    StopThreadControl(control);
}

// TH10 0x0042cdb0. Null-checks the object, runs the in-place teardown at
// 0x0042cb60 (which clears DAT_0047784c and stops the manager thread region)
// and releases the outer allocation.
void DestroyGameManager(void *game_manager)
{
    if (game_manager != 0) {
        TeardownGameManagerInPlace(game_manager);
        FreeMainChainObject(game_manager);
    }
}

} // namespace th10
