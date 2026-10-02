#include "PlayerFrameworkHelpers.hpp"
#include "AsciiHudOwner.hpp"
#include "PlayerShotData.hpp"

#include "EntityHelpers.hpp"
#include "MainChainContext.hpp"
#include "PlayerRecord.hpp"
#include "StageEffectHelpers.hpp"
#include "TimelineRenderObjectSetup.hpp"
#include "PlayerShotSpawner.hpp"
#include "BgmRuntime.hpp"
#include "VmRecord.hpp"

#include <cmath>
#include <string.h>

namespace th10 {

namespace {

extern void *g_TitleScreen; // TH10 DAT_00477810
extern void *g_AsciiHudOwner; // TH10 DAT_0047770c
extern void *g_MainChainRenderOwner; // TH10 DAT_00491c10
extern float g_FrameTimeScale; // TH10 DAT_00476f78
extern i32 g_PlayerPowerGaugeDword; // TH10 DAT_00474c48 (dword view)
extern void *g_TextLayerManagerSlot; // TH10 DAT_00477814 (holds manager)
extern i32 g_ActiveTextLayer; // TH10 DAT_00474c7c
extern void *g_EffectScriptContext; // runtime-filled bind context
extern void *g_EffectManagerRoot; // TH10 DAT_004776f0

// TH10 0x420a90 (stack args): start a BGM track by path.

// TH10 0x43e460 (ECX = root): halt named sound entries.

// TH10 0x448ac0 (ECX = ?, stack/EDX = manager): attach a pool VM to the
// effect script list.

// TH10 0x41a120 (stack args): create a text-effect object from a string.

// TH10 0x448db0: fire a VM operation by id with a float payload.

// TH10 0x404f30 analog for effect records (script id, vm record on stack).

inline i32 ReadInt(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const i32 *>(bytes + offset);
}

inline void WriteInt(u8 *bytes, u32 offset, i32 value)
{
    *reinterpret_cast<i32 *>(bytes + offset) = value;
}

inline float ReadFloat(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const float *>(bytes + offset);
}

inline void WriteFloat(u8 *bytes, u32 offset, float value)
{
    *reinterpret_cast<float *>(bytes + offset) = value;
}

inline u32 ReadUintAt(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const u32 *>(bytes + offset);
}

inline void WriteUintAt(u8 *bytes, u32 offset, u32 value)
{
    *reinterpret_cast<u32 *>(bytes + offset) = value;
}

// The 0x3c8 timer block of an effect record (+0x3c4 is cleared alongside):
// first-init sentinel idiom, then the unconditional stopped-timer reset.
void InitializeRecordTimer(u8 *block)
{
    if ((*reinterpret_cast<u8 *>(block + 0x10) & 1) == 0) {
        WriteInt(block, 4, 0);
        WriteInt(block, 0, static_cast<i32>(0xfff0bdc1U));
        WriteInt(block, 8, 0);
        *reinterpret_cast<const float **>(block + 0xc) = &g_FrameTimeScale;
        *reinterpret_cast<u8 *>(block + 0x10) |= 1;
    }
    WriteInt(block, 4, 0);
    WriteInt(block, 8, 0);
    WriteInt(block, 0, -1);
    WriteInt(block - 4, 0, 0);
}

} // namespace

// TH10 0x0040ac90. Stores the pending state at controller+0x390; when bit
// 12 of the dword rooted at controller+0x3cc is set the request is
// overridden with state 2.
void RequestGameStateTransitionEaxStackAbi(void *controller, i32 state)
{
    MainChainContext &chain = *static_cast<MainChainContext *>(controller);
    // The native reads the full dword at +0x3cc; only its first byte is a
    // named field (callback_state_byte), so the dword read is anchored
    // there.
    if ((*reinterpret_cast<const u32 *>(&chain.callback_state_byte) &
         0x1000) != 0)
        state = 2;
    chain.requested_state = static_cast<MainChainState>(state);
}

// TH10 0x00412e70. The argument is divided by ten (truncated) and
// subtracted from the dword at 0x474c4c with a 5000 floor.
void AddMaximumScorePenalty(i32 amount)
{
    u8 *const block = reinterpret_cast<u8 *>(0x474c40U);
    WriteInt(block, 0xc, ReadInt(block, 0xc) - amount / 10);
    if (ReadInt(block, 0xc) < 5000)
        WriteInt(block, 0xc, 5000);
}

// TH10 0x00413790. Nine 0x3ac-stride slots at hud+0x4cdc (pool_c records:
// the first dword of each is the record's +0x35c flags dword); bit 1 of
// each is the life-icon visibility. count > 8 skips the hide loop entirely.
void RefreshLifeIconsEaxStackAbi(i32 count)
{
    AsciiHudOwner &hud =
        *reinterpret_cast<AsciiHudOwner *>(g_AsciiHudOwner);
    for (i32 index = 0; index < count && index < 9; ++index)
        hud.pool_c[index].flags |= 2U;
    if (count > 8)
        return;
    for (i32 index = count < 0 ? 0 : count; index < 9; ++index)
        hud.pool_c[index].flags &= ~2U;
}

// TH10 0x00424650. Shortens the lifetime of the texts already on the
// active layer, then spawns the popup with the fixed tint and timing.
void ShowCautionText(const float position[3])
{
    void *const manager = *static_cast<void *const *>(g_TextLayerManagerSlot);
    u8 *const layer_list = *reinterpret_cast<u8 *const *>(
        static_cast<u8 *>(manager) + 0x7c +
        static_cast<u32>(g_ActiveTextLayer) * 0xc);
    for (const u32 *node = reinterpret_cast<const u32 *>(layer_list);
         node != 0; node = reinterpret_cast<const u32 *>(node[1])) {
        u8 *const text = reinterpret_cast<u8 *>(node[0]);
        if (text != 0 && ReadInt(text, 0x70) > 0)
            WriteInt(text, 0x70, ReadInt(text, 0x70) - 1);
    }
        void *const created =
        CreateTextEffect(manager, position, "Caution!");
    u8 *const object = static_cast<u8 *>(created);
    const i32 lifetime = ReadInt(object, 0x60) - 0x3c;
    WriteInt(object, 0x60, lifetime > 0 ? lifetime : 1);
    WriteInt(object, 0x6c, 0x5a);
    WriteInt(object, 0x84, static_cast<i32>(0xffff8080U));
    WriteInt(object, 0x70, 0x1e);
}

// TH10 0x00426610. atan2(target.y - player.y, target.x - player.x); the
// pi/2 fallback applies only to the exact (0, 0) case — NaN inputs
// propagate to a NaN result.
float ComputeDeathBurstAngleEcxEaxAbi(void *player_memory,
                                      const float target[2])
{
    const PlayerRecord &player_rec =
        *reinterpret_cast<const PlayerRecord *>(player_memory);
    const float dx = target[0] - player_rec.position_x;
    const float dy = target[1] - player_rec.position_y;
    if (dx == 0.0f && dy == 0.0f)
        return 1.5707964f;
    return static_cast<float>(std::atan2(static_cast<double>(dy),
                                         static_cast<double>(dx)));
}

// TH10 0x0041bb00. Path A (kind != 8) scans 150 effect records of 0x3f0
// bytes at manager+0x14 (free test rec+0x3dc == 0), clamps the source x
// into [-192, 192], initializes the movement block and timer, remaps
// kinds when the power gauge exceeds 99 (1,4 -> 9; 10,11 -> 5), selects
// the script per kind, and stores the color. Path B (kind == 8) uses a
// 2048-entry ring buffer at manager+0x24eb4 without the clamp.
void SpawnExplosionParticleEaxEcxEfxAbi(void *manager_memory,
                                        void *position_memory, i32 kind,
                                        u32 color, float angle, float speed)
{
    u8 *const manager = static_cast<u8 *>(manager_memory);
    const u8 *const position =
        static_cast<const u8 *>(position_memory);
    if (kind == 8) {
        const u32 ring_index =
            ReadUintAt(manager, 0x21ceb8);
        u8 *const record = manager + 0x24eb4 + ring_index * 0x3f0;
        if (ReadInt(record, 0x3dc) == 0) {
            const u32 count = ReadUintAt(manager, 0x21cebc) + 1;
            WriteUintAt(manager, 0x21cebc, count);
            i32 spread;
            if (count >= 0x400)
                spread = static_cast<i32>(count % 256);
            else if (count >= 0x200)
                spread = static_cast<i32>(count % 128) + 4;
            else if (count >= 0x100)
                spread = static_cast<i32>(count % 64) + 8;
            else
                spread = static_cast<i32>(count % 32) + 16;
            WriteInt(record, 0x3ec, spread);
            WriteInt(record, 0x3e0, 8);
            WriteInt(record, 0x3e4, 8);
            WriteInt(record, 0x3dc, 5);
            WriteFloat(record, 0x3ac, ReadFloat(position, 0));
            WriteFloat(record, 0x3b0, ReadFloat(position, 4));
            WriteFloat(record, 0x3b4, ReadFloat(position, 8));
            InitMovementBlock(record + 0x3b8, angle, speed);
            WriteInt(record, 0x3c0, 0);
            InitializeRecordTimer(record + 0x3c8);
        }
        *reinterpret_cast<u32 *>(manager + 0x21ceb8) =
            (ring_index + 1) % 2048U;
        return;
    }

    for (u32 index = 0; index != 150; ++index) {
        u8 *const record = manager + 0x14 + index * 0x3f0;
        if (ReadInt(record, 0x3dc) != 0)
            continue;
        WriteInt(record, 0x3dc, 1);
        WriteFloat(record, 0x3ac, ReadFloat(position, 0));
        WriteFloat(record, 0x3b0, ReadFloat(position, 4));
        WriteFloat(record, 0x3b4, ReadFloat(position, 8));
        float x = ReadFloat(record, 0x3ac);
        if (x <= -192.0f)
            x = -192.0f;
        else if (x >= 192.0f)
            x = 192.0f;
        WriteFloat(record, 0x3ac, x);
        InitMovementBlock(record + 0x3b8, angle, speed);
        WriteInt(record, 0x3c0, 0);
        InitializeRecordTimer(record + 0x3c8);

        i32 script_kind = kind;
        if (g_PlayerPowerGaugeDword > 99 && kind >= 1 && kind <= 0xb) {
            if (kind == 1 || kind == 4)
                script_kind = 9;
            else if (kind == 10 || kind == 0xb)
                script_kind = 5;
        }
        if (ReadInt(record, 0x3e0) == 3)
            (void)SpawnStageEffectEdxEbxAbi(
                *reinterpret_cast<void *const *>(
                    static_cast<u8 *>(g_EffectManagerRoot) + 0x3e0b50),
                &angle, 0x189);
        i32 script_id = 0x176 + script_kind;
        if (script_kind == 10) {
            WriteInt(record, 0x3e4, 1);
            script_id = 0x177;
        } else if (script_kind == 0xb) {
            WriteInt(record, 0x3e4, 4);
            script_id = 0x17a;
        }
        WriteInt(record, 0x3e0, script_kind);
        // The effect context that owns the script table is the manager
        // root itself; the native call shape (0x404f30) is ESI=vm,
        // stack=context, EAX=script id.
        InitializePlayerMainVmEsiStackAbi(record + 0x14, manager,
                                          script_id);
        VmRecord &record_vm = *reinterpret_cast<VmRecord *>(record);
        record_vm.primary_color = color;
        return;
    }
}

// TH10 0x004231d0. Switches the manager to state 6, resets its timer,
// spawns the two game-over overlay VMs, starts the game-over BGM, halts
// the named sound entries, and resets the global slowdown.
void RunGameOverPathBStackAbi(void *manager_memory, i32 param)
{
    u8 *const manager = static_cast<u8 *>(manager_memory);
    // The 0x477810 global holds the 0x60 title-screen object pointer
    // directly (native single dereference, TH10 0x004231d0).
    u8 *const title_screen = static_cast<u8 *>(g_TitleScreen);
    if (title_screen != 0 && ReadInt(title_screen, 0x5c) == 1) {
        extern i32 g_PostGameOverState; // TH10 DAT_00491fb8
        extern u32 g_ReplayModeFlags; // TH10 DAT_00491ff4
        g_PostGameOverState =
            (g_ReplayModeFlags & 0x1000) != 0 ? 2 : 4;
        return;
    }
    WriteInt(manager, 4, 6);
    if ((ReadInt(manager, 0x20) & 1) == 0) {
        WriteInt(manager, 0x14, 0);
        WriteInt(manager, 0x18, 0);
        WriteInt(manager, 0x10, static_cast<i32>(0xfff0bdc1U));
        *reinterpret_cast<const float **>(manager + 0x1c) =
            &g_FrameTimeScale;
        WriteInt(manager, 0x20, ReadInt(manager, 0x20) | 1);
    }
    WriteInt(manager, 0x14, 0);
    WriteInt(manager, 0x18, 0);
    WriteInt(manager, 0x10, -1);
    u8 *const title = static_cast<u8 *>(g_TitleScreen);
    if (title != 0)
        WriteInt(title, 0x58, ReadInt(title, 0x58) | 0x10);

    // Native order: spawn script 0 (id -> +0x1d8), build the overlay with
    // that id, publish front_anm_work at +0x2c4, then spawn script 0x80
    // (id -> +0x1d4).
    u32 overlay_id = 0;
    {
        void *const vm = AllocatePoolVmEsiAbi(g_MainChainRenderOwner);
        VmRecord &vm_record = *reinterpret_cast<VmRecord *>(vm);
        vm_record.flags |= 0x40000000U;
        vm_record.render_kind = 0xf;
        AssignPoolVmScriptEcxEaxAbi(vm, 0);
        AttachEffectVmToListB(&overlay_id, vm, g_MainChainRenderOwner);
        WriteInt(manager, 0x1d8, static_cast<i32>(overlay_id));
    }
    (void)CreateGameOverOverlay(g_MainChainRenderOwner,
                                static_cast<i32>(overlay_id),
                                0x20, 0x10, 0x180, 0x1c0);
    AsciiHudOwner &hud =
        *reinterpret_cast<AsciiHudOwner *>(g_AsciiHudOwner);
    WriteInt(manager, 0x2c4,
             static_cast<i32>(reinterpret_cast<u32>(hud.front_anm_work)));
    {
        void *const vm = AllocatePoolVmEsiAbi(g_MainChainRenderOwner);
        VmRecord &vm_record = *reinterpret_cast<VmRecord *>(vm);
        vm_record.flags |= 0x40000000U;
        vm_record.render_kind = 0xf;
        AssignPoolVmScriptEcxEaxAbi(vm, 0x80);
        AttachEffectVmToListB(&overlay_id, vm, g_MainChainRenderOwner);
        WriteInt(manager, 0x1d4, static_cast<i32>(overlay_id));
    }
    StartBgmTrack("bgm/th10_17.wav", 0);
    extern u32 g_SoundStopFlags; // TH10 DAT_00491d78
    extern TransitionRootPartial g_TransitionRoot; // TH10 DAT_00492590
    if ((g_SoundStopFlags & 0x10) != 0)
        QueueBgmCommand(&g_TransitionRoot, "dummy", 4, 0);
    QueueBgmCommand(&g_TransitionRoot, "dummy", 2, 0);
    *reinterpret_cast<u8 *>(0x47783cU + 0x1d8a3U) = 1;
    WriteInt(manager, 0x1e4, 0);
    WriteFloat(manager, 0x2c0, g_FrameTimeScale);
    g_FrameTimeScale = 1.0f;
}

// TH10 0x0043e7e0. Binds the effect script to the VM; a missing script or
// the context stop flag wipes the whole 0x3ac-byte record instead.
void BindEffectScriptContextEaxEcxDxAbi(void *context_memory,
                                        i32 script_index, void *vm_memory)
{
    u8 *const vm = static_cast<u8 *>(vm_memory);
    VmRecord &rec = *reinterpret_cast<VmRecord *>(vm);
    u8 *const context = static_cast<u8 *>(context_memory);
    void *const script_list =
        *reinterpret_cast<void *const *>(context + 0x11c);
    const bool stopped = ReadInt(context, 0x124) != 0;
    void *const script = script_list != 0 && !stopped
        ? reinterpret_cast<void **>(script_list)[script_index]
        : static_cast<void *>(0);
    if (script == 0 || stopped) {
        memset(vm, 0, 0x3ac);
        return;
    }
    rec.bound_script_id = static_cast<u16>(script_index);
    rec.bound_file_id = *reinterpret_cast<const u16 *>(context);
    rec.bound_resource = context;
    rec.flags &= ~0x600U;
    rec.script_base = script;
    rec.current_instruction = script;
    if ((rec.timer_flags & 1) == 0) {
        rec.timer_cur = 0;
        rec.timer_prev = static_cast<i32>(0xfff0bdc1U);
        rec.timer_accum = 0.0f;
        rec.timer_rate = &g_FrameTimeScale;
        rec.timer_flags |= 1;
    }
    rec.timer_cur = 0;
    rec.timer_accum = 0.0f;
    rec.timer_prev = static_cast<i32>(0xffffffffU);
    rec.flags &= ~1U;
    (void)script_index;
    (void)FinalizeTimelineRenderObjectSetup(vm);
    *reinterpret_cast<u32 *>(static_cast<u8 *>(g_MainChainRenderOwner) +
                             0x4c) += 1;
}

} // namespace th10
