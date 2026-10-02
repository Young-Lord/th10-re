// Semantic reconstruction of TH10 0x0040a1a0, the composite destructor of
// the 0x688-byte stage-host object. The native carries an SEH frame (scope
// handler 0x004657e9) whose unwind path is not exercised by any caller; the
// reconstruction follows the established project convention of omitting the
// exception frame.
//
// Callers: 0x0040a3c0 (factory failure path, then frees the host),
// 0x0040a410 (scalar deleting destructor: bit 0 of the stack argument
// requests the outer free), and 0x0040a430 (guarded delete helper).

#include "StageHostTeardown.hpp"

#include "CallbackScheduler.hpp"
#include "ManagerReleaseWrappers.hpp"
#include "StageHostObject.hpp"
#include "ThreadControl.hpp"

namespace th10 {

namespace {

extern void ReleaseResourceBuffer(void *pointer);   // TH10 0x00452422
extern CallbackScheduler *g_CallbackScheduler;      // TH10 DAT_00491be4

// Published manager globals torn down here.
extern void *g_AsciiHudOwner;            // TH10 DAT_0047770c
extern void *g_PlayerStateBlock;         // TH10 DAT_00477834
extern void *g_EffectManagerRoot;        // TH10 DAT_004776f0
extern void *g_AsciiHudConditionalState; // TH10 DAT_00477704
extern void *g_StageNode;                // TH10 DAT_004776ec (pointer holder)
extern void *g_BulletManagerSlot;        // TH10 DAT_00477818
extern void *g_TextEffectOwnerSlot;      // TH10 DAT_00477840
extern void *g_StageHostObject;          // TH10 DAT_004776f8 (this host)

u32 LoadU32From(const void *address)
{
    const u8 *const bytes = static_cast<const u8 *>(address);
    return static_cast<u32>(bytes[0]) | (static_cast<u32>(bytes[1]) << 8)
         | (static_cast<u32>(bytes[2]) << 16)
         | (static_cast<u32>(bytes[3]) << 24);
}

void StoreU32To(void *address, u32 value)
{
    u8 *const bytes = static_cast<u8 *>(address);
    bytes[0] = static_cast<u8>(value);
    bytes[1] = static_cast<u8>(value >> 8);
    bytes[2] = static_cast<u8>(value >> 16);
    bytes[3] = static_cast<u8>(value >> 24);
}

// Native idiom around each scheduler record (host.calc_element +0x8 /
// host.draw_element +0xc): under the scheduler critical section
// (DAT_00492274) with the DAT_0049231c activity byte incremented, the
// shared removal 0x00449f60 unlinks the record and frees scheduler-owned
// records. RemoveSynchronized models exactly that.
void RemoveHostSchedulerRecord(ChainElem *record)
{
    if (record == 0)
        return;
    CallbackSchedulerApi::RemoveSynchronized(g_CallbackScheduler, record);
}

} // namespace

// TH10 0x0040a1a0. Stack argument = host, ret 4.
void DestroyStageHostObjectStackAbi(void *host)
{
    StageHostObject *const host_view =
        static_cast<StageHostObject *>(host);
    u8 *const host_bytes = static_cast<u8 *>(host);

    // The host doubles as the ECL select menu for name-table purposes:
    // native ESI = the host for 0x00409eb0. That release (defined in
    // ManagerReleaseWrappers.cpp) frees every stage_table[i] string
    // (+0x34 array, +0x38 count) and then the array itself.
    ReleaseEclSelectMenuNamesEsiAbi(host);

    RemoveHostSchedulerRecord(host_view->calc_element);
    RemoveHostSchedulerRecord(host_view->draw_element);

    // Destroy-and-free every published manager through the shared
    // "in-place destructor, then shared delete" wrappers (native pairs
    // 0x004145f0/0x00424ed0/0x00405f70/0x0040d530/0x00405620/0x0041adf0/
    // 0x0042b570). Each destructor clears its own global.
    ReleaseAsciiHudOwnerEsiAbi(g_AsciiHudOwner);
    ReleasePlayerStateBlockEsiAbi(g_PlayerStateBlock);
    ReleaseEffectManagerRootEsiAbi(g_EffectManagerRoot);
    ReleaseAsciiHudConditionalStateEsiAbi(g_AsciiHudConditionalState);
    ReleaseGameContextEsiAbi(g_StageNode);
    ReleaseBulletManagerEsiAbi(g_BulletManagerSlot);
    ReleaseMainChainObject840EsiAbi(g_TextEffectOwnerSlot);

    // +0x620 owned buffer (DELIBERATE RAW: this slot lies INSIDE the
    // embedded animation_vm record at +0x2c8, aliasing VmRecord+0x358, so
    // StageHostObject carries no typed field for it): the published host
    // global is cleared first, then the buffer is released through the CRT
    // free and the slot cleared.
    g_StageHostObject = 0; // DAT_004776f8
    const u32 buffer = LoadU32From(host_bytes + 0x620U);
    if (buffer != 0U)
        ReleaseResourceBuffer(reinterpret_cast<void *>(buffer));
    StoreU32To(host_bytes + 0x620U, 0U);

    // Stop the embedded thread control block: plant the ThreadControl
    // vtable 0x4703e4 first, exactly as the native destructor does.
    host_view->thread_control.marker =
        reinterpret_cast<void *>(0x004703e4U);
    StopThreadControl(&host_view->thread_control);
}

} // namespace th10
