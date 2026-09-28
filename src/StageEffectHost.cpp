// TH10 stage host object (DAT_004776f8, g_StageHostObject) lifecycle and
// the game-mode entry sequence.
//
// The 0x688-byte stage host carries an embedded 0x3ac record at +0x2c8,
// registers the ECL select-menu calc adapter (0x40abe0) and the
// sprite-view overlay draw adapter (0x40abf0), and owns the scene
// configuration table selector published to DAT_00477748.

#include "Th10Types.hpp"
#include "CallbackScheduler.hpp"
#include "ManagerCreation.hpp"
#include "GameManagerState.hpp"
#include "StageHostTeardown.hpp"
#include "SpriteViewDebugText.hpp"

namespace th10 {

extern CallbackScheduler *g_CallbackScheduler;  // TH10 ds:0x491be4
extern void *g_StageHostObject;                 // TH10 DAT_004776f8
extern u32 g_SceneConfigTablePointer;           // TH10 DAT_00477748
extern u32 g_StageIndex;                        // TH10 DAT_00474c7c
extern u32 g_StageIndexMirror;                  // TH10 DAT_00474c80
extern void *g_AsciiHudConditionalState;        // TH10 DAT_00477704

// Cross-module boundaries.
void *CreateGameContextObject(); // GameContextLifecycle.cpp (0x4056b0)
extern void CreateBulletManagerRootBoundary();     // TH10 0x41aed0
extern void CreateMainChainObject840Boundary();    // TH10 0x42b660

// Thunk adapters (native entries; ASM boundaries).
i32 TH10_FASTCALL StageHostEclMenuCalcAdapter(void *host);    // 0x40abe0
i32 TH10_FASTCALL StageHostSpriteViewDrawAdapter(void *host); // 0x40abf0

// TH10 0x0040a040. Native EDX = the 0x688-byte stage host. The native
// writes a series of field seeds (default 0x4703e4 pointer at +0x10,
// counts 999 at +0x44/+0x11c/+0x1f4, seeds 1 at +0x10c/+0x1e4/+0x2bc,
// cleared flags on the embedded +0x2c8 record) and then wipes the WHOLE
// 0x1a2-dword object (order quirk: the seeds are dead stores), sets bit
// 1 of the first dword and publishes DAT_004776f8. Returns the host.
void *InitStageHostObjectEdxAbi(void *host) {
    u8 *bytes = static_cast<u8 *>(host);

    // Dead-seed block preserved for fidelity (erased by the wipe below).
    *reinterpret_cast<u32 *>(bytes + 0x10) = 0x4703e4U;
    *reinterpret_cast<u32 *>(bytes + 0x10c) = 1;
    *reinterpret_cast<u32 *>(bytes + 0x44) = 0x3e7;
    *reinterpret_cast<u32 *>(bytes + 0x11c) = 0x3e7;
    *reinterpret_cast<u32 *>(bytes + 0x1e4) = 1;
    *reinterpret_cast<u32 *>(bytes + 0x1f4) = 0x3e7;
    *reinterpret_cast<u32 *>(bytes + 0x2bc) = 1;
    // Embedded 0x3ac record at +0x2c8: clear its nine busy-flag dwords.
    static const u32 kEmbeddedFlagOffsets[9] = {
        0x6c, 0xb0, 0xfc, 0x128, 0x174, 0x1b0, 0x1fc, 0x228, 0x378};
    for (int i = 0; i < 9; ++i) {
        u32 *flag = reinterpret_cast<u32 *>(
            bytes + 0x2c8 + kEmbeddedFlagOffsets[i]);
        *flag &= ~2u;
    }

    u32 *wipe = reinterpret_cast<u32 *>(bytes);
    for (int i = 0; i < 0x1a2; ++i) {
        wipe[i] = 0;
    }
    *wipe |= 2u;
    g_StageHostObject = host;
    return host;
}

// TH10 0x0040a130. Native EBX = stage host. Selects the default scene
// configuration table (DAT_00477748 = 0x474788), clears the stage-index
// mirrors DAT_00474c7c / DAT_00474c80, and registers two scheduler
// records with the host as their argument: calc 0x40abe0 (ECL select
// menu update adapter) at priority 5 and draw 0x40abf0 (sprite-view
// overlay draw adapter) at priority 0x27, both enabled (flags |= 2).
// Elements land at host+8 / host+0xc. Returns 0.
i32 RegisterStageHostSchedulerRecordsEbxAbi(void *host) {
    u8 *bytes = static_cast<u8 *>(host);
    g_SceneConfigTablePointer = 0x474788U;
    g_StageIndex = 0;
    g_StageIndexMirror = 0;

    ChainElem *calc = CallbackSchedulerApi::Create(
        StageHostEclMenuCalcAdapter);
    calc->flags |= ChainElemFlag_Enabled;
    calc->arg = host;
    CallbackSchedulerApi::AddToCalculationChain(g_CallbackScheduler, calc,
                                                5);
    *reinterpret_cast<ChainElem **>(bytes + 0x8) = calc;

    ChainElem *draw = CallbackSchedulerApi::Create(
        StageHostSpriteViewDrawAdapter);
    draw->flags |= ChainElemFlag_Enabled;
    draw->arg = host;
    CallbackSchedulerApi::AddToDrawChain(g_CallbackScheduler, draw, 0x27);
    *reinterpret_cast<ChainElem **>(bytes + 0xc) = draw;
    return 0;
}

// TH10 0x0040a3c0. operator new(0x688) + init + register; on a
// registration failure destroys (0x40a1a0), frees and returns null.
void *CreateStageHostObject() {
    void *host = ::operator new(0x688U);
    if (host != 0) {
        InitStageHostObjectEdxAbi(host);
    }
    const i32 result = RegisterStageHostSchedulerRecordsEbxAbi(host);
    if (result != 0) {
        if (host != 0) {
            DestroyStageHostObjectStackAbi(host);
            ::operator delete(host);
        }
        return 0;
    }
    return host;
}

// TH10 0x0040ada0. Native EAX = configuration index, ECX = stage host.
// Publishes the selected scene configuration table
// (DAT_00477748 = 0x474788 + index * 0x30) and stores the index at
// host+0x3c and host+0x40.
void SelectStageConfigTableEcxEaxAbi(void *host, u32 index) {
    g_SceneConfigTablePointer = 0x474788U + index * 0x30;
    u8 *bytes = static_cast<u8 *>(host);
    *reinterpret_cast<u32 *>(bytes + 0x3c) = index;
    *reinterpret_cast<u32 *>(bytes + 0x40) = index;
}

// TH10 0x0040add0. Native EAX = the record to reset (usercall; the entry
// carries no cross-references in the canonical binary and stands alone
// between 0x40ada0 and 0x40ae00, so it is preserved as a trivial
// initializer of its EAX record): clears +0x8c, +0x0 and +0xd4, seeds
// +0xd0 with flag bit 1 and sets the +0x8 counter to 999.
void ResetStageHostSubrecordDefaultsEaxAbi(void *record) {
    u32 *fields = static_cast<u32 *>(record);
    fields[0x8c / 4] = 0;
    fields[0] = 0;
    fields[0xd4 / 4] = 0;
    fields[0xd0 / 4] = 1;
    fields[0x8 / 4] = 0x3e7U;
}

// TH10 0x0040d6b0 (StageHostConditionalState.cpp).
void *CreateStageConditionalStateStackAbi(const char *script_path);

// TH10 0x0040a350. Native ESI = the game manager. Full game-mode entry:
// creates the player state block, the effect manager root, the game
// context, the two remaining manager roots (0x41aed0 / 0x42b660), the
// stage conditional state (0x40d6b0) from the manager's selected stage
// script, and the ASCII HUD owner; finally shifts the manager's +0x1ec
// cursor record by +1 and -1 to refresh it (seeding +0x1f4 from the
// conditional state's +0x54 sub-record cursor first) and sets the +0x30
// state to 1. Returns 0.
i32 EnterGameModeSetupEsiAbi(void *game_manager) {
    u8 *bytes = static_cast<u8 *>(game_manager);
    CreatePlayerStateBlock();
    CreateEffectManagerRoot();
    CreateGameContextObject();
    CreateBulletManagerRootBoundary();
    CreateMainChainObject840Boundary();

    // Stage conditional state from the selected stage script:
    // script = manager->stage_table[manager->stage_index] (+0x34 base,
    // +0x114 index).
    const u32 index = *reinterpret_cast<u32 *>(bytes + 0x114);
    void *stage_table = *reinterpret_cast<void **>(bytes + 0x34);
    const char *script_path = *reinterpret_cast<char **>(
        static_cast<u8 *>(stage_table) + index * 4);
    CreateStageConditionalStateStackAbi(script_path);

    CreateAsciiHudOwner();

    // Refresh the +0x1ec cursor record: seed +0x1f4 from the conditional
    // state's +0x54 sub-record cursor, shift +1 then -1, set state 1.
    void *cursor = bytes + 0x1ec;
    void *sub_record = *reinterpret_cast<void **>(
        static_cast<u8 *>(g_AsciiHudConditionalState) + 0x54);
    *reinterpret_cast<u32 *>(bytes + 0x1f4) =
        *reinterpret_cast<u32 *>(static_cast<u8 *>(sub_record) + 0x8);
    ShiftManagerSelector(cursor, 1);
    ShiftManagerSelector(cursor, -1);
    *reinterpret_cast<u32 *>(bytes + 0x30) = 1;
    return 0;
}

} // namespace th10
