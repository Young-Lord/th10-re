// TH10 stage conditional state (DAT_00477704) creation/destruction
// helpers and the inline "ANIM"/"ECLI" chunk parser feeding it.

#include "Th10Types.hpp"
#include "ManagerWork.hpp"
#include "ConditionalStateSubrecords.hpp"

namespace th10 {

extern void *g_AsciiHudConditionalState; // TH10 DAT_00477704
extern void *g_MainChainRenderOwner;     // TH10 ds:0x491c10

extern void AppendMainChainErrorText(const char *text); // TH10 0x44b810
extern void CrtFree(void *memory);                      // TH10 0x452422
// TH10 0x0040d530 (ManagerReleaseWrappers.cpp).
void DestroyAsciiHudConditionalStateEax(void *state);

namespace {
const u32 kAnimTag = 0x4d494e41U; // 'ANIM'
const u32 kEcliTag = 0x494c4345U; // 'ECLI'
} // namespace

// TH10 0x0040d680. Native ESI = the 0x68-byte conditional state
// destructor piece: publish the 0x46d0f0 vtable, CRT-free the +0x8c
// buffer and clear the pointer.
void DestroyConditionalStateBufferEsiAbi(void *state) {
    u8 *bytes = static_cast<u8 *>(state);
    *reinterpret_cast<u32 *>(bytes) = 0x46d0f0U;
    void *buffer = *reinterpret_cast<void **>(bytes + 0x8c);
    if (buffer != 0) {
        CrtFree(buffer);
        *reinterpret_cast<void **>(bytes + 0x8c) = 0;
    }
}

// TH10 0x0040d6b0. Native stdcall, one stack argument = the stage enemy
// script path (ret 4). operator new(0x68), wipe 0x1a dwords, set flag
// bit 1, publish DAT_00477704, run the sub-record initializer
// (0x40d280). On an initializer failure destroys (0x40d530), frees and
// returns null; otherwise returns the state. The native runs the
// initializer even for a null allocation (quirk preserved by order).
void *CreateStageConditionalStateStackAbi(const char *script_path) {
    void *state = ::operator new(0x68U);
    if (state != 0) {
        u32 *wipe = static_cast<u32 *>(state);
        for (int i = 0; i < 0x1a; ++i) {
            wipe[i] = 0;
        }
        *wipe |= 2u;
        g_AsciiHudConditionalState = state;
    }
    const i32 result = InitializeConditionalStateSubrecordsEbxStackAbi(
        state, script_path);
    if (result == 0) {
        return state;
    }
    if (state != 0) {
        DestroyAsciiHudConditionalStateEax(state);
        ::operator delete(state);
    }
    return 0;
}

// TH10 0x0040d400. Native ECX = the name-receiving sub-record object,
// stack = the section data (ret 4). Parses the inline section: validates
// the 'ANIM' tag, loads `count` names (each NUL-terminated, records
// realigned to 4 bytes) through the manager-work request with kind
// 9+index and publishes the work pointers into the conditional state's
// +0x34 table; a null result raises the 0x46cb68 diagnostic and returns
// -1. Then, when an 'ECLI' tag follows, registers each of its `count`
// names through the sub-record's vtable+0x8 entry. Returns 0 on success
// (or no ECLI block), -1 on the failure path, 0 for a missing ANIM tag.
i32 ParseAnimSectionStackAbi(void *sub_record, const u8 *data) {
    const u32 tag = *reinterpret_cast<const u32 *>(data);
    if (tag != kAnimTag) {
        return 0;
    }
    const u32 name_count = *reinterpret_cast<const u32 *>(data + 4);
    const u8 *base = data + 8;
    const u8 *cursor = base;

    u8 *state = static_cast<u8 *>(g_AsciiHudConditionalState);
    for (u32 i = 0; i < name_count; ++i) {
        const char *name = reinterpret_cast<const char *>(cursor);
        ManagerWorkPartial *work = RequestManagerWork(
            reinterpret_cast<ManagerWorkOwnerPartial *>(
                g_MainChainRenderOwner),
            static_cast<i32>(i) + 9, name);
        *reinterpret_cast<ManagerWorkPartial **>(
            state + 0x34 + i * 4) = work;
        if (work == 0) {
            AppendMainChainErrorText(
                reinterpret_cast<const char *>(0x46cb68));
            return -1;
        }
        while (*cursor != '\0') {
            ++cursor;
        }
        ++cursor;
        const u32 consumed = static_cast<u32>(cursor - base);
        cursor = base + ((consumed + 3) & ~3u);
    }

    if (*reinterpret_cast<const u32 *>(cursor) != kEcliTag) {
        return 0;
    }
    const u32 ecli_count = *reinterpret_cast<const u32 *>(cursor + 4);
    cursor += 8;
    for (u32 i = 0; i < ecli_count; ++i) {
        void *const *vtable =
            *reinterpret_cast<void *const **>(sub_record);
        typedef void (TH10_STDCALL *RegisterNameVirtual)(const char *name);
        RegisterNameVirtual register_name =
            reinterpret_cast<RegisterNameVirtual>(vtable[1]);
        register_name(reinterpret_cast<const char *>(cursor));

        while (*cursor != '\0') {
            ++cursor;
        }
        ++cursor;
    }
    return 0;
}

} // namespace th10
