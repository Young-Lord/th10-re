// TH10 title-screen VM slot lifecycle (0x402050 / 0x4020b0 / 0x402160 /
// 0x402640).
//
// The 0x2b64-byte title VM host carries two pools of 0x3ac-byte VM records
// (8 at +0x180, 3 at +0x1f08) managed through the MSVC eh vector
// constructor iterator (0x45252d; scalar ctor 0x402050, dtor 0x401ff0).

#include "Th10Types.hpp"
#include "TitleScreenState.hpp"
#include "TitleScreenStateCtor.hpp"
#include "VmRecord.hpp"

namespace th10 {

// TH10 0x00402440 (defined in TitleGameManagerLifecycle.cpp).
void DestroyTitleScreenStateBufferInPlace(void *object);

namespace {

const u32 kFlagBit1Clear = ~2u;

void ClearRecordBusyFlags(u8 *record) {
    // The native clears bit 1 of nine scattered flag dwords before the
    // full wipe (dead stores, preserved as the native quirk they are).
    VmRecord &vm = *reinterpret_cast<VmRecord *>(record);
    u32 *const flag_slots[9] = {
        &vm.timer_flags,         // 0x6c
        &vm.position_anim.flags, // 0xb0
        &vm.rgb_anim_1.flags,    // 0xfc
        &vm.alpha_anim_1.flags,  // 0x128
        &vm.rotation_anim.flags, // 0x174
        &vm.scale_anim.flags,    // 0x1b0
        &vm.rgb_anim_2.flags,    // 0x1fc
        &vm.alpha_anim_2.flags,  // 0x228
        &vm.saved_timer_flags};  // 0x378
    for (int i = 0; i < 9; ++i) {
        *flag_slots[i] &= kFlagBit1Clear;
    }
}

} // namespace

// TH10 0x00402050. Native __thiscall ECX = one 0x3ac-byte VM record; the
// eh vector constructor's scalar callback. Clears the nine busy-flag
// dwords, wipes the whole 0xeb-dword record, then stores the 0xffff
// sprite-index sentinel at +0x384 and returns the record.
void *InitTitleScreenVmRecordEcxAbi(void *record) {
    u8 *bytes = static_cast<u8 *>(record);
    ClearRecordBusyFlags(bytes);
    u32 *wipe = reinterpret_cast<u32 *>(bytes);
    for (int i = 0; i < 0xeb; ++i) {
        wipe[i] = 0;
    }
    reinterpret_cast<VmRecord *>(bytes)->sprite_entry_id = 0xffff;
    return record;
}

// TH10 0x004020b0. Native EAX = record. Clears only the nine busy-flag
// dwords (used when a record is recycled without a full reset).
void ClearTitleScreenVmRecordFlagsEaxAbi(void *record) {
    ClearRecordBusyFlags(static_cast<u8 *>(record));
}

// TH10 0x00402160. Native stdcall, one stack argument = the 0x2b64-byte
// host (ret 4). Clears the busy flags on the host's own scattered flag
// dwords, constructs both eh vector record pools (8 x 0x3ac at +0x180,
// 3 x 0x3ac at +0x1f08), clears the +0x2a2c flag, wipes the whole
// 0xad9-dword host (native order quirk: the wipe happens after the flag
// clears and the pool construction), sets bit 1 of the first dword and
// returns the host.
void *InitTitleScreenVmHostStackAbi(void *host) {
    TitleScreenState &state = *reinterpret_cast<TitleScreenState *>(host);
    u8 *bytes = static_cast<u8 *>(host);
    // Bit-1 clears on the timer flag dwords; the +0x34 slot belongs to the
    // unnamed wave-mode timer block at +0x24 and stays a raw offset.
    *reinterpret_cast<u32 *>(bytes + 0x34) &= kFlagBit1Clear;
    state.wait_timer.flags &= kFlagBit1Clear;        // +0x48
    state.interp_a_timer.flags &= kFlagBit1Clear;    // +0x90
    state.interp_b_timer.flags &= kFlagBit1Clear;    // +0xdc
    state.color_track_timer.flags &= kFlagBit1Clear; // +0x168

    // eh vector constructor iterator over the two record pools. The native
    // registers dtor 0x401ff0 / ctor 0x402050 with 0x45252d; the semantic
    // equivalent runs the scalar init over every record slot.
    for (int i = 0; i < 8; ++i) {
        InitTitleScreenVmRecordEcxAbi(&state.background_vms[i]);
    }
    for (int i = 0; i < 3; ++i) {
        InitTitleScreenVmRecordEcxAbi(&state.aux_vms[i]);
    }

    state.score_anim_timer.flags &= kFlagBit1Clear; // +0x2a2c

    u32 *wipe = reinterpret_cast<u32 *>(bytes);
    for (int i = 0; i < 0xad9; ++i) {
        wipe[i] = 0;
    }
    *reinterpret_cast<u32 *>(bytes) |= 2u;
    return host;
}

// TH10 0x00402640. Native stdcall (stack: stage-data name, priority base;
// ret 8). operator new(0x2b64) + InitTitleScreenVmHostStackAbi, then
// CreateTitleScreenStateEaxEcxStackAbi(host, name, priority). On a nonzero
// result (or a null allocation) the host is destroyed in place, freed and
// null is returned; otherwise the host is returned. The native runs the
// state constructor even on a null host (quirk preserved by the same call
// order here).
void *CreateTitleScreenVmHostStackAbi(const char *stage_data_name,
                                      u32 priority_base) {
    void *host = ::operator new(0x2b64U);
    if (host != 0) {
        InitTitleScreenVmHostStackAbi(host);
    }
    const i32 result =
        CreateTitleScreenStateEaxEcxStackAbi(host, stage_data_name,
                                             priority_base);
    if (result == 0) {
        return host;
    }
    if (host != 0) {
        DestroyTitleScreenStateBufferInPlace(host);
        ::operator delete(host);
    }
    return 0;
}

} // namespace th10
