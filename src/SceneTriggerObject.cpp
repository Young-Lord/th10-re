// Scene trigger object instruction queue and expire effect.
// Bodies: TH10 0x00426660, 0x00408750, 0x00406d90, 0x00426cf0, 0x004267f0.
#include "SceneTriggerObject.hpp"

#include <math.h>

#include "EntityHelpers.hpp"
#include "EffectManagerRoot.hpp"
#include "GameManagerState.hpp"
#include "PlayerShotData.hpp"
#include "PlayerStageHelpers.hpp"
#include "PlayerTimerHelpers.hpp"
#include "TitleScreenObject.hpp"

namespace th10 {

namespace {

// ---- shared globals ----------------------------------------------------

extern void *g_SceneCommandManager;   // TH10 DAT_004776f0 (+0x3e0b50 = the
                                      // effect VM manager)
extern void *g_EntityPoolManager;     // TH10 DAT_00491c10
extern void *g_ScreenTargetBlock;     // TH10 DAT_00477834 (+0x3c0 anchor)
extern void *g_SoundGateContext;      // TH10 DAT_00492590
extern void *g_SoundGateOwner;        // TH10 DAT_00477810 (+0x58 bit 0x200)
extern void *g_StageState;            // TH10 DAT_004776f4 (stage counters)
extern u32 *g_TriggerVmScriptTable;   // TH10 DAT_00474170
extern u8 g_SceneSingletons;          // TH10 DAT_00477710 (dword at +44)

// Constant pool -----------------------------------------------------------

const float kSpawnOffsetX = 224.0f;   // flt_470b4c
const float kSpawnOffsetY = 16.0f;    // flt_470b48
const float kClampLow = -990.0f;      // flt_470b50 (angle resolve)
const float kClampHigh = 990.0f;      // flt_470ccc (angle resolve)
const float kFallbackYLow = -999.0f;  // flt_470cc8 (opcode 64/128 y gate)
const i32 kQueueCapacity = 18;        // +0x45c compares against 0x12

// Boundary leaf (native register ABI noted at the site).

// TH10 0x004073e0. Native EAX = the 0x21c spawn packet, stack = the scene
// command manager; spawns the queued enemy and installs the packet's copy
// of the remaining instruction stream.
extern void SpawnSceneTriggerPacketEaxStackAbi(void *packet,
                                               void *scene_manager);

// TH10 0x0043dd10 (body in EclScriptLibrary.cpp). Native EBX = sound id,
// ESI = sound manager, stack = float payload.
extern void EnqueueSoundEffectEbxStackAbi(u32 sound_id, void *sound_manager,
                                          float value);

// Scene trigger object field offsets. The instruction-queue interpreter
// (0x406d90) runs on the 0x7f0-byte EffectTriggerRecord pool record (all
// of its offsets land below 0x7f0); the expire-effect paths (0x426cf0 /
// 0x4267f0) run on the larger >= 0x4320-byte stage object that shares the
// low-offset layout (its +0x430c script timer reaches past the pool
// record). Modeled pool-record fields (activation_gate_0004, the +8 VM,
// the +0x3b4 position, the +0x3e4 angle) go through the typed view in the
// queue interpreter; the big-object paths and every unmodeled region stay
// raw.
const u32 kOffScreen = 0x3c0U;        // float3 screen anchor
const u32 kOffFallbackY = 0x3d8U;     // float fallback for opcode 64/128 y
const u32 kOffExtentX = 0x41cU;       // float
const u32 kOffExtentY = 0x420U;       // float (also written by 0x1000000)
const u32 kOffField434 = 0x434U;      // int (opcode 0x2000)
const u32 kOffFlags = 0x43cU;         // accumulated opcode flags
const u32 kOffState = 0x458U;         // state word (4 = expiring)
const u32 kOffIndex = 0x45cU;         // instruction cursor
const u32 kOffQueue = 0x464U;         // 18 entries, stride 0x18

const u32 kOffExpireTimer = 0x474U;   // timer block {prev,count,acc,rate,flags}
const u32 kOffScriptTimer = 0x430cU;  // timer block forced to 5/6/6.0f

// One queued instruction: {u32 a0; u32 a1; u32 a2; u32 a3; u32 opcode;
// u32 has_args;} — byte +9 is the second byte of a2, word +0xc the low half
// of a3.
struct SceneTriggerInstruction {
    u32 arg0;
    u32 arg1;
    u32 arg2;
    u32 arg3;
    u32 opcode;
    u32 has_args;
};

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

inline float LoadFloat(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const float *>(bytes + offset);
}

inline void StoreFloat(u8 *bytes, u32 offset, float value)
{
    *reinterpret_cast<float *>(bytes + offset) = value;
}

// Shared lazy timer initialization (native pattern: the -999999 sentinel at
// the prev field, &flt_476f78 as the rate pointer, guarded by flag bit 0).
void LazyInitTimerBlock(u8 *object, u32 offset)
{
    u32 flags = LoadU32(object, offset + 0x10U);
    if ((flags & 1U) == 0U) {
        StoreU32(object, offset, static_cast<u32>(-999999));
        StoreU32(object, offset + 4U, 0U);
        StoreU32(object, offset + 8U, 0U);
        StoreU32(object, offset + 0xcU, 0x476f78U);
        StoreU32(object, offset + 0x10U, flags | 1U);
    }
}

// Native branch chain for the opcode-16/64/128 angle fields (fcomp against
// flt_470b50/flt_470ccc): values <= -990.0f take the stored +0x3e4
// fallback, NaN and values in (-990, 990) keep the argument, and values
// >= 990.0f resolve to the live angle-to-target.
float ResolveAngleArg(float arg, const EffectTriggerRecord &rec)
{
    if (arg > kClampLow || arg != arg) {
        if (arg < kClampHigh || arg != arg)
            return arg;
        return AngleToScreenTargetEaxEcxAbi(&rec.position_x_03b4,
                                            g_ScreenTargetBlock);
    }
    return rec.raw_angle_03e4;
}

} // namespace

// TH10 0x00426660.
float AngleToScreenTargetEaxEcxAbi(const float position[2], void *target)
{
    const u8 *t = static_cast<const u8 *>(target);
    const float dx = LoadFloat(t, 0x3c0U) - position[0];
    const float dy = LoadFloat(t, 0x3c4U) - position[1];
    // Native (0x426671): both components are compared against
    // flt_470b04 (0.0f) and the degenerate case returns flt_470b94
    // (pi/2, 1.5707964f).
    if (dy == 0.0f && dx == 0.0f)
        return 1.5707964f;
    return static_cast<float>(atan2(static_cast<double>(dy),
                                    static_cast<double>(dx)));
}

// TH10 0x00408750.
void SetPolarVelocityThisAbi(float out_xy[2], float angle, float speed)
{
    out_xy[0] = static_cast<float>(cos(static_cast<double>(angle))) * speed;
    out_xy[1] = static_cast<float>(sin(static_cast<double>(angle))) * speed;
}

// TH10 0x00406d90. Native ECX = object (a 0x7f0-byte EffectTriggerRecord
// pool record).
void RunSceneTriggerInstructionQueueEcxAbi(void *object)
{
    u8 *const obj = static_cast<u8 *>(object);
    EffectTriggerRecord &rec = *static_cast<EffectTriggerRecord *>(object);

    i32 index = LoadI32(obj, kOffIndex);
    if (index >= kQueueCapacity)
        return;

    for (;;) {
        const u32 slot = static_cast<u32>(index) * 0x18U;
        SceneTriggerInstruction *cur =
            reinterpret_cast<SceneTriggerInstruction *>(obj + kOffQueue
                                                        + slot);
        SceneTriggerInstruction *next = cur + 1;

        const u32 opcode = cur->opcode;
        if (opcode == 0U)
            return; // 0x406dcb: zero opcode leaves the interpreter for good.

        // An argument-less opcode only runs while no flags were accumulated
        // yet; the check reads the whole dword at +0x43c.
        if (cur->has_args == 0U && LoadU32(obj, kOffFlags) != 0U)
            return;

        if (opcode == 0x80000000U) {
            // Pure skip: advance one slot.
            ++index;
            if (index >= kQueueCapacity)
                return;
            continue;
        }

        const u32 a0 = cur->arg0;
        const u32 a1 = cur->arg1;
        const u32 a2 = cur->arg2;
        const u32 a3 = cur->arg3;
        u32 *flags32 = reinterpret_cast<u32 *>(obj + kOffFlags);

        switch (opcode) {
        case 1U:
            *flags32 |= 1U;
            TickPlayerTimerEaxStackAbi(obj + 0x614U, 0);
            StoreU32(obj, 0x638U, 0U);
            break;

        case 16U: {
            *flags32 |= 0x10U;
            StoreU32(obj, 0x65cU, a0);
            StoreFloat(obj, 0x660U, ResolveAngleArg(
                *reinterpret_cast<const float *>(&a1), rec));
            TickPlayerTimerEaxStackAbi(obj + 0x648U, 0);
            StoreU32(obj, 0x670U, a2);
            SetPolarVelocityThisAbi(
                reinterpret_cast<float *>(obj + 0x664U),
                LoadFloat(obj, 0x660U), LoadFloat(obj, 0x65cU));
            if (index != 0 && LoadI32(obj, kOffState) >= 0)
                ReserveContextChannel(
                    g_SoundGateContext,
                    static_cast<u32>(LoadI32(obj, kOffState)), 0U);
            break;
        }

        case 32U: {
            *flags32 |= 0x20U;
            StoreU32(obj, 0x690U, a0);
            StoreU32(obj, 0x694U, a1);
            TickPlayerTimerEaxStackAbi(obj + 0x67cU, 0);
            StoreU32(obj, 0x6a4U, a2);
            if (index != 0 && LoadI32(obj, kOffState) >= 0)
                ReserveContextChannel(
                    g_SoundGateContext,
                    static_cast<u32>(LoadI32(obj, kOffState)), 0U);
            break;
        }

        case 64U:
        case 128U: {
            *flags32 |= opcode;
            StoreFloat(obj, 0x6c8U, ResolveAngleArg(
                *reinterpret_cast<const float *>(&a0), rec));
            // y falls back to +0x3d8 for anything not strictly above
            // -999.0f (NaN included — the fcomp parity chain sends it to
            // the fallback). Gate constant: flt_470cc8 = -999.0f.
            const float y = *reinterpret_cast<const float *>(&a1);
            StoreFloat(obj, 0x6c4U,
                       (y > kFallbackYLow) ? y
                                           : LoadFloat(obj, kOffFallbackY));
            LazyInitTimerBlock(obj, 0x6b0U);
            StoreU32(obj, 0x6b4U, 0U);
            StoreU32(obj, 0x6b8U, 0U);
            StoreU32(obj, 0x6b0U, static_cast<u32>(-1));
            StoreU32(obj, 0x6d8U, a2);
            StoreU32(obj, 0x6dcU, a3);
            StoreU32(obj, 0x6e0U, 0U);
            break;
        }

        case 0x400U:
        case 0x800U:
        case 0x8000000U:
            // All three share one handler; the raw opcode lands in +0x6f8.
            *flags32 |= opcode;
            StoreU32(obj, 0x6f8U, opcode);
            StoreU32(obj, 0x710U, a2);
            StoreU32(obj, 0x70cU, 0U);
            break;

        case 0x1000U:
            // Native writes through the +4 activation-gate dword verbatim.
            rec.activation_gate_0004 = a2;
            break;

        case 0x2000U:
            StoreU32(obj, kOffField434, a2);
            ++index;
            if (index >= kQueueCapacity)
                return;
            continue;

        case 0x4000U: {
            // VM bind: script = table[a2] + a3 on the record's attached VM.
            const i32 script = static_cast<i32>(g_TriggerVmScriptTable[a2])
                + static_cast<i32>(a3);
            void *vm_manager = static_cast<EffectManagerRoot *>(
                g_SceneCommandManager)->bullet_resource_3e0b50;
            InitializePlayerMainVmEsiStackAbi(&rec.vm, vm_manager, script);
            break;
        }

        case 0x8000U:
            *flags32 |= opcode;
            TickPlayerTimerEaxStackAbi(obj + 0x718U, static_cast<i32>(a2));
            break;

        case 0x10000U:
            ActivateStageEnemyEsiAbi(obj);
            break;

        case 0x20000U:
            // Positional sound keyed on the object's world x.
            EnqueueSoundEffectEbxStackAbi(a2, g_SoundGateContext,
                                          rec.position_x_03b4);
            break;

        case 0x100000U:
            *flags32 |= opcode;
            TickPlayerTimerEaxStackAbi(obj + 0x74cU, static_cast<i32>(a2));
            break;

        case 0x200000U:
            *flags32 |= opcode;
            TickPlayerTimerEaxStackAbi(obj + 0x780U, static_cast<i32>(a2));
            break;

        case 0x400000U: {
            // Spawn packet: header words, the object position, the next and
            // current record heads, then a verbatim 0x1b0-byte copy of the
            // whole 18-entry queue (native rep movsd of 0x6c dwords from
            // +0x464), then the trailing word/dword fields.
            u8 packet[0x21c];
            StoreU32(packet, 0x00U, 0U); // var_210 u16 0 (low half used)
            {
                const u32 byte9 = static_cast<u32>(
                    reinterpret_cast<const u8 *>(cur)[9]);
                const u16 signed_word = static_cast<u16>(
                    static_cast<i32>(static_cast<i32>(byte9 << 24) >> 24));
                *reinterpret_cast<u16 *>(packet + 0x02U) = signed_word;
            }
            // Position dwords are bit-copies in the native; read them
            // through the typed fields.
            StoreU32(packet, 0x04U,
                     *reinterpret_cast<u32 *>(&rec.position_x_03b4));
            StoreU32(packet, 0x08U,
                     *reinterpret_cast<u32 *>(&rec.position_y_03b8));
            StoreU32(packet, 0x0cU,
                     *reinterpret_cast<u32 *>(&rec.position_z_03bc));
            StoreU32(packet, 0x10U, next->arg0);
            StoreU32(packet, 0x14U, next->arg1);
            StoreU32(packet, 0x18U, cur->arg0);
            StoreU32(packet, 0x1cU, cur->arg1);
            for (u32 i = 0; i < 0x6cU; ++i)
                StoreU32(packet, 0x20U + 4U * i,
                         LoadU32(obj, kOffQueue + 4U * i));
            *reinterpret_cast<u16 *>(packet + 0x1f4U) =
                static_cast<u16>(cur->arg3);
            *reinterpret_cast<u16 *>(packet + 0x1f6U) =
                static_cast<u16>(next->arg2);
            *reinterpret_cast<u16 *>(packet + 0x1f8U) = 0U; // var_18
            StoreU32(packet, 0x1fcU, next->arg3);
            StoreU32(packet, 0x204U, static_cast<u32>(-1));
            StoreU32(packet, 0x208U, a2 & 0xffU);

            SpawnSceneTriggerPacketEaxStackAbi(packet, g_SceneCommandManager);

            // The native bumps the cursor while building the packet, again
            // after the spawn, and a third time (through the default tail)
            // when the spawn-and-activate bit is set.
            index += 2;
            if ((a2 & 0x80000000U) != 0U) {
                ++index;
                ActivateStageEnemyEsiAbi(obj);
            }
            if (index >= kQueueCapacity)
                return;
            continue;
        }

        case 0x1000000U:
            StoreU32(obj, kOffExtentY, a2);
            ++index;
            if (index >= kQueueCapacity)
                return;
            continue;

        case 0x2000000U:
            // Unconditional jump: the cursor becomes the argument verbatim
            // (a value past the capacity ends the queue).
            index = static_cast<i32>(a2);
            if (index >= kQueueCapacity)
                return;
            continue;

        case 0x4000000U:
            *flags32 |= 0x4000000U;
            StoreU32(obj, 0x7c8U, a0);
            StoreU32(obj, 0x7ccU, a1);
            LazyInitTimerBlock(obj, 0x7b4U);
            StoreU32(obj, 0x7b8U, 0U);
            StoreU32(obj, 0x7bcU, 0U);
            StoreU32(obj, 0x7b4U, static_cast<u32>(-1));
            StoreU32(obj, 0x7dcU, a2);
            break;

        default:
            break;
        }

        // Default advance (native default tail at 0x40731c).
        ++index;
        if (index >= kQueueCapacity)
            return;
    }
}

// TH10 0x00426cf0. Native stdcall (ret 4).
void FireSceneTriggerExpireEffect(void *object)
{
    u8 *const obj = static_cast<u8 *>(object);
    u32 out_id = 0;

    StoreU32(obj, kOffState, 4U);

    // Spawn position: the screen anchor with the fixed +224/+16 game-area
    // shift folded in before the verbatim position publisher runs.
    float spawn[3];
    spawn[0] = LoadFloat(obj, kOffScreen) + kSpawnOffsetX;
    spawn[1] = LoadFloat(obj, kOffScreen + 4U) + kSpawnOffsetY;
    spawn[2] = LoadFloat(obj, kOffScreen + 8U);

    void *const vm_manager = static_cast<EffectManagerRoot *>(
        g_SceneCommandManager)->bullet_resource_3e0b50;

    // Flash VM: script 0x162.
    u8 *vm = static_cast<u8 *>(AllocatePoolVmEsiAbi(vm_manager));
    StoreU32(vm, 0x20U, 0U);
    StoreU32(vm, 0x35cU, LoadU32(vm, 0x35cU) | 0x40000000U);
    AssignPoolVmScriptEcxEaxAbi(vm, 0x162);
    LinkEntityAndAssignIdEaxEsiAbi(&out_id, vm);
    SetEntityPositionDirectEsiAbi(g_EntityPoolManager, out_id, spawn);

    // Debris VMs: script 0x163, thirty-two of them, same position.
    for (u32 i = 0; i < 32U; ++i) {
        vm = static_cast<u8 *>(AllocatePoolVmEsiAbi(vm_manager));
        StoreU32(vm, 0x20U, 0U);
        StoreU32(vm, 0x35cU, LoadU32(vm, 0x35cU) | 0x40000000U);
        AssignPoolVmScriptEcxEaxAbi(vm, 0x163);
        LinkEntityAndAssignIdEaxEsiAbi(&out_id, vm);
        SetEntityPositionDirectEsiAbi(g_EntityPoolManager, out_id, spawn);
    }

    // Arm the expire timers. The +0x474 block is lazily initialized then
    // unconditionally reset; the +0x430c block lazily initializes with the
    // fixed 5/6/6.0f pattern.
    LazyInitTimerBlock(obj, kOffExpireTimer);
    StoreU32(obj, kOffExpireTimer + 4U, 0U);
    StoreU32(obj, kOffExpireTimer + 8U, 0U);
    StoreU32(obj, kOffExpireTimer, static_cast<u32>(-1));

    // Native 0x426e3d: null-checked dword test of the title screen's
    // +0x58 flags (bit 0x200 sound gate).
    void *const gate_owner = g_SoundGateOwner;
    if (gate_owner != 0) {
        const TitleScreen &ts =
            *reinterpret_cast<const TitleScreen *>(gate_owner);
        if ((ts.flags & 0x200U) == 0U)
            ReserveContextChannel(g_SoundGateContext, 4U, 0U);
    }

    LazyInitTimerBlock(obj, kOffScriptTimer);
    StoreU32(obj, kOffScriptTimer + 4U, 6U);
    StoreFloat(obj, kOffScriptTimer + 8U, 6.0f);
    StoreU32(obj, kOffScriptTimer, 5U);

    // Detach the object's ANM VM (script index 0 wipes it).
    AssignAnmScriptToVmEcxEaxBbxAbi(obj + 0x10U, obj + 0x14U, 0);

    // Past 60 stage ticks, clear the shared stage life/anchor flags: the
    // bit-1 mask over +0x378c and the seven per-life dwords, plus +0x3790.
    u8 *const stage = static_cast<u8 *>(g_StageState);
    if (LoadI32(stage, 0x3738U) >= 60) {
        StoreU32(stage, 0x3790U, 0U);
        StoreU32(stage, 0x378cU, LoadU32(stage, 0x378cU) & 0xfffffffdU);
        StoreU32(stage, 0x0ad4U, LoadU32(stage, 0x0ad4U) & 0xfffffffdU);
        StoreU32(stage, 0x0e80U, LoadU32(stage, 0x0e80U) & 0xfffffffdU);
        StoreU32(stage, 0x15d8U, LoadU32(stage, 0x15d8U) & 0xfffffffdU);
        StoreU32(stage, 0x1984U, LoadU32(stage, 0x1984U) & 0xfffffffdU);
        StoreU32(stage, 0x1d30U, LoadU32(stage, 0x1d30U) & 0xfffffffdU);
        StoreU32(stage, 0x20dcU, LoadU32(stage, 0x20dcU) & 0xfffffffdU);
        StoreU32(stage, 0x2488U, LoadU32(stage, 0x2488U) & 0xfffffffdU);
    }
}

// TH10 0x004267f0. Native EAX = position pair, ECX = object, stack
// (rotation, scale, limit).
i32 CheckSceneTriggerRotatedRegionEaxEcxStackAbi(const float *position,
                                                 void *object, float rotation,
                                                 float scale, float limit)
{
    u8 *const obj = static_cast<u8 *>(object);

    const float dx = LoadFloat(obj, kOffScreen) - position[0];
    const float dy = LoadFloat(obj, kOffScreen + 4U) - position[1];
    const double neg = -static_cast<double>(rotation);
    const float s = static_cast<float>(sin(neg));
    const float c = static_cast<float>(cos(neg));

    const float rx = dx * c - s * dy;
    const float ry = c * dy + s * dx;

    const float extent_x = LoadFloat(obj, kOffExtentX);
    const float extent_y = LoadFloat(obj, kOffExtentY);
    const float left = rx - extent_x;
    const float top = ry - extent_y;
    const float right = rx + extent_x;

    if (!(left <= limit))
        return 0;

    const float bottom = ry + extent_y;
    if (scale * 192.0f < top || right < 30.0f || scale * 57.0f > bottom) {
        // Outside the coarse band: only the deep overlap window answers 2.
        if (left <= limit && right >= 30.0f && scale * 12.0f >= top
            && scale * 144.0f <= top)
            return 2;
        return 0;
    }

    void *const singleton =
        *reinterpret_cast<void **>(&g_SceneSingletons + 44U);
    if (singleton != 0
        && LoadU32(static_cast<u8 *>(singleton), 40632U) != 0U)
        return 0;

    const i32 state = LoadI32(obj, kOffState);
    if (state == 2 || state == 4 || state == 3)
        return 0;
    if (LoadI32(obj, kOffScriptTimer + 4U) > 0)
        return 0;

    FireSceneTriggerExpireEffect(obj);
    return 1;
}

} // namespace th10
