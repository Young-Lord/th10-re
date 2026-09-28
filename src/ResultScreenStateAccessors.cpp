#include "ResultScreenStateAccessors.hpp"

namespace th10 {

namespace {

// TH10 0x452493: operator new; 0x00415b00: result-script initializer.
void *AllocateHeapBlock(u32 bytes);
void *InitializeResultScreenScriptStateNative(void *state, void *script);

extern void *g_ResultScriptSlotCount;   // TH10 DAT_00474c84 (dword counter)
extern void *g_ResultScriptSlotCache;   // TH10 DAT_00474c8c

} // namespace

i32 CreateResultScreenScriptSlotEdiEsiAbi(void *manager, i32 slot)
{
    void *state = AllocateHeapBlock(0x90);
    void *script = 0;
    if (state != 0) {
        u8 *base = *reinterpret_cast<u8 **>(
            static_cast<u8 *>(manager) + 0x9ebc);
        script = InitializeResultScreenScriptStateNative(
            state, base + *reinterpret_cast<u32 *>(
                             base + static_cast<u32>(slot) * 8U + 4U));
    }
    *reinterpret_cast<void **>(
        static_cast<u8 *>(manager) + 0x9eb8) = script;
    // Native quirk: the slot stamp runs on the raw result, so a failed
    // allocation would write through null; operator new aborts first.
    *reinterpret_cast<u32 *>(script) = static_cast<u32>(slot);
    const u32 next = static_cast<u32>(slot) + 1U;
    const u32 previous = *reinterpret_cast<u32 *>(&g_ResultScriptSlotCount);
    *reinterpret_cast<u32 *>(&g_ResultScriptSlotCount) = next;
    if (previous != next)
        *reinterpret_cast<u32 *>(&g_ResultScriptSlotCache) = 0;
    return static_cast<i32>(next);
}

i32 ReadEntityFlagBit2EaxAbi(const void *record)
{
    const u32 flags = *reinterpret_cast<const u32 *>(
        static_cast<const u8 *>(record) + 0x60);
    return static_cast<i32>((flags >> 2) & 1U);
}

i32 ReadEntityFlagBit4EaxAbi(const void *record)
{
    const u32 flags = *reinterpret_cast<const u32 *>(
        static_cast<const u8 *>(record) + 0x60);
    return static_cast<i32>((flags >> 4) & 1U);
}

i32 ReadEntityFlagBit5EaxAbi(const void *record)
{
    const u32 flags = *reinterpret_cast<const u32 *>(
        static_cast<const u8 *>(record) + 0x60);
    return static_cast<i32>((flags >> 5) & 1U);
}

i32 ReadManagerFlagBit3At2A18EaxAbi(const void *manager)
{
    const u32 flags = *reinterpret_cast<const u32 *>(
        static_cast<const u8 *>(manager) + 0x2a18);
    return static_cast<i32>((flags >> 3) & 1U);
}

namespace {

void MarkResultPairFlags(void *owner)
{
    u8 *bytes = static_cast<u8 *>(owner);
    u32 *first = *reinterpret_cast<u32 **>(bytes + 8);
    first[1] |= 2U;
    u32 *second = *reinterpret_cast<u32 **>(bytes + 0xc);
    second[1] |= 2U;
}

} // namespace

void MarkResultPairFlagsThiscallA(void *owner)
{
    MarkResultPairFlags(owner);
}

void MarkResultPairFlagsThiscallB(void *owner)
{
    MarkResultPairFlags(owner);
}

i32 RearmInterpTimerAt40EaxAbi(void *block)
{
    u8 *bytes = static_cast<u8 *>(block);
    u32 flags = *reinterpret_cast<u32 *>(bytes + 0x50);
    if ((flags & 1U) == 0) {
        flags |= 1U;
        *reinterpret_cast<u32 *>(bytes + 0x44) = 0;
        *reinterpret_cast<u32 *>(bytes + 0x40) = 0xfff0bdc1U;
        *reinterpret_cast<u32 *>(bytes + 0x48) = 0;
        // TH10 0x00476f78: frame-time scale rate pointer.
        *reinterpret_cast<u32 *>(bytes + 0x4c) = 0x476f78U;
        *reinterpret_cast<u32 *>(bytes + 0x50) = flags;
    }
    *reinterpret_cast<u32 *>(bytes + 0x44) = 0;
    *reinterpret_cast<u32 *>(bytes + 0x48) = 0;
    *reinterpret_cast<i32 *>(bytes + 0x40) = -1;
    *reinterpret_cast<u32 *>(bytes + 0x58) = 0;
    *reinterpret_cast<u32 *>(bytes + 0x5c) = 0;
    return 0;
}

} // namespace th10
