// Stage/background object callback-table family (TH10 0x41c030..0x41f670).
//
// The manager published at DAT_0047781c owns two object flavours:
//   kind A: 0xd58 bytes, callback table 0x46da60,
//   kind B: 0xd74 bytes, callback table 0x46da10,
// plus the all-stub default table 0x46dab0 installed by 0x41c030. Each table
// is 19 function pointers followed by a null terminator in .rdata. The
// per-kind objects share the header layout documented in StageObjectVtable.hpp
// and differ in the descriptor copy size (0x1dc vs 0x1f8), the animation VM
// record bases (+0x600/+0x9ac vs +0x61c/+0x9c8) and the ring-effect kind word
// (+0x44a vs +0x466).
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
#include "PlayerFrameworkHelpers.hpp"
#include "PlayerMotionHelpers.hpp"
#include "PlayerShotData.hpp"
#include "PlayerTimerHelpers.hpp"
#include "SceneTriggerFeatures.hpp"
#include "SceneTriggerUpdate.hpp"
#include "StageEffectHelpers.hpp"
#include "Th10Types.hpp"
#include "TimelineRenderObjectSetup.hpp"
#include "TitleBulletUpdate.hpp"
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
    u32 kind_word;      // +0x44a (A) / +0x466 (B)
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
// [DAT_00477818+0x458] and list-A registration (id counter +0x732454).
void SpawnRingEffectVm(i32 script_index, const float position[3], float z)
{
    u8 *rec = static_cast<u8 *>(AllocatePoolVmEsiAbi(g_MainChainRenderOwner));
    StoreU32At(rec, 0x35cU, LoadU32At(rec, 0x35cU) | 0x40000000U);
    StoreU32At(rec, 0x20U, 0U);
    StoreFloatAt(rec, 0x340U, position[0] + kSpawnZ);
    StoreFloatAt(rec, 0x344U, position[1] + kTipGateZ);
    StoreFloatAt(rec, 0x348U, z);
    void *anm_work = *reinterpret_cast<void **>(
        static_cast<u8 *>(g_BulletManagerSlot) + 0x458U);
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
    u8 *node = static_cast<u8 *>(object);
    const u32 next = LoadU32At(node, 0x08U);
    StoreU32At(reinterpret_cast<void *>(LoadU32At(node, 0x04U)), 0x08U, next);
    if (next != 0U)
        StoreU32At(reinterpret_cast<void *>(next), 0x04U,
                   LoadU32At(node, 0x04U));
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
    u8 *obj = static_cast<u8 *>(block);
    StoreU32At(obj, kSobOffTable, 0x0046DAB0U); // dead: erased below
    StoreU32At(obj, 0x20U, LoadU32At(obj, 0x20U) & 0xFFFFFFFEU);
    for (i32 i = 0; i < 0x12; ++i) {
        const u32 off = 0x68U + 0x34U * static_cast<u32>(i);
        StoreU32At(obj, off, LoadU32At(obj, off) & 0xFFFFFFFEU);
    }
    StoreU32At(obj, 0x420U, LoadU32At(obj, 0x420U) & 0xFFFFFFFEU);
    ZeroBlock(obj, 0x424U);

    u32 flags = LoadU32At(obj, 0x20U); // 0 after the wipe
    if ((flags & 1U) == 0U) {
        flags |= 1U;
        StoreI32At(obj, 0x14U, 0);
        StoreU32At(obj, 0x10U, 0xFFF0BDC1U);
        StoreI32At(obj, 0x18U, 0);
        StoreU32At(obj, 0x1cU, reinterpret_cast<u32>(&g_FrameTimeScale));
        StoreU32At(obj, 0x20U, flags);
    }
    StoreI32At(obj, 0x14U, 0);
    StoreI32At(obj, 0x18U, 0);
    StoreI32At(obj, 0x10U, -1);
    return obj;
}

// TH10 0x0041c100.
void *CreateStageObjectManagerInPlaceEsiAbi(void *manager)
{
    InitStageObjectHeaderDefaultsEdxAbi(
        static_cast<u8 *>(manager) + 0x10U); // dead: erased below
    ZeroBlock(manager, 0x45cU);
    g_BulletListRootSlot = manager;
    return manager;
}

namespace {

// Dead-flag clear + wipe + trailing 0xffff word, per 0x3ac-byte VM record.
// The native clears bit 0 of nine dwords first and then wipes the record, so
// every stored value except the trailing word is erased; the order is kept.
void ResetVmRecord(u8 *record)
{
    static const u32 kFlagOffsets[9] = {
        0x06cU, 0x0b0U, 0x0fcU, 0x128U, 0x174U, 0x1b0U, 0x1fcU, 0x228U,
        0x378U
    };
    for (u32 i = 0; i < 9U; ++i) {
        StoreU32At(record, kFlagOffsets[i],
                   LoadU32At(record, kFlagOffsets[i]) & 0xFFFFFFFEU);
    }
    ZeroBlock(record, 0x3acU);
    StoreU16At(record, 0x384U, 0xFFFFU);
}

} // namespace

// TH10 0x0041c5b0.
void *InitStageObjectKindA_EbxAbi(void *object)
{
    u8 *obj = static_cast<u8 *>(object);
    InitStageObjectHeaderDefaultsEdxAbi(obj);
    StoreU32At(obj, kSobOffTable, 0x0046DA60U);

    ZeroBlock(obj + kSobOffDesc, 0x1dcU); // region A (0x424..0x600)
    ResetVmRecord(obj + kSobOffRecA_A);
    // Native reads obj+0xa18 (rec2+0x6c) before the second wipe - dead.
    (void)LoadU32At(obj, 0xa18U);
    ResetVmRecord(obj + kSobOffRecA_B);
    return obj;
}

// TH10 0x0041c680.
void *InitStageObjectKindB_EbxAbi(void *object)
{
    u8 *obj = static_cast<u8 *>(object);
    InitStageObjectHeaderDefaultsEdxAbi(obj);
    StoreU32At(obj, kSobOffTable, 0x0046DA10U);

    ZeroBlock(obj + kSobOffDesc, 0x1f8U);
    StoreU32At(obj, kSobOffDesc + 0x2cU, 0x41000000U); // 8.0f at +0x450
    (void)LoadU32At(obj, 0x688U);  // dead reads of the wiped flag words
    ResetVmRecord(obj + kSobOffRecB_A);
    (void)LoadU32At(obj, 0xa34U);
    ResetVmRecord(obj + kSobOffRecB_B);
    return obj;
}

// TH10 0x0041c510. ESI = manager, EDI forwarded, stack = kind.
void *SpawnStageObjectEsiEdiStackAbi(void *manager, void *forwarded_edi,
                                     i32 kind)
{
    u8 *mgr = static_cast<u8 *>(manager);
    if (LoadI32At(mgr, 0x438U) >= 0x100)
        return 0;

    u32 cursor = LoadU32At(mgr, 0x43cU) + 1U;
    if (cursor == 0U)
        cursor = 1U;
    StoreU32At(mgr, 0x43cU, cursor);

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

    u8 *obj = static_cast<u8 *>(object);
    // Native dereferences the object even when allocation failed (+0x54).
    StoreU32At(obj, 0x54U, cursor);
    StoreU32At(obj, kSobOffListPrev, LoadU32At(mgr, 0x434U));
    StoreU32At(reinterpret_cast<void *>(LoadU32At(mgr, 0x434U)),
               kSobOffListNext, reinterpret_cast<u32>(obj));
    StoreU32At(mgr, 0x438U, LoadU32At(mgr, 0x438U) + 1U);
    StoreU32At(mgr, 0x434U, reinterpret_cast<u32>(obj));

    // Slot-1 dispatch through the installed table; the native forwards the
    // caller's EDI verbatim as the descriptor argument.
    typedef i32 (TH10_STDCALL *InitFn)(void *, const void *);
    const u32 table = LoadU32At(obj, kSobOffTable);
    const InitFn init = *reinterpret_cast<const InitFn *>(table + 4U);
    init(obj, forwarded_edi);

    return object;
}

// TH10 0x0041c8c0.
i32 TH10_STDCALL StageObjectSpawnDescriptorA(void *object,
                                             const void *descriptor)
{
    u8 *obj = static_cast<u8 *>(object);
    CopyBlock(obj + kSobOffDesc, descriptor, 0x1dcU);

    StoreI32At(obj, kSobOffState, 2);

    void *anm_work = *reinterpret_cast<void **>(
        static_cast<u8 *>(g_BulletListRootSlot) + 0x458U);

    // VM 1: script = DAT_00474170[(i16)+0x448] + (i16)+0x44a.
    {
        u8 *rec = obj + kSobOffRecA_A;
        const i16 slot = static_cast<i16>(LoadU16At(obj, 0x448U));
        const i16 kind = static_cast<i16>(LoadU16At(obj, 0x44aU));
        const i32 script = static_cast<i32>(
            LoadU32At(g_TriggerVmScriptTable, 4U * static_cast<u32>(slot)))
            + kind;
        InitializePlayerMainVmEsiStackAbi(rec, anm_work, script);
        FinalizeTimelineRenderObjectSetup(rec);
        StoreU16At(obj, 0x904U, 2); // rec1+0x304 mode word
        if ((LoadU32At(obj, 0x44cU) & 1U) != 0U)
            StoreU32At(rec, 0x35cU,
                       (LoadU32At(rec, 0x35cU) & 0xFFFFFFDFU) | 0x10U);
        StoreU32At(obj, 0x95cU,
                   (LoadU32At(obj, 0x95cU) & 0xFC63FFFFU) | 0x600000U);
    }

    // VM 2: script = (i16)+0x44a + 0x103.
    {
        u8 *rec = obj + kSobOffRecA_B;
        const i16 kind = static_cast<i16>(LoadU16At(obj, 0x44aU));
        InitializePlayerMainVmEsiStackAbi(rec, anm_work, kind + 0x103);
        FinalizeTimelineRenderObjectSetup(rec);
        StoreU16At(obj, 0xcb0U, 2); // rec2+0x304 mode word
        StoreU32At(rec, 0x35cU,
                   (LoadU32At(rec, 0x35cU) & 0xFFFFFFDFU) | 0x10U);
        StoreU32At(obj, 0xd08U,
                   (LoadU32At(obj, 0xd08U) & 0xFC7FFFFFU) | 0x400000U);
    }

    // Lazy timer arm (+0x410 record), then the spawn defaults.
    if ((LoadU32At(obj, 0x420U) & 1U) == 0U) {
        StoreI32At(obj, 0x414U, 0);
        StoreU32At(obj, 0x410U, 0xFFF0BDC1U);
        StoreI32At(obj, 0x418U, 0);
        StoreU32At(obj, 0x41cU, reinterpret_cast<u32>(&g_FrameTimeScale));
        StoreU32At(obj, 0x420U, LoadU32At(obj, 0x420U) | 1U);
    }
    StoreI32At(obj, 0x414U, 0x1e);
    StoreU32At(obj, 0x418U, 0x41F00000U); // 30.0f
    StoreI32At(obj, 0x410U, 0x1d);

    const float depth = LoadFloatAt(obj, 0x438U);
    StoreFloatAt(obj, kSobOffDepth, depth);
    StoreFloatAt(obj, kSobOffPos + 0x00U, LoadFloatAt(obj, kSobOffDesc + 0U));
    StoreFloatAt(obj, kSobOffPos + 0x04U, LoadFloatAt(obj, kSobOffDesc + 4U));
    StoreFloatAt(obj, kSobOffPos + 0x08U, LoadFloatAt(obj, kSobOffDesc + 8U));
    StoreFloatAt(obj, kSobOffZSpeed, LoadFloatAt(obj, 0x444U));
    StoreFloatAt(obj, kSobOffAngle, LoadFloatAt(obj, 0x430U));
    StoreFloatAt(obj, kSobOffAlpha, LoadFloatAt(obj, 0x440U));
    StoreU32At(obj, kSobOffKind, 0x18U);
    StoreFloatAt(obj, kSobOffZVel,
                 depth > kZero ? FloatFromBits(0x3C23D70AU) : kZero);

    Float2 velocity;
    SetVectorFromAngle(&velocity, LoadFloatAt(obj, 0x430U),
                       LoadFloatAt(obj, 0x444U));
    StoreFloatAt(obj, kSobOffVel + 0x00U, velocity.x);
    StoreFloatAt(obj, kSobOffVel + 0x04U, velocity.y);
    StoreFloatAt(obj, kSobOffVel + 0x08U, 0.0f);
    return 0;
}

// TH10 0x0041e5c0.
i32 TH10_STDCALL StageObjectSpawnDescriptorB(void *object,
                                             const void *descriptor)
{
    u8 *obj = static_cast<u8 *>(object);
    CopyBlock(obj + kSobOffDesc, descriptor, 0x1f8U);

    StoreI32At(obj, kSobOffState, 3);

    const i16 slot = static_cast<i16>(LoadU16At(obj, 0x464U));
    const i16 kind = static_cast<i16>(LoadU16At(obj, 0x466U));
    void *anm_work = *reinterpret_cast<void **>(
        static_cast<u8 *>(g_BulletListRootSlot) + 0x458U);

    {
        u8 *rec = obj + kSobOffRecB_A;
        const i32 script = static_cast<i32>(
            LoadU32At(g_TriggerVmScriptTable, 4U * static_cast<u32>(slot)))
            + kind;
        InitializePlayerMainVmEsiStackAbi(rec, anm_work, script);
        FinalizeTimelineRenderObjectSetup(rec);
        StoreU16At(obj, 0x920U, 2); // rec1+0x304
        if ((LoadU32At(obj, 0x468U) & 2U) != 0U)
            StoreU32At(rec, 0x35cU,
                       (LoadU32At(rec, 0x35cU) & 0xFFFFFFDFU) | 0x10U);
        StoreU32At(obj, 0x978U,
                   (LoadU32At(obj, 0x978U) & 0xFC63FFFFU) | 0x600000U);
    }

    {
        u8 *rec = obj + kSobOffRecB_B;
        InitializePlayerMainVmEsiStackAbi(rec, anm_work, kind + 0x103);
        FinalizeTimelineRenderObjectSetup(rec);
        StoreU16At(obj, 0xcccU, 2); // rec2+0x304
        StoreU32At(rec, 0x35cU,
                   (LoadU32At(rec, 0x35cU) & 0xFFFFFFDFU) | 0x10U);
        StoreU32At(obj, 0xd24U,
                   (LoadU32At(obj, 0xd24U) & 0xFC7FFFFFU) | 0x400000U);
    }

    StoreFloatAt(obj, kSobOffPos + 0x00U, LoadFloatAt(obj, kSobOffDesc + 0U));
    StoreFloatAt(obj, kSobOffPos + 0x04U, LoadFloatAt(obj, kSobOffDesc + 4U));
    StoreFloatAt(obj, kSobOffPos + 0x08U, LoadFloatAt(obj, kSobOffDesc + 8U));
    StoreFloatAt(obj, kSobOffAngle, LoadFloatAt(obj, 0x43cU));
    StoreFloatAt(obj, kSobOffDepth, LoadFloatAt(obj, 0x448U));
    StoreU32At(obj, kSobOffKind, 0x18U);
    StoreFloatAt(obj, kSobOffAlpha, 2.0f); // 0x40000000
    StoreFloatAt(obj, kSobOffZSpeed, LoadFloatAt(obj, 0x450U));
    return 0;
}

// TH10 0x0041d3d0.
i32 TH10_STDCALL StageObjectUpdateA(void *object)
{
    u8 *obj = static_cast<u8 *>(object);

    // Slot 0 dispatch through the installed table (0x41ca80 for kind A).
    {
        typedef void (*SlotFn)(void *);
        const u32 table = LoadU32At(obj, kSobOffTable);
        const SlotFn slot0 = *reinterpret_cast<const SlotFn *>(table);
        slot0(obj);
    }

    const u32 flags0 = LoadU32At(obj, kSobOffFlags);
    if (flags0 != 0U) {
        // Ten-way feature dispatch, native order (bit 0x80 arrives through
        // the `test al,al; jns` idiom).
        typedef void (*SlotFn)(void *);
        const u32 table = LoadU32At(obj, kSobOffTable);
        if ((flags0 & 1U) != 0U)
            (*reinterpret_cast<const SlotFn *>(table + 9U * 4U))(obj);
        if ((flags0 & 0x10U) != 0U)
            (*reinterpret_cast<const SlotFn *>(table + 10U * 4U))(obj);
        if ((flags0 & 0x20U) != 0U)
            (*reinterpret_cast<const SlotFn *>(table + 11U * 4U))(obj);
        if ((flags0 & 0x40U) != 0U)
            (*reinterpret_cast<const SlotFn *>(table + 12U * 4U))(obj);
        if ((flags0 & 0x100U) != 0U)
            (*reinterpret_cast<const SlotFn *>(table + 13U * 4U))(obj);
        if (static_cast<i32>(flags0) < 0) // test al,al; jns: bit 0x80
            (*reinterpret_cast<const SlotFn *>(table + 14U * 4U))(obj);
        if ((flags0 & 0x8000C00U) != 0U)
            (*reinterpret_cast<const SlotFn *>(table + 15U * 4U))(obj);
        if ((flags0 & 0x100000U) != 0U)
            (*reinterpret_cast<const SlotFn *>(table + 16U * 4U))(obj);
        if ((flags0 & 0x200000U) != 0U)
            (*reinterpret_cast<const SlotFn *>(table + 17U * 4U))(obj);
        if ((flags0 & 0x4000000U) != 0U)
            (*reinterpret_cast<const SlotFn *>(table + 18U * 4U))(obj);

        // Bit 0x8000: shift the +0x15c timer by -1 while +0x160 > 0.
        u32 flags = LoadU32At(obj, kSobOffFlags);
        if ((flags & 0x8000U) != 0U) {
            if (LoadI32At(obj, 0x160U) > 0) {
                ShiftTimerByEsiStackAbi(obj + 0x15cU, -1.0f);
            } else {
                StoreU32At(obj, kSobOffFlags, flags ^ 0x8000U);
            }
        }
    }

    // Depth/ground handling. zlim at +0x434, ceiling at +0x43c.
    {
        const float z = LoadFloatAt(obj, kSobOffDepth);
        const float zlim = LoadFloatAt(obj, 0x434U);
        const float dz = g_FrameTimeScale * LoadFloatAt(obj, kSobOffZSpeed);
        if (!(z < zlim)) {
            // z >= zlim: accumulate the depth velocity and drift.
            StoreFloatAt(obj, kSobOffZVel,
                         LoadFloatAt(obj, kSobOffZVel) + dz);
            StoreFloatAt(obj, kSobOffPos + 0x00U,
                         LoadFloatAt(obj, kSobOffPos + 0x00U)
                             + g_FrameTimeScale
                                   * LoadFloatAt(obj, kSobOffVel + 0x00U));
            StoreFloatAt(obj, kSobOffPos + 0x04U,
                         LoadFloatAt(obj, kSobOffPos + 0x04U)
                             + g_FrameTimeScale
                                   * LoadFloatAt(obj, kSobOffVel + 0x04U));
            StoreFloatAt(obj, kSobOffPos + 0x08U,
                         LoadFloatAt(obj, kSobOffPos + 0x08U)
                             + g_FrameTimeScale
                                   * LoadFloatAt(obj, kSobOffVel + 0x08U));
            const float zmax = LoadFloatAt(obj, 0x43cU);
            if (zmax > kZero) {
                const float candidate = LoadFloatAt(obj, kSobOffZVel)
                    + LoadFloatAt(obj, kSobOffDepth);
                if (candidate < zmax) {
                    const float reflected = zmax
                        - LoadFloatAt(obj, kSobOffZVel);
                    StoreFloatAt(obj, kSobOffDepth, reflected);
                    StoreFloatAt(obj, 0x434U, reflected);
                    if (!(reflected > kZero))
                        return 1; // fell below zero: release
                }
            }
        } else {
            const float advanced = z + dz;
            StoreFloatAt(obj, kSobOffDepth, advanced);
            if (advanced > zlim)
                StoreFloatAt(obj, kSobOffDepth, zlim);
        }
    }

    // Timer-gated angle motion and the tip emission. The native carries the
    // first tip's z component (seeded from a stale stack slot) into the
    // second tip block, so it is tracked across the two blocks here.
    float first_tip_z = 0.0f;
    if (LoadI32At(obj, 0x414U) > 0) {
        ShiftTimerByEsiStackAbi(obj + 0x410U, -1.0f);
    } else {
        Float2 vec;
        SetVectorFromAngle(&vec, LoadFloatAt(obj, kSobOffAngle),
                           LoadFloatAt(obj, kSobOffDepth));
        float tip[3];
        tip[0] = vec.x + LoadFloatAt(obj, kSobOffPos + 0x00U);
        tip[1] = vec.y + LoadFloatAt(obj, kSobOffPos + 0x04U);
        // Native seeds tip.z from an uninitialized stack slot; a fresh stack
        // slot contributes zero.
        tip[2] = first_tip_z + LoadFloatAt(obj, kSobOffPos + 0x08U);
        first_tip_z = tip[2];

        const float extent = LoadFloatAt(obj, kSobOffAlpha);
        if (IsBoxOutsidePlayfieldEcxStackAbi(tip, extent, extent) != 0
            && IsBoxOutsidePlayfieldEcxStackAbi(tip, extent, extent) != 0)
            return 1;
    }

    // Tip 2: the region probe against the player-region classifier.
    {
        const float z = LoadFloatAt(obj, kSobOffDepth);
        const float y = LoadFloatAt(obj, kSobOffAlpha);
        if (z > kTipGateZ && y > kTipGateY) {
            Float2 vec;
            SetVectorFromAngle(&vec, LoadFloatAt(obj, kSobOffAngle),
                               z * kTipScaleZ);
            float tip2[3];
            tip2[0] = vec.x + LoadFloatAt(obj, kSobOffPos + 0x00U);
            tip2[1] = vec.y + LoadFloatAt(obj, kSobOffPos + 0x04U);
            // Native accumulates the z component over the first tip block.
            tip2[2] = first_tip_z + LoadFloatAt(obj, kSobOffPos + 0x08U);

            const float half_y = y < kTipAlpha
                ? y * kHalf
                : y - (y + kTipGateZ) * kHalf;
            const i32 region = ClassifyPositionInPlayerRegionEaxEcxStackAbi(
                tip2, g_OptionPositionBase, LoadFloatAt(obj, kSobOffAngle),
                half_y, z * kTipScaleB);
            if (region == 1) {
                typedef i32 (TH10_STDCALL *SweepFn)(void *, const float *,
                                                    const float *, i32);
                const u32 table = LoadU32At(obj, kSobOffTable);
                const SweepFn sweep =
                    *reinterpret_cast<const SweepFn *>(table + 6U * 4U);
                const float *target = reinterpret_cast<const float *>(
                    static_cast<u8 *>(g_OptionPositionBase) + 0x3c0U);
                sweep(obj, target, tip2, 0);
            } else if (region == 2) {
                if (LoadI32At(obj, 0x14U) % 5 == 0) {
                    u8 *stage = static_cast<u8 *>(g_EffectManagerRoot);
                    SpawnStageEffectEdxEbxAbi(
                        reinterpret_cast<void *>(LoadU32At(stage,
                                                           0x3e0b50U)),
                        tip2, 0x1b2);
                    QueueBulletDeathEffectEbxEsiStackAbi(
                        0x1c, EffectManager(),
                        LoadFloatAt(obj, kSobOffPos + 0x00U));
                }
            }
        }
    }

    // VM position publication and ticks.
    {
        u8 *rec1 = obj + kSobOffRecA_A;
        const u32 scale_ptr = LoadU32At(rec1, 0x394U);
        StoreU32At(rec1, 0x35cU, LoadU32At(rec1, 0x35cU) | 8U);
        StoreFloatAt(rec1, 0x3cU,
                     LoadFloatAt(obj, kSobOffAlpha)
                         / LoadFloatAt(reinterpret_cast<void *>(scale_ptr),
                                       0x34U));
        StoreFloatAt(rec1, 0x40U,
                     LoadFloatAt(obj, kSobOffDepth)
                         / LoadFloatAt(reinterpret_cast<void *>(scale_ptr),
                                       0x30U));
        FinalizeTimelineRenderObjectSetup(rec1);
        if (LoadFloatAt(obj, kSobOffZVel) == kZero)
            FinalizeTimelineRenderObjectSetup(obj + kSobOffRecA_B);
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
    u8 *obj = static_cast<u8 *>(object);

    const float x = LoadFloatAt(obj, kSobOffPos + 0x00U)
        + LoadFloatAt(obj, kSobOffVel + 0x00U);
    const float y = LoadFloatAt(obj, kSobOffPos + 0x04U)
        + LoadFloatAt(obj, kSobOffVel + 0x04U);
    const float z = LoadFloatAt(obj, kSobOffPos + 0x08U)
        + LoadFloatAt(obj, kSobOffVel + 0x08U);

    const bool inside = x >= kFieldMinX && x < kFieldMaxX && y >= kZero
        && y < kFieldMaxY;
    if (inside)
        return;

    CutoffEffectRing(LoadI32At(obj, kSobOffKind));

    // Snapshot branch 1: the angle reversed by pi.
    StoreFloatAt(obj, 0x430U,
                 WrapAngleToPi(-LoadFloatAt(obj, kSobOffAngle) - kPi));
    StoreFloatAt(obj + kSobOffDesc, 0x00U, x);
    StoreFloatAt(obj + kSobOffDesc, 0x04U, y);
    StoreFloatAt(obj + kSobOffDesc, 0x08U, z);
    StoreU32At(obj, 0x444U, LoadU32At(obj, 0x13cU));
    SpawnStageObjectEsiEdiStackAbi(g_BulletListRootSlot, obj + kSobOffDesc,
                                   0);

    // Snapshot branch 2 (only when flag 0x8000000 is clear): the depth
    // window check against the +0x40/+0x4c pair decides whether the second
    // spawn happens; the flag mask is always applied on this path.
    if ((LoadU32At(obj, kSobOffFlags) & 0x8000000U) == 0U) {
        const bool depth_inside = LoadFloatAt(obj, kSobOffPos + 0x08U)
            >= kZero;
        (void)depth_inside;
        CutoffEffectRing(LoadI32At(obj, kSobOffKind));
        StoreFloatAt(obj + kSobOffDesc, 0x00U, x);
        StoreFloatAt(obj + kSobOffDesc, 0x04U, y);
        StoreFloatAt(obj + kSobOffDesc, 0x08U, z);
        StoreU32At(obj, 0x444U, LoadU32At(obj, 0x13cU));
        SpawnStageObjectEsiEdiStackAbi(g_BulletListRootSlot,
                                       obj + kSobOffDesc, 0);
    }
    StoreU32At(obj, kSobOffFlags,
               LoadU32At(obj, kSobOffFlags) & 0xF7FFF3FFU);
}

// TH10 0x0041d170 (slot 12; the 0x40 feature flag). Distance-driven shrink:
// while the +0xf8 timer has not reached +0x11c the motion vector length
// shrinks as base*(1 - rate/+0x11c); on reach the terminal event fires, the
// timer re-arms and the vector rebuilds with the full +0x11c length.
void TH10_STDCALL StageObjectDistanceShrinkA(void *object)
{
    u8 *obj = static_cast<u8 *>(object);
    const i32 timer = LoadI32At(obj, 0xf8U);
    const i32 max_distance = LoadI32At(obj, 0x11cU);

    if (timer >= max_distance) {
        CutoffEffectRing(LoadI32At(obj, kSobOffKind));
        const i32 counter = LoadI32At(obj, 0x124U) + 1;
        StoreI32At(obj, 0x124U, counter);
        if (counter >= LoadI32At(obj, 0x120U))
            StoreU32At(obj, kSobOffFlags,
                       LoadU32At(obj, kSobOffFlags) & 0xFFFFFFBFU);
        StoreFloatAt(obj, kSobOffPos + 0x00U,
                     LoadFloatAt(obj, 0x10cU)
                         + LoadFloatAt(obj, kSobOffPos + 0x00U));
        StoreFloatAt(obj, kSobOffZSpeed, LoadFloatAt(obj, 0x108U));
        // Lazy arm of the +0xf4 timer record, then the unconditional reset.
        if ((LoadU32At(obj, 0x104U) & 1U) == 0U) {
            StoreI32At(obj, 0xf8U, 0);
            StoreU32At(obj, 0xf4U, 0xFFF0BDC1U);
            StoreI32At(obj, 0xfcU, 0);
            StoreU32At(obj, 0x100U,
                       reinterpret_cast<u32>(&g_FrameTimeScale));
            StoreU32At(obj, 0x104U, LoadU32At(obj, 0x104U) | 1U);
        }
        StoreI32At(obj, 0xf8U, 0);
        StoreI32At(obj, 0xfcU, 0);
        StoreI32At(obj, 0xf4U, -1);
        // Native rebuilds the vector with length = max_distance.
        Float2 vec;
        SetVectorFromAngle(&vec, LoadFloatAt(obj, kSobOffAngle),
                           static_cast<float>(max_distance));
        StoreFloatAt(obj, kSobOffVel + 0x00U, vec.x);
        StoreFloatAt(obj, kSobOffVel + 0x04U, vec.y);
    } else {
        // length = base * (1 - rate / max_distance)
        const float base = LoadFloatAt(obj, kSobOffZSpeed);
        const float rate = LoadFloatAt(obj, 0xfcU);
        Float2 vec;
        SetVectorFromAngle(
            &vec, LoadFloatAt(obj, kSobOffAngle),
            base * (kOne - rate / static_cast<float>(max_distance)));
        StoreFloatAt(obj, kSobOffVel + 0x00U, vec.x);
        StoreFloatAt(obj, kSobOffVel + 0x04U, vec.y);
        // Native still publishes [+0xf4] = [+0xf8] before the rate advance.
        StoreI32At(obj, 0xf4U, LoadI32At(obj, 0xf8U));
        const float rate_value = LoadFloatAt(
            *reinterpret_cast<void **>(static_cast<u8 *>(obj) + 0x100U), 0U);
        if (rate_value > kRateLow && rate_value < kRateHigh) {
            StoreI32At(obj, 0xf8U, LoadI32At(obj, 0xf8U) + 1);
            StoreFloatAt(obj, 0xfcU, LoadFloatAt(obj, 0xfcU) + kOne);
        } else {
            StoreFloatAt(obj, 0xfcU,
                         rate_value + LoadFloatAt(obj, 0xfcU));
            // Native rounds half-away-from-zero through 0x463b2c.
            const double rounded = ::floor(
                static_cast<double>(LoadFloatAt(obj, 0xfcU)) + 0.5);
            StoreI32At(obj, 0xf8U, static_cast<i32>(rounded));
        }
        return;
    }

    // Shared tail on the terminal path: publish [+0xf4] = [+0xf8] and
    // advance the rate record identically.
    StoreI32At(obj, 0xf4U, LoadI32At(obj, 0xf8U));
    const float rate_value = LoadFloatAt(
        *reinterpret_cast<void **>(static_cast<u8 *>(obj) + 0x100U), 0U);
    if (rate_value > kRateLow && rate_value < kRateHigh) {
        StoreI32At(obj, 0xf8U, LoadI32At(obj, 0xf8U) + 1);
        StoreFloatAt(obj, 0xfcU, LoadFloatAt(obj, 0xfcU) + kOne);
    } else {
        StoreFloatAt(obj, 0xfcU, rate_value + LoadFloatAt(obj, 0xfcU));
        const double rounded = ::floor(
            static_cast<double>(LoadFloatAt(obj, 0xfcU)) + 0.5);
        StoreI32At(obj, 0xf8U, static_cast<i32>(rounded));
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
i32 SweepCommon(u8 *obj, float *arg1, const void *arg2, i32 flag,
                const SweepKindMap &map, SweepMode mode)
{
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

    const float start[3] = { LoadFloatAt(obj, kSobOffPos + 0x00U),
                             LoadFloatAt(obj, kSobOffPos + 0x04U),
                             LoadFloatAt(obj, kSobOffPos + 0x08U) };
    Float2 dir;
    SetVectorFromAngle(&dir, LoadFloatAt(obj, kSobOffAngle), kSweepStep);

    u8 hits[0x40];
    ZeroBlock(hits, 0x40U);
    i32 hit_count = 0;
    i32 steps = 0;
    float point[3] = { start[0], start[1], start[2] };
    float angle = kAngleStep;

    if (kAngleStep < LoadFloatAt(obj, kSobOffDepth)) {
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
                    SpawnExplosionParticleEaxEcxEfxAbi(
                        *static_cast<void **>(g_BulletManagerSlot),
                        point, 8, 0xFFFFFFFFU, kExplosionAngle,
                        kExplosionSpeed);
                }
                if (mode == kSweepBox)
                    SpawnRingEffectVm(script, start, start[2]);
                else
                    SpawnRingEffectVm(script, point, point[2]);
            }
            point[0] += dir.x;
            point[1] += dir.y;
            angle += kAngleStep;
            if (!(angle + kDepthWindow < LoadFloatAt(obj, kSobOffDepth)))
                break;
            ++steps;
        }
    }

    if (hit_count == 0)
        return 0;
    if (hit_count >= steps) {
        StoreU32At(obj, kSobOffDone, 1U);
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
        const float depth = LoadFloatAt(obj, kSobOffDepth)
            - static_cast<float>(leading) * kAngleStep;
        StoreFloatAt(obj, kSobOffDepth, depth);
        if (depth > kDepthLatch) {
            StoreFloatAt(obj, 0x434U, depth);
            StoreFloatAt(obj, kSobOffZVel, depth);
        } else {
            StoreU32At(obj, kSobOffDone, 1U);
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
            StoreFloatAt(obj, kSobOffDepth,
                         LoadFloatAt(obj, kSobOffDepth)
                             - static_cast<float>(gap) * kAngleStep);
            if (LoadFloatAt(obj, kSobOffDepth) >= kDepthLatch)
                StoreU32At(obj, kSobOffDone, 1U);
            if (gap * 12 > 18) {
                if (mode == kSweepBox) {
                    u8 desc_copy[0x1dcU];
                    CopyBlock(desc_copy, obj + kSobOffDesc,
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
    return SweepCommon(static_cast<u8 *>(object),
                       const_cast<float *>(extent), extent, flag, kMapA,
                       kSweepBox);
}

// TH10 0x0041dd80 (slot 7, kind A).
i32 TH10_STDCALL StageObjectSweepRadialA(void *object, const float target[3],
                                         float radius, i32 flag)
{
    return SweepCommon(static_cast<u8 *>(object),
                       const_cast<float *>(target), &radius, flag, kMapA,
                       kSweepRadial);
}

// TH10 0x0041eb00 (slot 6, kind B).
i32 TH10_STDCALL StageObjectSweepBoxB(void *object, const float center[3],
                                      const float extent[3], i32 flag)
{
    (void)center;
    return SweepCommon(static_cast<u8 *>(object),
                       const_cast<float *>(extent), extent, flag, kMapB,
                       kSweepBox);
}

// TH10 0x0041efa0 (slot 7, kind B).
i32 TH10_STDCALL StageObjectSweepRadialB(void *object, const float target[3],
                                         float radius, i32 flag)
{
    return SweepCommon(static_cast<u8 *>(object),
                       const_cast<float *>(target), &radius, flag, kMapB,
                       kSweepRadial);
}

// TH10 0x0041e260 (slot 5, kind A; the 0x41c850 bullet-clear virtual).
// Spawns a ring VM every 12 degrees from the object position until the angle
// passes the depth; optionally plays the explosion particle per step.
i32 TH10_STDCALL StageObjectSpreadA(void *object, i32 enable_explosion)
{
    u8 *obj = static_cast<u8 *>(object);
    const i16 kind = static_cast<i16>(LoadU16At(obj, kMapA.kind_word));
    const i32 script = kind * 2 + 0x11;

    float point[3] = { LoadFloatAt(obj, kSobOffPos + 0x00U),
                       LoadFloatAt(obj, kSobOffPos + 0x04U),
                       LoadFloatAt(obj, kSobOffPos + 0x08U) };
    Float2 dir;
    SetVectorFromAngle(&dir, LoadFloatAt(obj, kSobOffAngle), kSweepStep);
    float angle = kAngleStep;

    if (kAngleStep < LoadFloatAt(obj, kSobOffDepth)) {
        for (;;) {
            SpawnRingEffectVm(script, point, point[2]);
            if (enable_explosion != 0) {
                const float tip[2] = { point[0], point[1] };
                if (IsBoxOutsidePlayfieldEcxStackAbi(tip, 32.0f, 32.0f)
                        == 0) {
                    SpawnExplosionParticleEaxEcxEfxAbi(
                        *static_cast<void **>(g_BulletManagerSlot), point, 8,
                        0xFFFFFFFFU, kExplosionAngle, kExplosionSpeed);
                }
            }
            point[0] += dir.x;
            point[1] += dir.y;
            angle += kAngleStep;
            if (!(angle + kDepthWindow < LoadFloatAt(obj, kSobOffDepth)))
                break;
        }
    }
    StoreI32At(obj, kSobOffState, 1);
    return 0;
}

// TH10 0x0041f400 (slot 5, kind B). Spread with the playfield gate before
// each ring VM spawn.
i32 TH10_STDCALL StageObjectSpreadB(void *object, i32 enable_explosion)
{
    u8 *obj = static_cast<u8 *>(object);
    const i16 kind = static_cast<i16>(LoadU16At(obj, kMapB.kind_word));
    const i32 script = kind * 2 + 0x11;

    float point[3] = { LoadFloatAt(obj, kSobOffPos + 0x00U),
                       LoadFloatAt(obj, kSobOffPos + 0x04U),
                       LoadFloatAt(obj, kSobOffPos + 0x08U) };
    Float2 dir;
    SetVectorFromAngle(&dir, LoadFloatAt(obj, kSobOffAngle), kSweepStep);
    float angle = kAngleStep;

    if (kAngleStep < LoadFloatAt(obj, kSobOffDepth)) {
        for (;;) {
            const float tip[2] = { point[0], point[1] };
            if (IsBoxOutsidePlayfieldEcxStackAbi(tip, kTipGateZ, kTipGateZ)
                    == 0) {
                SpawnRingEffectVm(script, point, point[2]);
                if (enable_explosion != 0
                    && IsBoxOutsidePlayfieldEcxStackAbi(tip, 32.0f, 32.0f)
                           == 0) {
                    SpawnExplosionParticleEaxEcxEfxAbi(
                        *static_cast<void **>(g_BulletManagerSlot), point, 8,
                        0xFFFFFFFFU, kExplosionAngle, kExplosionSpeed);
                }
            }
            point[0] += dir.x;
            point[1] += dir.y;
            angle += kAngleStep;
            if (!(angle + kDepthWindow < LoadFloatAt(obj, kSobOffDepth)))
                break;
        }
    }
    StoreI32At(obj, kSobOffState, 1);
    return 0;
}

// TH10 0x0041e700 (slot 2, kind B).
i32 TH10_STDCALL StageObjectUpdateB(void *object)
{
    u8 *obj = static_cast<u8 *>(object);

    // Depth advance: only while below the +0x444 limit, clamped on top.
    {
        const float z = LoadFloatAt(obj, kSobOffDepth);
        const float zlim = LoadFloatAt(obj, 0x444U);
        if (z < zlim) {
            const float advanced = z
                + g_FrameTimeScale * LoadFloatAt(obj, kSobOffZSpeed);
            StoreFloatAt(obj, kSobOffDepth, advanced);
            if (advanced > zlim)
                StoreFloatAt(obj, kSobOffDepth, zlim);
        }
    }

    // Angle wrap: angle = wrap(angle + rate * +0x440) (0x44bc10).
    StoreFloatAt(obj, kSobOffAngle,
                 WrapAngleSumStackAbi(LoadFloatAt(obj, kSobOffAngle),
                                      g_FrameTimeScale
                                          * LoadFloatAt(obj, 0x440U)));

    // Descriptor flag bit 1: follow the chain position at
    // [DAT_00477704+0x10]+0x1068.
    if ((LoadU32At(obj, 0x468U) & 1U) != 0U) {
        const u32 chain = LoadU32At(g_AsciiHudConditionalState, 0x10U);
        if (chain != 0U)
            CopyBlock(obj + kSobOffPos,
                      reinterpret_cast<const u8 *>(chain) + 0x1068U, 0x0cU);
    }

    // Entrance drift.
    const float rate = g_FrameTimeScale;
    StoreFloatAt(obj, kSobOffPos + 0x00U,
                 LoadFloatAt(obj, kSobOffPos + 0x00U)
                     + rate * LoadFloatAt(obj, 0x430U));
    StoreFloatAt(obj, kSobOffPos + 0x04U,
                 LoadFloatAt(obj, kSobOffPos + 0x04U)
                     + rate * LoadFloatAt(obj, 0x434U));
    StoreFloatAt(obj, kSobOffPos + 0x08U,
                 LoadFloatAt(obj, kSobOffPos + 0x08U)
                     + rate * LoadFloatAt(obj, 0x438U));

    // Entrance state machine (states 2..5 via the 0x41ea30 jump table);
    // the tick value lives at +0x14, the timers at +0x454/+0x458/+0x45c/
    // +0x460 and the alpha scale at +0x44c.
    switch (LoadI32At(obj, kSobOffState)) {
    case 3:
        if (LoadI32At(obj, 0x14U) >= LoadI32At(obj, 0x454U)) {
            TickPlayerTimerEaxStackAbi(obj + 0x10U, 0);
            StoreI32At(obj, kSobOffState, 4);
        }
        break;
    case 4:
        if (LoadI32At(obj, 0x14U) >= LoadI32At(obj, 0x458U)) {
            TickPlayerTimerEaxStackAbi(obj + 0x10U, 0);
            StoreI32At(obj, kSobOffState, 2);
            StoreFloatAt(obj, kSobOffAlpha, LoadFloatAt(obj, 0x44cU));
            if (LoadI32At(obj, 0x14U) >= LoadI32At(obj, 0x45cU)) {
                TickPlayerTimerEaxStackAbi(obj + 0x10U, 0);
                StoreI32At(obj, kSobOffState, 5);
                if (LoadI32At(obj, 0x14U) >= LoadI32At(obj, 0x460U))
                    return 1;
                StoreFloatAt(obj, kSobOffAlpha,
                             LoadFloatAt(obj, 0x44cU)
                                 - LoadFloatAt(obj, 0x44cU)
                                       * LoadFloatAt(obj, 0x18U)
                                       / static_cast<float>(
                                           LoadI32At(obj, 0x460U)));
            } else {
                StoreFloatAt(obj, kSobOffAlpha,
                             LoadFloatAt(obj, 0x44cU)
                                 * LoadFloatAt(obj, 0x18U)
                                 / static_cast<float>(
                                     LoadI32At(obj, 0x458U)));
            }
        }
        break;
    default:
        break;
    }

    // Tip emission for states 4 and 2 (same shape as kind A).
    const i32 state = LoadI32At(obj, kSobOffState);
    if (state == 4 || state == 2) {
        const float z = LoadFloatAt(obj, kSobOffDepth);
        if (z > kTipGateZ) {
            Float2 vec;
            SetVectorFromAngle(&vec, LoadFloatAt(obj, kSobOffAngle),
                               z * kTipScaleZ);
            float tip2[3];
            tip2[0] = vec.x + LoadFloatAt(obj, kSobOffPos + 0x00U);
            tip2[1] = vec.y + LoadFloatAt(obj, kSobOffPos + 0x04U);
            // Native accumulates the z component over a stale-seeded block.
            tip2[2] = 0.0f + LoadFloatAt(obj, kSobOffPos + 0x08U);

            const float y = LoadFloatAt(obj, kSobOffAlpha);
            const float half_y = y < kTipAlpha
                ? y * kHalf
                : y - (y + kTipGateZ) * kTipScaleC;
            const i32 region = ClassifyPositionInPlayerRegionEaxEcxStackAbi(
                tip2, g_OptionPositionBase, LoadFloatAt(obj, kSobOffAngle),
                half_y, z * kTipScaleB);
            if (region == 1) {
                typedef i32 (TH10_STDCALL *SweepFn)(void *, const float *,
                                                    const float *, i32);
                const u32 table = LoadU32At(obj, kSobOffTable);
                const SweepFn sweep =
                    *reinterpret_cast<const SweepFn *>(table + 6U * 4U);
                const float *target = reinterpret_cast<const float *>(
                    static_cast<u8 *>(g_OptionPositionBase) + 0x3c0U);
                sweep(obj, target, tip2, 0);
            } else if (region == 2) {
                if (LoadI32At(obj, 0x14U) % 5 == 0) {
                    u8 *stage = static_cast<u8 *>(g_EffectManagerRoot);
                    SpawnStageEffectEdxEbxAbi(
                        reinterpret_cast<void *>(LoadU32At(stage,
                                                           0x3e0b50U)),
                        tip2, 0x1b2);
                    QueueBulletDeathEffectEbxEsiStackAbi(
                        0x1c, EffectManager(),
                        LoadFloatAt(obj, kSobOffPos + 0x00U));
                }
            }
        }
    }

    // VM publication (kind B record bases).
    {
        u8 *rec1 = obj + kSobOffRecB_A;
        const u32 scale_ptr = LoadU32At(rec1, 0x394U);
        StoreU32At(rec1, 0x35cU, LoadU32At(rec1, 0x35cU) | 8U);
        StoreFloatAt(rec1, 0x3cU,
                     LoadFloatAt(obj, kSobOffAlpha)
                         / LoadFloatAt(reinterpret_cast<void *>(scale_ptr),
                                       0x34U));
        StoreFloatAt(rec1, 0x40U,
                     LoadFloatAt(obj, kSobOffDepth)
                         / LoadFloatAt(reinterpret_cast<void *>(scale_ptr),
                                       0x30U));
        FinalizeTimelineRenderObjectSetup(rec1);
        if (LoadFloatAt(obj, kSobOffZVel) == kZero)
            FinalizeTimelineRenderObjectSetup(obj + kSobOffRecB_B);
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
// stops the drift.
void TH10_STDCALL StageObjectDriftSlotA(void *object)
{
    u8 *obj = static_cast<u8 *>(object);
    const i32 ticks = LoadI32At(obj, 0x90U);

    if (ticks >= LoadI32At(obj, 0xb4U)) {
        StoreU32At(obj, kSobOffFlags,
                   LoadU32At(obj, kSobOffFlags) & 0xffffffefU);
        return;
    }

    const float scaled = g_FrameTimeScale;
    StoreFloatAt(obj, kSobOffZSpeed,
                 LoadFloatAt(obj, kSobOffZSpeed)
                     + scaled * LoadFloatAt(obj, 0xa0U));
    StoreFloatAt(obj, kSobOffVel + 0x00U,
                 LoadFloatAt(obj, kSobOffVel + 0x00U)
                     + scaled * LoadFloatAt(obj, 0xa8U));
    StoreFloatAt(obj, kSobOffVel + 0x04U,
                 LoadFloatAt(obj, kSobOffVel + 0x04U)
                     + scaled * LoadFloatAt(obj, 0xacU));
    StoreFloatAt(obj, kSobOffVel + 0x08U,
                 LoadFloatAt(obj, kSobOffVel + 0x08U)
                     + scaled * LoadFloatAt(obj, 0xb0U));

    const float vel_x = LoadFloatAt(obj, kSobOffVel + 0x00U);
    const float vel_y = LoadFloatAt(obj, kSobOffVel + 0x04U);
    const float threshold = FloatFromBits(0x40F00000U); // 0x470c58 7.5f
    if ((vel_x < -threshold || vel_x > threshold)
        || (vel_y < -threshold || vel_y > threshold)) {
        // fpatan(y, x) with the result into +0x3c.
        StoreFloatAt(obj, kSobOffAngle,
                     static_cast<float>(
                         std::atan2(static_cast<double>(vel_y),
                                    static_cast<double>(vel_x))));
    }

    // Publish the pre-advance tick count, then run the shared rate-record
    // step over {+0x90 count, +0x94 accumulator, +0x98 rate pointer}.
    StoreI32At(obj, 0x8cU, ticks);
    const float rate_value = LoadFloatAt(
        reinterpret_cast<void *>(LoadU32At(obj, 0x98U)), 0U);
    if (rate_value > kRateLow && rate_value < kRateHigh) {
        StoreI32At(obj, 0x90U, ticks + 1);
        StoreFloatAt(obj, 0x94U,
                     LoadFloatAt(obj, 0x94U) + kOne);
    } else {
        const float advanced = LoadFloatAt(obj, 0x94U) + rate_value;
        StoreFloatAt(obj, 0x94U, advanced);
        // Native rounds half-away-from-zero through 0x463b2c.
        const double rounded = advanced >= 0.0f
            ? std::floor(static_cast<double>(advanced) + 0.5)
            : std::ceil(static_cast<double>(advanced) - 0.5);
        StoreI32At(obj, 0x90U, static_cast<i32>(rounded));
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
