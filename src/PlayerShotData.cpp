#include "PlayerShotData.hpp"

#include "AsciiAnimationVm.hpp"
#include "PackedArchive.hpp"

#include <cmath>
#include <string.h>

namespace th10 {

// Player VM initialization and shot-data loading. The shot file stores
// relative pointers that are rebased here, and record handler fields are
// resolved through four index tables.

namespace {

extern void *g_MainChainRenderOwner; // TH10 DAT_00491c10
extern float g_FrameTimeScale; // TH10 DAT_00476f78
extern void *g_VmHandlerTableA[4]; // TH10 0x47476c
extern void *g_VmHandlerTableB[4]; // TH10 0x474778
extern void *g_VmHandlerTableC[4]; // TH10 0x491bf4 (runtime-filled)
extern void *g_VmHandlerTableD[4]; // TH10 0x491bf8 (runtime-filled)

// TH10 0x0043ee30: runs the VM's script interpreter (stack-arg form here).
void UpdateAnimationVmStackAbi(void *vm);

inline u32 ReadUint(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const u32 *>(bytes + offset);
}

inline u16 ReadU16(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const u16 *>(bytes + offset);
}

inline void WriteUint(u8 *bytes, u32 offset, u32 value)
{
    *reinterpret_cast<u32 *>(bytes + offset) = value;
}

inline void WriteU16(u8 *bytes, u32 offset, u16 value)
{
    *reinterpret_cast<u16 *>(bytes + offset) = value;
}

// The 0x5c timer block inside a VM record (prev/acc pair at 0x5c..0x68,
// flag at 0x6c); the sentinel write is overwritten by the unconditional
// reset, matching the native order.
void ResetVmTimerBlock(u8 *vm)
{
    if ((ReadUint(vm, 0x6c) & 1) == 0) {
        WriteUint(vm, 0x60, 0);
        WriteUint(vm, 0x5c, 0xfff0bdc1U);
        WriteUint(vm, 0x64, 0);
        *reinterpret_cast<const float **>(vm + 0x68) = &g_FrameTimeScale;
        WriteUint(vm, 0x6c, ReadUint(vm, 0x6c) | 1);
    }
    WriteUint(vm, 0x60, 0);
    WriteUint(vm, 0x64, 0);
    WriteUint(vm, 0x5c, 0xffffffffU);
}

} // namespace

// TH10 0x0043e710. A missing script or the manager's stop flag wipes the
// whole 0x3ac-byte VM record, including fields the reset preserves.
void AssignAnmScriptToVmEcxEaxBbxAbi(void *anm_work, void *vm_memory,
                                     i32 script_index)
{
    u8 *const vm = static_cast<u8 *>(vm_memory);
    u8 *const manager = static_cast<u8 *>(anm_work);
    void *const script = ReadUint(manager, 0x11c) != 0
        ? reinterpret_cast<void **>(ReadUint(manager, 0x11c))[script_index]
        : static_cast<void *>(0);
    if (script == 0 || ReadUint(manager, 0x124) != 0) {
        memset(vm, 0, 0x3ac);
        return;
    }
    ResetAsciiAnimationVmRecord(vm);
    WriteU16(vm, 0x38a, static_cast<u16>(script_index));
    WriteU16(vm, 0x386, ReadU16(manager, 0));
    WriteUint(vm, 0x308, reinterpret_cast<u32>(manager));
    WriteUint(vm, 0x35c, ReadUint(vm, 0x35c) & ~0x600U);
    WriteUint(vm, 0x38c, reinterpret_cast<u32>(script));
    WriteUint(vm, 0x390, reinterpret_cast<u32>(script));
    ResetVmTimerBlock(vm);
    WriteUint(vm, 0x35c, ReadUint(vm, 0x35c) & ~1U);
    UpdateAnimationVmStackAbi(vm);
    *reinterpret_cast<u32 *>(static_cast<u8 *>(g_MainChainRenderOwner) +
                             0x4c) += 1;
}

// TH10 0x00404f30. Resets the player VM record, clears the scratch and
// offset regions, publishes the script index, then binds the script.
void InitializePlayerMainVmEsiStackAbi(void *vm_memory, void *anm_work,
                                       i32 script_index)
{
    u8 *const vm = static_cast<u8 *>(vm_memory);
    ResetAsciiAnimationVmRecord(vm);
    static const u32 kClearedOffsets[9] = {
        0x340, 0x344, 0x348, 0x334, 0x338, 0x33c, 0x34c, 0x350, 0x354};
    for (u32 index = 0; index != 9; ++index)
        WriteUint(vm, kClearedOffsets[index], 0);
    *reinterpret_cast<u8 *>(vm + 0x3a1) = 0x10;
    *reinterpret_cast<u8 *>(vm + 0x3a0) = 0x10;
    WriteU16(vm, 0x38a, static_cast<u16>(script_index));
    AssignAnmScriptToVmEcxEaxBbxAbi(anm_work, vm, script_index);
}

// TH10 0x00426520. Loads the shot-data file, scales its two unit vectors by
// sin(pi/4), and rebases the record pointer tables.
i32 LoadPlayerShotDataEsiEaxAbi(void *player_memory, const char *entry_name)
{
    u8 *const player = static_cast<u8 *>(player_memory);
    u8 *const buffer = LoadPackedResource(entry_name, 0, 0);
    *reinterpret_cast<u8 **>(player + 0x45c) = buffer;
    if (buffer == 0)
        return -1;

    // FLD qword [0x470c50] (pi/4 as a double); FSIN. The native performs
    // the two writes twice with identical values; one pass is modeled.
    const double kPiOverFour = 0.7853981633974483;
    const float k = static_cast<float>(std::sin(kPiOverFour));
    *reinterpret_cast<float *>(buffer + 0x18) =
        k * *reinterpret_cast<const float *>(buffer + 0x10);
    *reinterpret_cast<float *>(buffer + 0x1c) =
        k * *reinterpret_cast<const float *>(buffer + 0x14);

    const u16 record_lists = ReadU16(buffer, 2);
    for (u32 index = 0; index != record_lists; ++index) {
        u32 *const slot =
            reinterpret_cast<u32 *>(buffer + 0x110 + index * 8);
        *slot += reinterpret_cast<u32>(buffer);
        u8 *node = reinterpret_cast<u8 *>(*slot);
        while (*reinterpret_cast<const signed char *>(node) >= 0) {
            WriteUint(node, 0x24,
                      reinterpret_cast<u32>(
                          g_VmHandlerTableA[ReadUint(node, 0x24)]));
            WriteUint(node, 0x28,
                      reinterpret_cast<u32>(
                          g_VmHandlerTableB[ReadUint(node, 0x28)]));
            WriteUint(node, 0x2c,
                      reinterpret_cast<u32>(
                          g_VmHandlerTableC[ReadUint(node, 0x2c)]));
            WriteUint(node, 0x30,
                      reinterpret_cast<u32>(
                          g_VmHandlerTableD[ReadUint(node, 0x30)]));
            node += 0x34;
        }
    }
    return 0;
}

} // namespace th10
