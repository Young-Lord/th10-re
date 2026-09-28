// TH10 title-screen VM slot lifecycle (0x402050 / 0x4020b0 / 0x402160 /
// 0x402640).
//
// The 0x2b64-byte title VM host carries two pools of 0x3ac-byte VM records
// (8 at +0x180, 3 at +0x1f08) managed through the MSVC eh vector
// constructor iterator (0x45252d; scalar ctor 0x402050, dtor 0x401ff0).

#include "Th10Types.hpp"
#include "TitleScreenStateCtor.hpp"

namespace th10 {

// TH10 0x00402440 (defined in TitleGameManagerLifecycle.cpp).
void DestroyTitleScreenStateBufferInPlace(void *object);

namespace {

const u32 kFlagBit1Clear = ~2u;

void ClearRecordBusyFlags(u8 *record) {
    // The native clears bit 1 of nine scattered flag dwords before the
    // full wipe (dead stores, preserved as the native quirk they are).
    static const u32 kFlagOffsets[9] = {
        0x6c, 0xb0, 0xfc, 0x128, 0x174, 0x1b0, 0x1fc, 0x228, 0x378};
    for (int i = 0; i < 9; ++i) {
        u32 *flag = reinterpret_cast<u32 *>(record + kFlagOffsets[i]);
        *flag &= kFlagBit1Clear;
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
    *reinterpret_cast<u16 *>(bytes + 0x384) = 0xffff;
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
    u8 *bytes = static_cast<u8 *>(host);
    static const u32 kHostFlagOffsets[5] = {0x34, 0x48, 0x90, 0xdc, 0x168};
    for (int i = 0; i < 5; ++i) {
        u32 *flag = reinterpret_cast<u32 *>(bytes + kHostFlagOffsets[i]);
        *flag &= kFlagBit1Clear;
    }

    // eh vector constructor iterator over the two record pools. The native
    // registers dtor 0x401ff0 / ctor 0x402050 with 0x45252d; the semantic
    // equivalent runs the scalar init over every record slot.
    void *pool_a = bytes + 0x180;
    for (int i = 0; i < 8; ++i) {
        InitTitleScreenVmRecordEcxAbi(
            static_cast<u8 *>(pool_a) + i * 0x3ac);
    }
    void *pool_b = bytes + 0x1f08;
    for (int i = 0; i < 3; ++i) {
        InitTitleScreenVmRecordEcxAbi(
            static_cast<u8 *>(pool_b) + i * 0x3ac);
    }

    *reinterpret_cast<u32 *>(bytes + 0x2a2c) &= kFlagBit1Clear;

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
