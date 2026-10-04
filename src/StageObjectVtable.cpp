// Stage/background object callback-table family (TH10 0x41c030..0x41f670).
//
// The manager published at DAT_0047781c owns two object flavours:
//   kind A: 0xd58 bytes, callback table 0x46da60,
//   kind B: 0xd74 bytes, callback table 0x46da10,
// plus the all-stub default table 0x46dab0 installed by 0x41c030. Each table
// is 19 function pointers followed by a null terminator in .rdata. The
// per-kind objects share the header layout modeled by StageObjectObject.hpp
// (StageObjectHeader / StageObjectKindADescriptor / StageObjectKindBDescriptor
// / StageObjectKindA / StageObjectKindB) and differ in the descriptor copy
// size (0x1dc vs 0x1f8), the animation VM record bases (+0x600/+0x9ac vs
// +0x61c/+0x9c8) and the ring-effect kind word (descriptor_0424.
// script_kind_0026 at +0x44a vs descriptor_0424.script_kind_0042 at +0x466).
//
// The sweep family (slots 5/6/7) walks the ray angle in 12-degree steps
// (flt_470cd8), stops once the step start passes object+0x40 plus the 6.0f
// window (flt_470d1c), box-tests (slot 6, half-extents = argument * 0.5) or
// radially tests (slot 7, dx^2+dy^2 vs the squared radius argument) each step
// point, marks a 0x40-byte hit bitmap on the stack, then advances the argument
// position across the first hit run and spawns ring effect VMs (script
// kind*2+0x11, 0x448db0 composition) at every gap whose angular width exceeds
// 18 degrees (flt_470c0c). The box twin additionally re-spawns whole stage
// objects from the descriptor copy through 0x41c510. /GS cookie checks
// (0x458ea5) are compiler artifacts and are not modeled.
#include <math.h>

#include "EntityHelpers.hpp"
#include "BgmRuntime.hpp"
#include "ConditionalStateObject.hpp"
#include "PlayerFrameworkHelpers.hpp"
#include "PlayerMotionHelpers.hpp"
#include "PlayerRecord.hpp"
#include "PlayerShotData.hpp"
#include "PlayerTimerHelpers.hpp"
#include "SceneTriggerFeatures.hpp"
#include "SceneTriggerUpdate.hpp"
#include "StageEffectHelpers.hpp"
#include "StageObjectManagerObject.hpp"
#include "StageObjectObject.hpp"
#include "Th10Types.hpp"
#include "TimelineRenderObjectSetup.hpp"
#include "TitleBulletUpdate.hpp"
#include "VmRecord.hpp"
#include "ZunMath.hpp"

#include <cmath>
#include "StageObjectVtable.hpp"

namespace th10 {

namespace {

// ---- globals (shared with the existing modules) --------------------------

extern void *g_MainChainRenderOwner;  // TH10 DAT_00491c10 (VM pool owner)
extern void *g_BulletManagerSlot;     // TH10 DAT_00477818
extern void *g_BulletListRootSlot;    // TH10 DAT_0047781c (this manager)
extern void *g_EffectManagerRoot;     // TH10 DAT_004776f0 (+0x3e0b50 pool)
extern void *g_OptionPositionBase;    // TH10 DAT_00477834 (player position
                                      // manager: pos at +0x3c0, region box
                                      // at +0x41c..+0x424)
extern void *g_AsciiHudConditionalState; // TH10 DAT_00477704
extern float g_FrameTimeScale;        // TH10 flt_00476f78
extern u32 *g_TriggerVmScriptTable;   // TH10 DAT_00474170
extern u8 g_EffectKindLookupTable;    // TH10 byte_004749ce (word at kind*8)
extern TransitionRootPartial g_TransitionRoot; // TH10 DAT_00492590

inline u8 *EffectManager()
{
    return reinterpret_cast<u8 *>(&g_TransitionRoot);
}

// ---- float constants (bit patterns read from .rdata) ---------------------

inline float FloatFromBits(u32 bits)
{
    union {
        u32 u;
        float f;
    } converter;
    converter.u = bits;
    return converter.f;
}

const float kHalf = FloatFromBits(0x3F000000U);      // 0x470b0c  0.5f
const float kPi = FloatFromBits(0x40490FDBU);        // 0x470b18  pi
const float kZero = FloatFromBits(0U);               // 0x470b04  0.0f
const float kOne = FloatFromBits(0x3F800000U);       // 0x470afc  1.0f
const float kFieldMinX = FloatFromBits(0xC3400000U); // 0x470b40 -192.0f
const float kFieldMaxX = FloatFromBits(0x43400000U); // 0x470b3c  192.0f
const float kFieldMaxY = FloatFromBits(0x43E00000U); // 0x470b38  448.0f
const float kTipGateZ = FloatFromBits(0x41800000U);  // 0x470b48  16.0f
const float kTipGateY = FloatFromBits(0x40400000U);  // 0x470bd4  3.0f
const float kTipAlpha = FloatFromBits(0x42000000U);  // 0x470bcc  32.0f
const float kTipScaleZ = FloatFromBits(0x3DCCCCCDU); // 0x470c18  0.1f
const float kTipScaleB = FloatFromBits(0x3F4CCCCDU); // 0x470cc0  0.8f
const float kTipScaleC = FloatFromBits(0x3EAAAAABU); // 0x470cc4  1/3
const float kDepthLatch = FloatFromBits(0x41900000U);// 0x470c0c  18.0f
const float kAngleStep = FloatFromBits(0x41400000U); // 0x470cd8  12.0f
const float kDepthWindow = FloatFromBits(0x40C00000U); // 0x470d1c 6.0f
const float kSweepStep = FloatFromBits(0x40C00000U); // 6.0f sweep length
const float kSpawnZ = FloatFromBits(0x43600000U);    // 0x470b4c  224.0f
const float kExplosionAngle = FloatFromBits(0xBFC90FDBU); // -pi
const float kExplosionSpeed = FloatFromBits(0x3F19999AU); // 0.6f
const float kRateLow = FloatFromBits(0x3F7D70A4U);   // 0x470b68  0.99f
const float kRateHigh = FloatFromBits(0x3F8147AEU);  // 0x470b64  1.01f

// ---- field access helpers ------------------------------------------------

u32 LoadU32At(const void *base, u32 offset)
{
    return *reinterpret_cast<const u32 *>(
        static_cast<const u8 *>(base) + offset);
}

void StoreU32At(void *base, u32 offset, u32 value)
{
    *reinterpret_cast<u32 *>(static_cast<u8 *>(base) + offset) = value;
}

i32 LoadI32At(const void *base, u32 offset)
{
    return static_cast<i32>(LoadU32At(base, offset));
}

void StoreI32At(void *base, u32 offset, i32 value)
{
    StoreU32At(base, offset, static_cast<u32>(value));
}

float LoadFloatAt(const void *base, u32 offset)
{
    return *reinterpret_cast<const float *>(
        static_cast<const u8 *>(base) + offset);
}

void StoreFloatAt(void *base, u32 offset, float value)
{
    *reinterpret_cast<float *>(static_cast<u8 *>(base) + offset) = value;
}

u16 LoadU16At(const void *base, u32 offset)
{
    return *reinterpret_cast<const u16 *>(
        static_cast<const u8 *>(base) + offset);
}

void StoreU16At(void *base, u32 offset, u16 value)
{
    *reinterpret_cast<u16 *>(static_cast<u8 *>(base) + offset) = value;
}

void CopyBlock(void *dst, const void *src, u32 bytes)
{
    u8 *d = static_cast<u8 *>(dst);
    const u8 *s = static_cast<const u8 *>(src);
    for (u32 i = 0; i < bytes; ++i)
        d[i] = s[i];
}

void ZeroBlock(void *dst, u32 bytes)
{
    u8 *d = static_cast<u8 *>(dst);
    for (u32 i = 0; i < bytes; ++i)
        d[i] = 0;
}

// ---- per-kind constants --------------------------------------------------

struct SweepKindMap {
    u32 kind_word;      // A: descriptor_0424.script_kind_0026 (+0x44a) /
                        //   B: descriptor_0424.script_kind_0042 (+0x466)
    u32 descriptor_size;
};

const SweepKindMap kMapA = { 0x44aU, 0x1dcU };
const SweepKindMap kMapB = { 0x466U, 0x1f8U };

// ---- boundaries ----------------------------------------------------------

// TH10 0x004267f0. EAX = position {x,y,z}, ECX = player position manager,
// stack = (angle, inner, radius) ret 0xc. Rotates the player-relative offset
// by `angle` and classifies it against the region box at manager+0x41c:
// 0 outside, 1 inner region, 2 outer region. Boundary - the body is a
// rotated-region sibling of the reconstructed 0x4266b0.
i32 TH10_STDCALL ClassifyPositionInPlayerRegionEaxEcxStackAbi(
    const float position[3], void *player_manager, float angle, float inner,
    float radius)
{
    (void)position;
    (void)player_manager;
    (void)angle;
    (void)inner;
    (void)radius;
    return 0;
}

// TH10 0x0043e710/0x448db0 composition used by the sweep/spread spawn path:
// pool VM alloc (manager DAT_00491c10), +0x35c |= 0x40000000, +0x20 = 0,
// position at +0x340..+0x348 with the +224/+16 offsets, ANM script bind from
// the stage object manager's bullet.anm work handle [DAT_0047781c+0x458]
// (native loads dword_47781C+0x458 at 0x41dabd / 0x41e305 / 0x41ed3e;
// DAT_00477818 has no handle at +0x458 - that offset lands inside
// BulletSlot[1]'s VM record - so this read targets the stage object manager)
// and list-A registration (id counter +0x732454).
void SpawnRingEffectVm(i32 script_index, const float position[3], float z)
{
    u8 *rec = static_cast<u8 *>(AllocatePoolVmEsiAbi(g_MainChainRenderOwner));
    VmRecord &vm = *reinterpret_cast<VmRecord *>(rec);
    vm.flags |= 0x40000000U;
    vm.render_kind = 0U;
    vm.delta_pos_x = position[0] + kSpawnZ;
    vm.delta_pos_y = position[1] + kTipGateZ;
    vm.delta_pos_z = z;
    StageObjectManager &stage_mgr =
        *reinterpret_cast<StageObjectManager *>(g_BulletListRootSlot);
    void *anm_work = stage_mgr.bullet_anm_work_0458;
    AssignAnmScriptToVmEcxEaxBbxAbi(anm_work, rec, script_index);
    u32 out_id = 0;
    LinkEntityAndAssignIdEaxEsiAbi(&out_id, rec);
}

// The effect-ring cutoff (0x43dc90 with value 0) used by the death paths.
void CutoffEffectRing(i32 kind)
{
    if (kind >= 0)
        QueueEffectRingValueEcxStackAbi(kind, 0);
}

} // namespace

// ---- shared no-op slot bodies --------------------------------------------

// TH10 0x41bf10 / 0x41bf20.
i32 StageObjectSlotRet0(void *object)
{
    (void)object;
    return 0;
}

// TH10 0x41bf90..0x41c020 (plain `ret` members).
void StageObjectSlotNoop(void *object)
{
    (void)object;
}

// TH10 0x41bf30. Unlinks the embedded node at object+4/+8 from its list.
void StageObjectSlotUnlinkThiscall(void *object)
{
    StageObjectHeader &node = *reinterpret_cast<StageObjectHeader *>(object);
    void *const next = node.list_next_0008;
    static_cast<StageObjectHeader *>(node.list_prev_0004)->list_next_0008 =
        next;
    if (next != 0)
        static_cast<StageObjectHeader *>(next)->list_prev_0004 =
            node.list_prev_0004;
}

// TH10 0x0041f7a0.
i32 TH10_STDCALL IsBoxOutsidePlayfieldEcxStackAbi(const float center[2],
                                                  float dx, float dy)
{
    if (center[0] + dx < kFieldMinX)
        return 1;
    if (center[0] - dx >= kFieldMaxX)
        return 1;
    if (center[1] + dy < kZero)
        return 1;
    if (center[1] - dy >= kFieldMaxY)
        return 1;
    return 0;
}

// TH10 0x0043dc90.
void TH10_STDCALL QueueEffectRingValueEcxStackAbi(i32 kind, i32 value)
{
    u8 *mgr = EffectManager();
    const i32 lookup = static_cast<i16>(LoadU16At(
        &g_EffectKindLookupTable, 8U * static_cast<u32>(kind)));

    i32 slot = 0;
    for (;;) {
        const i32 current = LoadI32At(mgr, 0x620U + 4U * slot);
        if (current < 0) {
            StoreI32At(mgr, 0x620U + 4U * slot, kind);
            // Native 0x43dcf0 stores to mgr+0x408+4*kind with NO bounds
            // clamp on `kind` (only the slot loop's <12 and the count's
            // <128 are checked); the bgm_sound_default_frequencies[128]
            // table at +0x408 is written past its end for kind >= 128.
            // Preserved verbatim as a native quirk.
            StoreI32At(mgr, 0x408U + 4U * static_cast<u32>(kind), lookup);
            StoreI32At(mgr, 0x680U + 4U * (slot << 7), value);
            break;
        }
        if (current == kind) {
            const i32 count = LoadI32At(mgr, 0x650U + 4U * slot);
            if (count < 0x80)
                StoreI32At(mgr, 0x680U + 4U * (count + (slot << 7)), value);
            break;
        }
        if (++slot >= 12)
            return;
    }
    StoreI32At(mgr, 0x650U + 4U * slot,
               LoadI32At(mgr, 0x650U + 4U * slot) + 1);
}

// TH10 0x0041c030. EDX = block. The stores before the memset are dead in the
// binary as well (the 0x424-byte rep stos erases them); they are kept in the
// native order.
void *InitStageObjectHeaderDefaultsEdxAbi(void *block)
{
    StageObjectHeader &hdr = *reinterpret_cast<StageObjectHeader *>(block);
    u8 *obj = static_cast<u8 *>(block); // raw_0068 / wipe base
    hdr.callback_table_0000 =
        reinterpret_cast<void *>(0x0046DAB0U); // dead: erased below
    hdr.timer_0010.flags &= 0xFFFFFFFEU;
    for (i32 i = 0; i < 0x12; ++i) {
        // Eighteen 0x34-stride feature records inside header.raw_0068
        // (0x68..0x404); flag dword at each record start.
        const u32 off = 0x68U + 0x34U * static_cast<u32>(i);
        StoreU32At(obj, off, LoadU32At(obj, off) & 0xFFFFFFFEU);
    }
    hdr.entrance_timer_0410.flags &= 0xFFFFFFFEU;
    ZeroBlock(obj, 0x424U);

    u32 flags = hdr.timer_0010.flags; // 0 after the wipe
    if ((flags & 1U) == 0U) {
        flags |= 1U;
        hdr.timer_0010.count = 0;
        *reinterpret_cast<u32 *>(&hdr.timer_0010.prev) = 0xFFF0BDC1U;
        hdr.timer_0010.accum = 0;
        hdr.timer_0010.rate = &g_FrameTimeScale;
        hdr.timer_0010.flags = flags;
    }
    hdr.timer_0010.count = 0;
    hdr.timer_0010.accum = 0;
    hdr.timer_0010.prev = -1;
    return obj;
}

// TH10 0x0041c100.
void *CreateStageObjectManagerInPlaceEsiAbi(void *manager)
{
    StageObjectManager &mgr = *reinterpret_cast<StageObjectManager *>(manager);
    InitStageObjectHeaderDefaultsEdxAbi(
        &mgr.list_sentinel_0010); // dead: erased below
    ZeroBlock(&mgr, 0x45cU);
    g_BulletListRootSlot = manager;
    return manager;
}

namespace {

// Dead-flag clear + wipe + trailing 0xffff word, per 0x3ac-byte VM record.
// The native clears bit 0 of nine dwords first and then wipes the record, so
// every stored value except the trailing word is erased; the order is kept.
void ResetVmRecord(u8 *record)
{
    VmRecord &vm = *reinterpret_cast<VmRecord *>(record);
    u32 *const flag_slots[9] = {
        &vm.timer_flags,         // 0x06c
        &vm.position_anim.flags, // 0x0b0
        &vm.rgb_anim_1.flags,    // 0x0fc
        &vm.alpha_anim_1.flags,  // 0x128
        &vm.rotation_anim.flags, // 0x174
        &vm.scale_anim.flags,    // 0x1b0
        &vm.rgb_anim_2.flags,    // 0x1fc
        &vm.alpha_anim_2.flags,  // 0x228
        &vm.saved_timer_flags};  // 0x378
    for (u32 i = 0; i < 9U; ++i) {
        *flag_slots[i] &= 0xFFFFFFFEU;
    }
    ZeroBlock(record, 0x3acU);
    vm.sprite_entry_id = 0xFFFFU;
}

} // namespace

// TH10 0x0041c5b0.
void *InitStageObjectKindA_EbxAbi(void *object)
{
    StageObjectKindA &so = *reinterpret_cast<StageObjectKindA *>(object);
    InitStageObjectHeaderDefaultsEdxAbi(&so.header);
    so.header.callback_table_0000 = reinterpret_cast<void *>(0x0046DA60U);

    ZeroBlock(&so.descriptor_0424, 0x1dcU); // region A (0x424..0x600)
    ResetVmRecord(reinterpret_cast<u8 *>(&so.vm1_0600));
    // Native reads vm2+0x6c (obj+0xa18) before the second wipe - dead.
    (void)so.vm2_09ac.timer_flags;
    ResetVmRecord(reinterpret_cast<u8 *>(&so.vm2_09ac));
    return object;
}

// TH10 0x0041c680.
void *InitStageObjectKindB_EbxAbi(void *object)
{
    StageObjectKindB &so = *reinterpret_cast<StageObjectKindB *>(object);
    InitStageObjectHeaderDefaultsEdxAbi(&so.header);
    so.header.callback_table_0000 = reinterpret_cast<void *>(0x0046DA10U);

    ZeroBlock(&so.descriptor_0424, 0x1f8U);
    // 8.0f seeded at +0x450 (descriptor_0424.zspeed_002c).
    so.descriptor_0424.zspeed_002c = FloatFromBits(0x41000000U);
    (void)so.vm1_061c.timer_flags; // dead read of the wiped flag word
                                   // (obj+0x688)
    ResetVmRecord(reinterpret_cast<u8 *>(&so.vm1_061c));
    (void)so.vm2_09c8.timer_flags; // obj+0xa34
    ResetVmRecord(reinterpret_cast<u8 *>(&so.vm2_09c8));
    return object;
}

// TH10 0x0041c510. ESI = manager, EDI forwarded, stack = kind.
void *SpawnStageObjectEsiEdiStackAbi(void *manager, void *forwarded_edi,
                                     i32 kind)
{
    StageObjectManager &mgr = *reinterpret_cast<StageObjectManager *>(manager);
    if (static_cast<i32>(mgr.node_count_0438) >= 0x100)
        return 0;

    u32 cursor = mgr.spawn_id_cursor_043c + 1U;
    if (cursor == 0U)
        cursor = 1U;
    mgr.spawn_id_cursor_043c = cursor;

    void *object = 0;
    if (kind == 0) {
        object = ::operator new(0xd58U);
        if (object != 0)
            InitStageObjectKindA_EbxAbi(object);
    } else if (kind == 1) {
        object = ::operator new(0xd74U);
        if (object != 0)
            InitStageObjectKindB_EbxAbi(object);
    } else {
        // Other kinds reach the int3 padding in the binary; modeled as null.
        return 0;
    }

    // Native dereferences the object even when allocation failed (+0x54):
    // the node reference is formed from the possibly-null pointer exactly as
    // the native does, so a failed allocation still faults on the store.
    StageObjectHeader &node = *reinterpret_cast<StageObjectHeader *>(object);
    node.spawn_id_0054 = cursor;
    node.list_prev_0004 = mgr.list_head_0434;
    reinterpret_cast<StageObjectHeader *>(mgr.list_head_0434)
        ->list_next_0008 = &node;
    mgr.node_count_0438 = mgr.node_count_0438 + 1U;
    mgr.list_head_0434 = &node;

    // Slot-1 dispatch through the installed table; the native forwards the
    // caller's EDI verbatim as the descriptor argument.
    typedef i32 (TH10_STDCALL *InitFn)(void *, const void *);
    const InitFn init = *reinterpret_cast<const InitFn *>(
        static_cast<u8 *>(node.callback_table_0000) + 4U);
    init(&node, forwarded_edi);

    return object;
}

// TH10 0x0041c8c0.
i32 TH10_STDCALL StageObjectSpawnDescriptorA(void *object,
                                             const void *descriptor)
{
    StageObjectKindA &so = *reinterpret_cast<StageObjectKindA *>(object);
    StageObjectHeader &hdr = so.header;
    StageObjectKindADescriptor &desc = so.descriptor_0424;
    CopyBlock(&desc, descriptor, 0x1dcU);

    hdr.state_000c = 2;

    // The bullet.anm work handle lives in the stage object manager
    // (DAT_0047781c; native loads [dword_47781C+0x458] at 0x41c8fd).
    StageObjectManager &stage_mgr =
        *reinterpret_cast<StageObjectManager *>(g_BulletListRootSlot);
    void *anm_work = stage_mgr.bullet_anm_work_0458;

    // VM 1: script = DAT_00474170[(i16)+0x448] + (i16)+0x44a.
    {
        u8 *rec = reinterpret_cast<u8 *>(&so.vm1_0600);
        VmRecord &rec_vm = so.vm1_0600;
        const i16 slot = static_cast<i16>(desc.script_slot_0024);
        const i16 kind = static_cast<i16>(desc.script_kind_0026);
        const i32 script = static_cast<i32>(
            LoadU32At(g_TriggerVmScriptTable, 4U * static_cast<u32>(slot)))
            + kind;
        InitializePlayerMainVmEsiStackAbi(rec, anm_work, script);
        FinalizeTimelineRenderObjectSetup(rec);
        rec_vm.state_word = 2; // rec1+0x304 mode word
        if ((desc.flags_0028 & 1U) != 0U)
            rec_vm.flags = (rec_vm.flags & 0xFFFFFFDFU) | 0x10U;
        rec_vm.flags = (rec_vm.flags & 0xFC63FFFFU) | 0x600000U;
    }

    // VM 2: script = (i16)+0x44a + 0x103.
    {
        u8 *rec = reinterpret_cast<u8 *>(&so.vm2_09ac);
        VmRecord &rec_vm = so.vm2_09ac;
        const i16 kind = static_cast<i16>(desc.script_kind_0026);
        InitializePlayerMainVmEsiStackAbi(rec, anm_work, kind + 0x103);
        FinalizeTimelineRenderObjectSetup(rec);
        rec_vm.state_word = 2; // rec2+0x304 mode word
        rec_vm.flags = (rec_vm.flags & 0xFFFFFFDFU) | 0x10U;
        rec_vm.flags = (rec_vm.flags & 0xFC7FFFFFU) | 0x400000U;
    }

    // Lazy timer arm (+0x410 record), then the spawn defaults.
    if ((hdr.entrance_timer_0410.flags & 1U) == 0U) {
        hdr.entrance_timer_0410.count = 0;
        *reinterpret_cast<u32 *>(&hdr.entrance_timer_0410.prev) =
            0xFFF0BDC1U;
        hdr.entrance_timer_0410.accum = 0;
        hdr.entrance_timer_0410.rate = &g_FrameTimeScale;
        hdr.entrance_timer_0410.flags |= 1U;
    }
    hdr.entrance_timer_0410.count = 0x1e;
    *reinterpret_cast<u32 *>(&hdr.entrance_timer_0410.accum) =
        0x41F00000U; // 30.0f
    hdr.entrance_timer_0410.prev = 0x1d;

    const float depth = desc.depth_0014;
    hdr.depth_0040 = depth;
    hdr.position_x_0024 = desc.spawn_x_0000;
    hdr.position_y_0028 = desc.spawn_y_0004;
    hdr.position_z_002c = desc.spawn_z_0008;
    hdr.zspeed_0048 = desc.zspeed_0020;
    hdr.angle_003c = desc.angle_000c;
    hdr.alpha_0044 = desc.alpha_001c;
    hdr.cutoff_kind_040c = 0x18U;
    hdr.zvel_004c = depth > kZero ? FloatFromBits(0x3C23D70AU) : kZero;

    Float2 velocity;
    SetVectorFromAngle(&velocity, desc.angle_000c, desc.zspeed_0020);
    hdr.velocity_x_0030 = velocity.x;
    hdr.velocity_y_0034 = velocity.y;
    hdr.velocity_z_0038 = 0.0f;
    return 0;
}

// TH10 0x0041e5c0.
i32 TH10_STDCALL StageObjectSpawnDescriptorB(void *object,
                                             const void *descriptor)
{
    StageObjectKindB &so = *reinterpret_cast<StageObjectKindB *>(object);
    StageObjectHeader &hdr = so.header;
    StageObjectKindBDescriptor &desc = so.descriptor_0424;
    CopyBlock(&desc, descriptor, 0x1f8U);

    hdr.state_000c = 3;

    const i16 slot = static_cast<i16>(desc.script_slot_0040);
    const i16 kind = static_cast<i16>(desc.script_kind_0042);
    // The bullet.anm work handle lives in the stage object manager
    // (DAT_0047781c; native loads [dword_47781C+0x458] at 0x41e5fb).
    StageObjectManager &stage_mgr =
        *reinterpret_cast<StageObjectManager *>(g_BulletListRootSlot);
    void *anm_work = stage_mgr.bullet_anm_work_0458;

    {
        u8 *rec = reinterpret_cast<u8 *>(&so.vm1_061c);
        VmRecord &rec_vm = so.vm1_061c;
        const i32 script = static_cast<i32>(
            LoadU32At(g_TriggerVmScriptTable, 4U * static_cast<u32>(slot)))
            + kind;
        InitializePlayerMainVmEsiStackAbi(rec, anm_work, script);
        FinalizeTimelineRenderObjectSetup(rec);
        rec_vm.state_word = 2; // rec1+0x304
        if ((desc.flags_0044 & 2U) != 0U)
            rec_vm.flags = (rec_vm.flags & 0xFFFFFFDFU) | 0x10U;
        rec_vm.flags = (rec_vm.flags & 0xFC63FFFFU) | 0x600000U;
    }

    {
        u8 *rec = reinterpret_cast<u8 *>(&so.vm2_09c8);
        VmRecord &rec_vm = so.vm2_09c8;
        InitializePlayerMainVmEsiStackAbi(rec, anm_work, kind + 0x103);
        FinalizeTimelineRenderObjectSetup(rec);
        rec_vm.state_word = 2; // rec2+0x304
        rec_vm.flags = (rec_vm.flags & 0xFFFFFFDFU) | 0x10U;
        rec_vm.flags = (rec_vm.flags & 0xFC7FFFFFU) | 0x400000U;
    }

    hdr.position_x_0024 = LoadFloatAt(desc.raw_0000, 0x00U);
    hdr.position_y_0028 = LoadFloatAt(desc.raw_0000, 0x04U);
    hdr.position_z_002c = LoadFloatAt(desc.raw_0000, 0x08U);
    hdr.angle_003c = desc.angle_0018;
    hdr.depth_0040 = desc.depth_0024;
    hdr.cutoff_kind_040c = 0x18U;
    hdr.alpha_0044 = 2.0f; // 0x40000000
    hdr.zspeed_0048 = desc.zspeed_002c;
    return 0;
}

// TH10 0x0041d3d0.
i32 TH10_STDCALL StageObjectUpdateA(void *object)
{
    StageObjectKindA &so = *reinterpret_cast<StageObjectKindA *>(object);
    StageObjectHeader &hdr = so.header;
    u8 *const bytes = reinterpret_cast<u8 *>(&so); // raw_0068 / descriptor
                                                   // raw-tail base

    // Slot 0 dispatch through the installed table (0x41ca80 for kind A).
    {
        typedef void (*SlotFn)(void *);
        const SlotFn slot0 =
            *reinterpret_cast<SlotFn *>(hdr.callback_table_0000);
        slot0(bytes);
    }

    const u32 flags0 = hdr.feature_flags_0404;
    if (flags0 != 0U) {
        // Ten-way feature dispatch, native order (bit 0x80 arrives through
        // the `test al,al; jns` idiom).
        typedef void (*SlotFn)(void *);
        const SlotFn *const slots =
            reinterpret_cast<const SlotFn *>(hdr.callback_table_0000);
        if ((flags0 & 1U) != 0U)
            slots[9U](bytes);
        if ((flags0 & 0x10U) != 0U)
            slots[10U](bytes);
        if ((flags0 & 0x20U) != 0U)
            slots[11U](bytes);
        if ((flags0 & 0x40U) != 0U)
            slots[12U](bytes);
        if ((flags0 & 0x100U) != 0U)
            slots[13U](bytes);
        if (static_cast<i32>(flags0) < 0) // test al,al; jns: bit 0x80
            slots[14U](bytes);
        if ((flags0 & 0x8000C00U) != 0U)
            slots[15U](bytes);
        if ((flags0 & 0x100000U) != 0U)
            slots[16U](bytes);
        if ((flags0 & 0x200000U) != 0U)
            slots[17U](bytes);
        if ((flags0 & 0x4000000U) != 0U)
            slots[18U](bytes);

        // Bit 0x8000: shift the +0x15c timer by -1 while +0x160 > 0 (both
        // offsets live inside header.raw_0068).
        u32 flags = hdr.feature_flags_0404;
        if ((flags & 0x8000U) != 0U) {
            if (LoadI32At(bytes, 0x160U) > 0) {
                ShiftTimerByEsiStackAbi(bytes + 0x15cU, -1.0f);
            } else {
                hdr.feature_flags_0404 = flags ^ 0x8000U;
            }
        }
    }

    // Depth/ground handling. zlim at +0x434, ceiling at +0x43c (both inside
    // the kind A descriptor image; its raw_0010/raw_0018 tail is unmodeled).
    {
        const float z = hdr.depth_0040;
        const float zlim = LoadFloatAt(bytes, 0x434U);
        const float dz = g_FrameTimeScale * hdr.zspeed_0048;
        if (!(z < zlim)) {
            // z >= zlim: accumulate the depth velocity and drift.
            hdr.zvel_004c = hdr.zvel_004c + dz;
            hdr.position_x_0024 = hdr.position_x_0024
                + g_FrameTimeScale * hdr.velocity_x_0030;
            hdr.position_y_0028 = hdr.position_y_0028
                + g_FrameTimeScale * hdr.velocity_y_0034;
            hdr.position_z_002c = hdr.position_z_002c
                + g_FrameTimeScale * hdr.velocity_z_0038;
            const float zmax = LoadFloatAt(bytes, 0x43cU);
            if (zmax > kZero) {
                const float candidate = hdr.zvel_004c + hdr.depth_0040;
                if (candidate < zmax) {
                    const float reflected = zmax - hdr.zvel_004c;
                    hdr.depth_0040 = reflected;
                    StoreFloatAt(bytes, 0x434U, reflected);
                    if (!(reflected > kZero))
                        return 1; // fell below zero: release
                }
            }
        } else {
            const float advanced = z + dz;
            hdr.depth_0040 = advanced;
            if (advanced > zlim)
                hdr.depth_0040 = zlim;
        }
    }

    // Timer-gated angle motion and the tip emission. The native carries the
    // first tip's z component (seeded from a stale stack slot) into the
    // second tip block, so it is tracked across the two blocks here.
    float first_tip_z = 0.0f;
    if (hdr.entrance_timer_0410.count > 0) {
        ShiftTimerByEsiStackAbi(
            reinterpret_cast<u8 *>(&hdr.entrance_timer_0410), -1.0f);
    } else {
        Float2 vec;
        SetVectorFromAngle(&vec, hdr.angle_003c, hdr.depth_0040);
        float tip[3];
        tip[0] = vec.x + hdr.position_x_0024;
        tip[1] = vec.y + hdr.position_y_0028;
        // Native seeds tip.z from an uninitialized stack slot; a fresh stack
        // slot contributes zero.
        tip[2] = first_tip_z + hdr.position_z_002c;
        first_tip_z = tip[2];

        const float extent = hdr.alpha_0044;
        // Native 0x41d5d8 first tests the object's current position
        // (ECX = obj+0x24, loaded at 0x41d5b7) and only when that reports
        // outside re-tests the tip (ECX = the stack tip at 0x41d5e5). Both
        // calls pass the +0x44 alpha as both extents; deleting requires
        // both probes to be outside.
        if (IsBoxOutsidePlayfieldEcxStackAbi(
                reinterpret_cast<const float *>(&hdr.position_x_0024),
                extent, extent) != 0
            && IsBoxOutsidePlayfieldEcxStackAbi(tip, extent, extent) != 0)
            return 1;
    }

    // Tip 2: the region probe against the player-region classifier.
    {
        const float z = hdr.depth_0040;
        const float y = hdr.alpha_0044;
        if (z > kTipGateZ && y > kTipGateY) {
            Float2 vec;
            SetVectorFromAngle(&vec, hdr.angle_003c, z * kTipScaleZ);
            float tip2[3];
            tip2[0] = vec.x + hdr.position_x_0024;
            tip2[1] = vec.y + hdr.position_y_0028;
            // Native accumulates the z component over the first tip block.
            tip2[2] = first_tip_z + hdr.position_z_002c;

            const float half_y = y < kTipAlpha
                ? y * kHalf
                : y - (y + kTipGateZ) * kHalf;
            const i32 region = ClassifyPositionInPlayerRegionEaxEcxStackAbi(
                tip2, g_OptionPositionBase, hdr.angle_003c,
                half_y, z * kTipScaleB);
            if (region == 1) {
                typedef i32 (TH10_STDCALL *SweepFn)(void *, const float *,
                                                    const float *, i32);
                const SweepFn sweep =
                    *reinterpret_cast<const SweepFn *>(
                        static_cast<u8 *>(hdr.callback_table_0000)
                            + 6U * 4U);
                PlayerRecord &player =
                    *reinterpret_cast<PlayerRecord *>(
                        g_OptionPositionBase);
                const float *target = &player.position_x;
                sweep(bytes, target, tip2, 0);
            } else if (region == 2) {
                if (hdr.timer_0010.count % 5 == 0) {
                    u8 *stage = static_cast<u8 *>(g_EffectManagerRoot);
                    SpawnStageEffectEdxEbxAbi(
                        reinterpret_cast<void *>(LoadU32At(stage,
                                                           0x3e0b50U)),
                        tip2, 0x1b2);
                    QueueBulletDeathEffectEbxEsiStackAbi(
                        0x1c, EffectManager(), hdr.position_x_0024);
                }
            }
        }
    }

    // VM position publication and ticks.
    {
        u8 *rec1 = reinterpret_cast<u8 *>(&so.vm1_0600);
        VmRecord &rec_vm = so.vm1_0600;
        const void *scale_ptr = rec_vm.anim_entry;
        rec_vm.flags |= 8U;
        rec_vm.scale_x =
            hdr.alpha_0044 / LoadFloatAt(scale_ptr, 0x34U);
        rec_vm.scale_y =
            hdr.depth_0040 / LoadFloatAt(scale_ptr, 0x30U);
        FinalizeTimelineRenderObjectSetup(rec1);
        if (hdr.zvel_004c == kZero)
            FinalizeTimelineRenderObjectSetup(
                reinterpret_cast<u8 *>(&so.vm2_09ac));
    }
    return 0;
}

// TH10 0x0041cfd0 (slot 15; the 0x8000c00 feature flag).
//
// When the object leaves the playfield (x outside [-192,192), y outside
// [0,448)) the cutoff ring entry is queued and the motion snapshot is copied
// into the descriptor header, then a fresh kind A object is spawned through
// 0x41c510 with the forwarded descriptor pointer. The second (flag-gated)
// branch repeats the snapshot with the recalculated angle and clears flag
// bits 0x8000000|0xc00 at the end.
void TH10_STDCALL StageObjectCutoffRespawnA(void *object)
{
    StageObjectKindA &so = *reinterpret_cast<StageObjectKindA *>(object);
    StageObjectHeader &hdr = so.header;
    StageObjectKindADescriptor &desc = so.descriptor_0424;
    u8 *const bytes = reinterpret_cast<u8 *>(&so);

    const float x = hdr.position_x_0024 + hdr.velocity_x_0030;
    const float y = hdr.position_y_0028 + hdr.velocity_y_0034;
    const float z = hdr.position_z_002c + hdr.velocity_z_0038;

    const bool inside = x >= kFieldMinX && x < kFieldMaxX && y >= kZero
        && y < kFieldMaxY;
    if (inside)
        return;

    CutoffEffectRing(static_cast<i32>(hdr.cutoff_kind_040c));

    // Snapshot branch 1: the angle reversed by pi.
    desc.angle_000c = WrapAngleToPi(-hdr.angle_003c - kPi);
    desc.spawn_x_0000 = x;
    desc.spawn_y_0004 = y;
    desc.spawn_z_0008 = z;
    *reinterpret_cast<u32 *>(&desc.zspeed_0020) = LoadU32At(bytes, 0x13cU);
    SpawnStageObjectEsiEdiStackAbi(g_BulletListRootSlot, &desc, 0);

    // Snapshot branch 2 (only when flag 0x8000000 is clear): the depth
    // window check against the +0x40/+0x4c pair decides whether the second
    // spawn happens; the flag mask is always applied on this path.
    if ((hdr.feature_flags_0404 & 0x8000000U) == 0U) {
        const bool depth_inside = hdr.position_z_002c >= kZero;
        (void)depth_inside;
        CutoffEffectRing(static_cast<i32>(hdr.cutoff_kind_040c));
        desc.spawn_x_0000 = x;
        desc.spawn_y_0004 = y;
        desc.spawn_z_0008 = z;
        *reinterpret_cast<u32 *>(&desc.zspeed_0020) = LoadU32At(bytes, 0x13cU);
        SpawnStageObjectEsiEdiStackAbi(g_BulletListRootSlot, &desc, 0);
    }
    hdr.feature_flags_0404 &= 0xF7FFF3FFU;
}

// TH10 0x0041d170 (slot 12; the 0x40 feature flag). Distance-driven shrink:
// while the +0xf8 timer has not reached +0x11c the motion vector length
// shrinks as base*(1 - rate/+0x11c); on reach the terminal event fires, the
// timer re-arms and the vector rebuilds with the full +0x11c length.
// All of the shrink state (+0xf4..+0x124) lives inside header.raw_0068.
void TH10_STDCALL StageObjectDistanceShrinkA(void *object)
{
    StageObjectKindA &so = *reinterpret_cast<StageObjectKindA *>(object);
    StageObjectHeader &hdr = so.header;
    u8 *const bytes = reinterpret_cast<u8 *>(&so);
    const i32 timer = LoadI32At(bytes, 0xf8U);
    const i32 max_distance = LoadI32At(bytes, 0x11cU);

    if (timer >= max_distance) {
        CutoffEffectRing(static_cast<i32>(hdr.cutoff_kind_040c));
        const i32 counter = LoadI32At(bytes, 0x124U) + 1;
        StoreI32At(bytes, 0x124U, counter);
        if (counter >= LoadI32At(bytes, 0x120U))
            hdr.feature_flags_0404 &= 0xFFFFFFBFU;
        hdr.position_x_0024 =
            LoadFloatAt(bytes, 0x10cU) + hdr.position_x_0024;
        hdr.zspeed_0048 = LoadFloatAt(bytes, 0x108U);
        // Lazy arm of the +0xf4 timer record, then the unconditional reset.
        if ((LoadU32At(bytes, 0x104U) & 1U) == 0U) {
            StoreI32At(bytes, 0xf8U, 0);
            StoreU32At(bytes, 0xf4U, 0xFFF0BDC1U);
            StoreI32At(bytes, 0xfcU, 0);
            StoreU32At(bytes, 0x100U,
                       reinterpret_cast<u32>(&g_FrameTimeScale));
            StoreU32At(bytes, 0x104U, LoadU32At(bytes, 0x104U) | 1U);
        }
        StoreI32At(bytes, 0xf8U, 0);
        StoreI32At(bytes, 0xfcU, 0);
        StoreI32At(bytes, 0xf4U, -1);
        // Native rebuilds the vector with length = max_distance.
        Float2 vec;
        SetVectorFromAngle(&vec, hdr.angle_003c,
                           static_cast<float>(max_distance));
        hdr.velocity_x_0030 = vec.x;
        hdr.velocity_y_0034 = vec.y;
    } else {
        // length = base * (1 - rate / max_distance)
        const float base = hdr.zspeed_0048;
        const float rate = LoadFloatAt(bytes, 0xfcU);
        Float2 vec;
        SetVectorFromAngle(
            &vec, hdr.angle_003c,
            base * (kOne - rate / static_cast<float>(max_distance)));
        hdr.velocity_x_0030 = vec.x;
        hdr.velocity_y_0034 = vec.y;
        // Native still publishes [+0xf4] = [+0xf8] before the rate advance.
        StoreI32At(bytes, 0xf4U, LoadI32At(bytes, 0xf8U));
        const float rate_value = LoadFloatAt(
            *reinterpret_cast<void **>(bytes + 0x100U), 0U);
        if (rate_value > kRateLow && rate_value < kRateHigh) {
            StoreI32At(bytes, 0xf8U, LoadI32At(bytes, 0xf8U) + 1);
            StoreFloatAt(bytes, 0xfcU, LoadFloatAt(bytes, 0xfcU) + kOne);
        } else {
            StoreFloatAt(bytes, 0xfcU,
                         rate_value + LoadFloatAt(bytes, 0xfcU));
            // Native rounds half-away-from-zero through 0x463b2c.
            const double rounded = ::floor(
                static_cast<double>(LoadFloatAt(bytes, 0xfcU)) + 0.5);
            StoreI32At(bytes, 0xf8U, static_cast<i32>(rounded));
        }
        return;
    }

    // Shared tail on the terminal path: publish [+0xf4] = [+0xf8] and
    // advance the rate record identically.
    StoreI32At(bytes, 0xf4U, LoadI32At(bytes, 0xf8U));
    const float rate_value = LoadFloatAt(
        *reinterpret_cast<void **>(bytes + 0x100U), 0U);
    if (rate_value > kRateLow && rate_value < kRateHigh) {
        StoreI32At(bytes, 0xf8U, LoadI32At(bytes, 0xf8U) + 1);
        StoreFloatAt(bytes, 0xfcU, LoadFloatAt(bytes, 0xfcU) + kOne);
    } else {
        StoreFloatAt(bytes, 0xfcU, rate_value + LoadFloatAt(bytes, 0xfcU));
        const double rounded = ::floor(
            static_cast<double>(LoadFloatAt(bytes, 0xfcU)) + 0.5);
        StoreI32At(bytes, 0xf8U, static_cast<i32>(rounded));
    }
}

// ---- sweeps ---------------------------------------------------------------

namespace {

enum SweepMode {
    kSweepBox = 0,
    kSweepRadial = 1
};

// Shared engine of 0x41d880/0x41dd80 (kind A) and 0x41eb00/0x41efa0 (kind B).
// arg1: box mode - the position record {x,y,z} advanced across hit runs and
//       used as the box centre; radial mode - the target point the ring VMs
//       home on (advanced across the first hit run).
// arg2: box mode - three-float extent (half = extent * 0.5); radial mode -
//       the radius (squared once).
// flag: gates the per-hit explosion particle.
//
// The kind word and the descriptor copy size stay byte-mapped per kind
// (kMapA: descriptor_0424.script_kind_0026 at +0x44a with the 0x1dc copy,
//  kMapB: descriptor_0424.script_kind_0042 at +0x466 with the 0x1f8 copy).
// The shared-header fields are typed through StageObjectHeader; the +0x434
// publish slot (inside the descriptor image) and the raw_0068 feature-record
// region stay raw.
i32 SweepCommon(u8 *obj, float *arg1, const void *arg2, i32 flag,
                const SweepKindMap &map, SweepMode mode)
{
    StageObjectHeader &hdr = *reinterpret_cast<StageObjectHeader *>(obj);
    const i16 kind = static_cast<i16>(LoadU16At(obj, map.kind_word));
    const i32 script = kind * 2 + 0x11;

    float half[3];
    float min_box[3];
    float max_box[3];
    float radius_sq = 0.0f;
    if (mode == kSweepBox) {
        const float *extent = static_cast<const float *>(arg2);
        for (i32 i = 0; i < 3; ++i)
            half[i] = extent[i] * kHalf;
        for (i32 i = 0; i < 3; ++i) {
            min_box[i] = arg1[i] - half[i];
            max_box[i] = arg1[i] + half[i];
        }
    } else {
        const float radius = *static_cast<const float *>(arg2);
        radius_sq = radius * radius;
    }

    const float start[3] = { hdr.position_x_0024,
                             hdr.position_y_0028,
                             hdr.position_z_002c };
    Float2 dir;
    SetVectorFromAngle(&dir, hdr.angle_003c, kSweepStep);

    u8 hits[0x40];
    ZeroBlock(hits, 0x40U);
    i32 hit_count = 0;
    i32 steps = 0;
    float point[3] = { start[0], start[1], start[2] };
    float angle = kAngleStep;

    if (kAngleStep < hdr.depth_0040) {
        for (;;) {
            const bool hit =
                mode == kSweepBox
                    ? point[0] > min_box[0] && point[0] < max_box[0]
                          && point[1] > min_box[1]
                          && point[1] < max_box[1]
                    : (arg1[0] - point[0]) * (arg1[0] - point[0])
                              + (arg1[1] - point[1])
                                    * (arg1[1] - point[1])
                          < radius_sq;
            if (hit) {
                hits[steps] = 1;
                ++hit_count;
                const float tip[2] = { point[0], point[1] };
                if (flag != 0
                    && IsBoxOutsidePlayfieldEcxStackAbi(tip, 32.0f, 32.0f)
                           == 0) {
                    // Native 0x41da95 passes DAT_00477818's content (the
                    // bullet manager base) straight to 0x41bb00.
                    SpawnExplosionParticleEaxEcxEfxAbi(
                        g_BulletManagerSlot, point, 8, 0xFFFFFFFFU,
                        kExplosionAngle, kExplosionSpeed);
                }
                if (mode == kSweepBox)
                    SpawnRingEffectVm(script, start, start[2]);
                else
                    SpawnRingEffectVm(script, point, point[2]);
            }
            point[0] += dir.x;
            point[1] += dir.y;
            angle += kAngleStep;
            if (!(angle + kDepthWindow < hdr.depth_0040))
                break;
            ++steps;
        }
    }

    if (hit_count == 0)
        return 0;
    if (hit_count >= steps) {
        hdr.done_latch_0050 = 1U;
        return 0;
    }

    // Leading hit run: advance the argument position across it and step the
    // depth back by run*12; publish [+0x434]/[+0x4c] while above 18 degrees,
    // otherwise latch +0x50 and stop.
    i32 leading = 0;
    while (leading < steps && hits[leading] != 0)
        ++leading;
    if (leading > 0) {
        arg1[0] += dir.x * static_cast<float>(leading);
        arg1[1] += dir.y * static_cast<float>(leading);
        const float depth = hdr.depth_0040
            - static_cast<float>(leading) * kAngleStep;
        hdr.depth_0040 = depth;
        if (depth > kDepthLatch) {
            StoreFloatAt(obj, 0x434U, depth);
            hdr.zvel_004c = depth;
        } else {
            hdr.done_latch_0050 = 1U;
            return 0;
        }
    }

    // Gap scan: for every run of misses of width `gap`, step the depth by
    // gap*12 and (when the width exceeds 18 degrees) spawn at the gap:
    // box mode re-spawns a stage object from the descriptor copy, radial
    // mode spawns a ring VM homing on the advanced target.
    i32 cursor = leading;
    while (cursor < steps) {
        const i32 run_start = cursor;
        i32 gap = 0;
        while (cursor < steps && hits[cursor] == 0) {
            ++cursor;
            ++gap;
        }
        if (gap > 0) {
            StoreFloatAt(obj, 0x434U,
                         LoadFloatAt(obj, 0x434U)
                             - static_cast<float>(gap) * kAngleStep);
            hdr.depth_0040 = hdr.depth_0040
                - static_cast<float>(gap) * kAngleStep;
            if (hdr.depth_0040 >= kDepthLatch)
                hdr.done_latch_0050 = 1U;
            if (gap * 12 > 18) {
                if (mode == kSweepBox) {
                    u8 desc_copy[0x1dcU];
                    CopyBlock(desc_copy, obj + 0x424U, // descriptor_0424 image
                              map.descriptor_size);
                    float spawn_pos[3] = {
                        start[0] + dir.x * static_cast<float>(run_start),
                        start[1] + dir.y * static_cast<float>(run_start),
                        start[2]
                    };
                    (void)spawn_pos;
                    SpawnStageObjectEsiEdiStackAbi(g_BulletListRootSlot,
                                                   desc_copy, 0);
                } else {
                    float spawn_pos[3] = {
                        arg1[0] + dir.x * static_cast<float>(run_start),
                        arg1[1] + dir.y * static_cast<float>(run_start),
                        arg1[2]
                    };
                    SpawnRingEffectVm(script, spawn_pos, spawn_pos[2]);
                }
            }
        }
        while (cursor < steps && hits[cursor] != 0)
            ++cursor;
    }
    return 0;
}

} // namespace

// TH10 0x0041d880 (slot 6, kind A).
i32 TH10_STDCALL StageObjectSweepBoxA(void *object, const float center[3],
                                      const float extent[3], i32 flag)
{
    (void)center;
    StageObjectKindA &so = *reinterpret_cast<StageObjectKindA *>(object);
    return SweepCommon(reinterpret_cast<u8 *>(&so),
                       const_cast<float *>(extent), extent, flag, kMapA,
                       kSweepBox);
}

// TH10 0x0041dd80 (slot 7, kind A).
i32 TH10_STDCALL StageObjectSweepRadialA(void *object, const float target[3],
                                         float radius, i32 flag)
{
    StageObjectKindA &so = *reinterpret_cast<StageObjectKindA *>(object);
    return SweepCommon(reinterpret_cast<u8 *>(&so),
                       const_cast<float *>(target), &radius, flag, kMapA,
                       kSweepRadial);
}

// TH10 0x0041eb00 (slot 6, kind B).
i32 TH10_STDCALL StageObjectSweepBoxB(void *object, const float center[3],
                                      const float extent[3], i32 flag)
{
    (void)center;
    StageObjectKindB &so = *reinterpret_cast<StageObjectKindB *>(object);
    return SweepCommon(reinterpret_cast<u8 *>(&so),
                       const_cast<float *>(extent), extent, flag, kMapB,
                       kSweepBox);
}

// TH10 0x0041efa0 (slot 7, kind B).
i32 TH10_STDCALL StageObjectSweepRadialB(void *object, const float target[3],
                                         float radius, i32 flag)
{
    StageObjectKindB &so = *reinterpret_cast<StageObjectKindB *>(object);
    return SweepCommon(reinterpret_cast<u8 *>(&so),
                       const_cast<float *>(target), &radius, flag, kMapB,
                       kSweepRadial);
}

// TH10 0x0041e260 (slot 5, kind A; the 0x41c850 bullet-clear virtual).
// Spawns a ring VM every 12 degrees from the object position until the angle
// passes the depth; optionally plays the explosion particle per step.
i32 TH10_STDCALL StageObjectSpreadA(void *object, i32 enable_explosion)
{
    StageObjectKindA &so = *reinterpret_cast<StageObjectKindA *>(object);
    StageObjectHeader &hdr = so.header;
    const i16 kind = static_cast<i16>(so.descriptor_0424.script_kind_0026);
    const i32 script = kind * 2 + 0x11;

    float point[3] = { hdr.position_x_0024,
                       hdr.position_y_0028,
                       hdr.position_z_002c };
    Float2 dir;
    SetVectorFromAngle(&dir, hdr.angle_003c, kSweepStep);
    float angle = kAngleStep;

    if (kAngleStep < hdr.depth_0040) {
        for (;;) {
            SpawnRingEffectVm(script, point, point[2]);
            if (enable_explosion != 0) {
                const float tip[2] = { point[0], point[1] };
                if (IsBoxOutsidePlayfieldEcxStackAbi(tip, 32.0f, 32.0f)
                        == 0) {
                    // Native 0x41e445 passes DAT_00477818's content (the
                    // bullet manager base) straight to 0x41bb00.
                    SpawnExplosionParticleEaxEcxEfxAbi(
                        g_BulletManagerSlot, point, 8,
                        0xFFFFFFFFU, kExplosionAngle, kExplosionSpeed);
                }
            }
            point[0] += dir.x;
            point[1] += dir.y;
            angle += kAngleStep;
            if (!(angle + kDepthWindow < hdr.depth_0040))
                break;
        }
    }
    hdr.state_000c = 1;
    return 0;
}

// TH10 0x0041f400 (slot 5, kind B). Spread with the playfield gate before
// each ring VM spawn.
i32 TH10_STDCALL StageObjectSpreadB(void *object, i32 enable_explosion)
{
    StageObjectKindB &so = *reinterpret_cast<StageObjectKindB *>(object);
    StageObjectHeader &hdr = so.header;
    const i16 kind = static_cast<i16>(so.descriptor_0424.script_kind_0042);
    const i32 script = kind * 2 + 0x11;

    float point[3] = { hdr.position_x_0024,
                       hdr.position_y_0028,
                       hdr.position_z_002c };
    Float2 dir;
    SetVectorFromAngle(&dir, hdr.angle_003c, kSweepStep);
    float angle = kAngleStep;

    if (kAngleStep < hdr.depth_0040) {
        for (;;) {
            const float tip[2] = { point[0], point[1] };
            if (IsBoxOutsidePlayfieldEcxStackAbi(tip, kTipGateZ, kTipGateZ)
                    == 0) {
                SpawnRingEffectVm(script, point, point[2]);
                if (enable_explosion != 0
                    && IsBoxOutsidePlayfieldEcxStackAbi(tip, 32.0f, 32.0f)
                           == 0) {
                    // Native passes DAT_00477818's content (the bullet
                    // manager base) straight to 0x41bb00.
                    SpawnExplosionParticleEaxEcxEfxAbi(
                        g_BulletManagerSlot, point, 8,
                        0xFFFFFFFFU, kExplosionAngle, kExplosionSpeed);
                }
            }
            point[0] += dir.x;
            point[1] += dir.y;
            angle += kAngleStep;
            if (!(angle + kDepthWindow < hdr.depth_0040))
                break;
        }
    }
    hdr.state_000c = 1;
    return 0;
}

// TH10 0x0041e700 (slot 2, kind B).
i32 TH10_STDCALL StageObjectUpdateB(void *object)
{
    StageObjectKindB &so = *reinterpret_cast<StageObjectKindB *>(object);
    StageObjectHeader &hdr = so.header;
    StageObjectKindBDescriptor &desc = so.descriptor_0424;

    // Depth advance: only while below the +0x444 limit, clamped on top.
    {
        const float z = hdr.depth_0040;
        const float zlim = desc.depth_clamp_0020;
        if (z < zlim) {
            const float advanced = z
                + g_FrameTimeScale * hdr.zspeed_0048;
            hdr.depth_0040 = advanced;
            if (advanced > zlim)
                hdr.depth_0040 = zlim;
        }
    }

    // Angle wrap: angle = wrap(angle + rate * +0x440) (0x44bc10).
    hdr.angle_003c = WrapAngleSumStackAbi(
        hdr.angle_003c,
        g_FrameTimeScale * desc.angle_rate_001c);

    // Descriptor flag bit 1: follow the chain position at
    // [DAT_00477704+0x10]+0x1068. The +0x10 slot is published_ids[0] (the
    // primary stage/battle ECL record pointer), a modeled field.
    if ((desc.flags_0044 & 1U) != 0U) {
        ConditionalState &hud_cond =
            *static_cast<ConditionalState *>(g_AsciiHudConditionalState);
        const u32 chain = hud_cond.published_ids[0];
        if (chain != 0U)
            CopyBlock(&hdr.position_x_0024,
                      reinterpret_cast<const u8 *>(chain) + 0x1068U, 0x0cU);
    }

    // Entrance drift.
    const float rate = g_FrameTimeScale;
    hdr.position_x_0024 = hdr.position_x_0024
        + rate * desc.velocity_x_000c;
    hdr.position_y_0028 = hdr.position_y_0028
        + rate * desc.velocity_y_0010;
    hdr.position_z_002c = hdr.position_z_002c
        + rate * desc.velocity_z_0014;

    // Entrance state machine (states 2..5 via the 0x41ea30 jump table);
    // the tick value lives at +0x14 (timer_0010.count), the timers at
    // +0x454/+0x458/+0x45c/+0x460 (entrance_timer_a..d_0030..3c) and the
    // alpha scale at +0x44c (alpha_target_0028).
    switch (hdr.state_000c) {
    case 3:
        if (hdr.timer_0010.count >= desc.entrance_timer_a_0030) {
            TickPlayerTimerEaxStackAbi(
                reinterpret_cast<u8 *>(&hdr.timer_0010), 0);
            hdr.state_000c = 4;
        }
        break;
    case 4:
        if (hdr.timer_0010.count >= desc.entrance_timer_b_0034) {
            TickPlayerTimerEaxStackAbi(
                reinterpret_cast<u8 *>(&hdr.timer_0010), 0);
            hdr.state_000c = 2;
            hdr.alpha_0044 = desc.alpha_target_0028;
            if (hdr.timer_0010.count >= desc.entrance_timer_c_0038) {
                TickPlayerTimerEaxStackAbi(
                    reinterpret_cast<u8 *>(&hdr.timer_0010), 0);
                hdr.state_000c = 5;
                if (hdr.timer_0010.count >= desc.entrance_timer_d_003c)
                    return 1;
                hdr.alpha_0044 =
                    desc.alpha_target_0028
                        - desc.alpha_target_0028
                              * LoadFloatAt(&hdr.timer_0010,
                                            0x08U) // +0x18 accum as float
                              / static_cast<float>(
                                    desc.entrance_timer_d_003c);
            } else {
                hdr.alpha_0044 =
                    desc.alpha_target_0028
                        * LoadFloatAt(&hdr.timer_0010,
                                      0x08U) // +0x18 accum as float
                        / static_cast<float>(
                              desc.entrance_timer_b_0034);
            }
        }
        break;
    default:
        break;
    }

    // Tip emission for states 4 and 2 (same shape as kind A).
    const i32 state = hdr.state_000c;
    if (state == 4 || state == 2) {
        const float z = hdr.depth_0040;
        if (z > kTipGateZ) {
            Float2 vec;
            SetVectorFromAngle(&vec, hdr.angle_003c, z * kTipScaleZ);
            float tip2[3];
            tip2[0] = vec.x + hdr.position_x_0024;
            tip2[1] = vec.y + hdr.position_y_0028;
            // Native accumulates the z component over a stale-seeded block.
            tip2[2] = 0.0f + hdr.position_z_002c;

            const float y = hdr.alpha_0044;
            const float half_y = y < kTipAlpha
                ? y * kHalf
                : y - (y + kTipGateZ) * kTipScaleC;
            const i32 region = ClassifyPositionInPlayerRegionEaxEcxStackAbi(
                tip2, g_OptionPositionBase, hdr.angle_003c,
                half_y, z * kTipScaleB);
            if (region == 1) {
                typedef i32 (TH10_STDCALL *SweepFn)(void *, const float *,
                                                    const float *, i32);
                const SweepFn sweep =
                    *reinterpret_cast<const SweepFn *>(
                        static_cast<u8 *>(hdr.callback_table_0000)
                            + 6U * 4U);
                PlayerRecord &player =
                    *reinterpret_cast<PlayerRecord *>(
                        g_OptionPositionBase);
                const float *target = &player.position_x;
                sweep(&so, target, tip2, 0);
            } else if (region == 2) {
                if (hdr.timer_0010.count % 5 == 0) {
                    u8 *stage = static_cast<u8 *>(g_EffectManagerRoot);
                    SpawnStageEffectEdxEbxAbi(
                        reinterpret_cast<void *>(LoadU32At(stage,
                                                           0x3e0b50U)),
                        tip2, 0x1b2);
                    QueueBulletDeathEffectEbxEsiStackAbi(
                        0x1c, EffectManager(), hdr.position_x_0024);
                }
            }
        }
    }

    // VM publication (kind B record bases).
    {
        u8 *rec1 = reinterpret_cast<u8 *>(&so.vm1_061c);
        VmRecord &rec_vm = so.vm1_061c;
        const void *scale_ptr = rec_vm.anim_entry;
        rec_vm.flags |= 8U;
        rec_vm.scale_x =
            hdr.alpha_0044 / LoadFloatAt(scale_ptr, 0x34U);
        rec_vm.scale_y =
            hdr.depth_0040 / LoadFloatAt(scale_ptr, 0x30U);
        FinalizeTimelineRenderObjectSetup(rec1);
        if (hdr.zvel_004c == kZero)
            FinalizeTimelineRenderObjectSetup(
                reinterpret_cast<u8 *>(&so.vm2_09c8));
    }
    return 0;
}

// TH10 0x0041d2c0 (table 0x46da60 slot 10, feature flag 0x10). Frame-time
// drift: while the +0x90 tick count stays below the +0xb4 limit, scale the
// depth rate (+0x48) and the velocity triple (+0x30/+0x34/+0x38) by
// frame_time_scale * the +0xa0/+0xa8/+0xac/+0xb0 multipliers, refresh the
// angle from atan2(velocity.y, velocity.x) once either component's
// magnitude passes 7.5f (0x470c58), then advance the +0x8c/+0x90/+0x94/
// +0x98 rate record. Reaching the limit clears feature flag 0x10 and
// stops the drift. All of the drift state (+0x8c..+0xb4) lives inside
// header.raw_0068.
void TH10_STDCALL StageObjectDriftSlotA(void *object)
{
    StageObjectKindA &so = *reinterpret_cast<StageObjectKindA *>(object);
    StageObjectHeader &hdr = so.header;
    u8 *const bytes = reinterpret_cast<u8 *>(&so); // raw_0068 drift record
    const i32 ticks = LoadI32At(bytes, 0x90U);

    if (ticks >= LoadI32At(bytes, 0xb4U)) {
        hdr.feature_flags_0404 &= 0xffffffefU;
        return;
    }

    const float scaled = g_FrameTimeScale;
    hdr.zspeed_0048 =
        hdr.zspeed_0048 + scaled * LoadFloatAt(bytes, 0xa0U);
    hdr.velocity_x_0030 =
        hdr.velocity_x_0030 + scaled * LoadFloatAt(bytes, 0xa8U);
    hdr.velocity_y_0034 =
        hdr.velocity_y_0034 + scaled * LoadFloatAt(bytes, 0xacU);
    hdr.velocity_z_0038 =
        hdr.velocity_z_0038 + scaled * LoadFloatAt(bytes, 0xb0U);

    const float vel_x = hdr.velocity_x_0030;
    const float vel_y = hdr.velocity_y_0034;
    const float threshold = FloatFromBits(0x40F00000U); // 0x470c58 7.5f
    if ((vel_x < -threshold || vel_x > threshold)
        || (vel_y < -threshold || vel_y > threshold)) {
        // fpatan(y, x) with the result into +0x3c.
        hdr.angle_003c = static_cast<float>(
            std::atan2(static_cast<double>(vel_y),
                       static_cast<double>(vel_x)));
    }

    // Publish the pre-advance tick count, then run the shared rate-record
    // step over {+0x90 count, +0x94 accumulator, +0x98 rate pointer}.
    StoreI32At(bytes, 0x8cU, ticks);
    const float rate_value = LoadFloatAt(
        reinterpret_cast<void *>(LoadU32At(bytes, 0x98U)), 0U);
    if (rate_value > kRateLow && rate_value < kRateHigh) {
        StoreI32At(bytes, 0x90U, ticks + 1);
        StoreFloatAt(bytes, 0x94U,
                     LoadFloatAt(bytes, 0x94U) + kOne);
    } else {
        const float advanced = LoadFloatAt(bytes, 0x94U) + rate_value;
        StoreFloatAt(bytes, 0x94U, advanced);
        // Native rounds half-away-from-zero through 0x463b2c.
        const double rounded = advanced >= 0.0f
            ? std::floor(static_cast<double>(advanced) + 0.5)
            : std::ceil(static_cast<double>(advanced) - 0.5);
        StoreI32At(bytes, 0x90U, static_cast<i32>(rounded));
    }
}

// TH10 0x0041ca80 kind A script interpreter (see StageObjectVtable.hpp).
// Not part of this batch; the slot-0 dispatch calls through the table so a
// later reconstruction can drop in transparently.
void StageObjectScriptTickA(void *object)
{
    (void)object;
}

// Unimplemented sibling slots referenced by the two native tables
// (slots 3/4/8 of kind A and 3/4/8/10 of kind B; kind A slot 10 is now
// reconstructed above as StageObjectDriftSlotA). Only ever reached
// through the tables.
void StageObjectSlotBoundary(void *object)
{
    (void)object;
}

} // namespace th10
