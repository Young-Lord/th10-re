// TH10 game-manager gate slot helpers (0x401d20 / 0x401fd0).
//
// The 0x491c28 game-manager slot owns a bank of 0x118-byte records at
// +0x154 (the per-record camera work / script bind blocks). 0x401d20 is the
// "acquire record by index" step used by ECL instruction handlers: it
// parks the record pointer at slot+0x384, refreshes the camera work, and
// virtually binds the record's +0xcc script area on the slot's +0x8 object.

#include "Th10Types.hpp"
#include "MainChainRender.hpp"

namespace th10 {

// The slot's +0x8 object publishes the bind entry through its vtable slot
// +0xbc: native __thiscall (ECX = object, stack = record script area).
typedef void (TH10_STDCALL *BindGateRecordVirtual)(void *record_script_area);

namespace {

void *const g_GameManagerSlotBase = reinterpret_cast<void *>(0x491c28);

} // namespace

// TH10 0x0040215a0 boundary (native EDI = 0x118-byte camera work record).
void UpdateMainChainCameraWorkEdiAbi(MainChainCameraWork *work); // declared

// TH10 0x00401d20. Native EBX = record index, ESI = the 0x491c28 slot.
// record = slot + index * 0x118 + 0x154; stores it at slot+0x384, runs the
// camera-work refresh over the record, then calls the +0x8 object's vtable
// +0xbc with record+0xcc, and finally publishes the index at slot+0x388.
void AcquireGateVmRecordSlotEbxAbi(void *slot, u32 index) {
    u8 *slot_bytes = static_cast<u8 *>(slot);
    u8 *record = slot_bytes + 0x154 + index * 0x118;
    *reinterpret_cast<u8 **>(slot_bytes + 0x384) = record;

    UpdateMainChainCameraWorkEdiAbi(reinterpret_cast<MainChainCameraWork *>(
        record));

    void *binder = *reinterpret_cast<void **>(slot_bytes + 0x8);
    void *const *vtable = *reinterpret_cast<void *const **>(binder);
    BindGateRecordVirtual bind =
        reinterpret_cast<BindGateRecordVirtual>(vtable[0xbc / 4]);
    bind(record + 0xcc);

    *reinterpret_cast<u32 *>(slot_bytes + 0x388) = index;
}

// TH10 0x00401d20 native entry wrapper bound at the 0x491c28 slot.
void AcquireGameManagerGateVmRecordSlotEbxAbi(u32 index) {
    AcquireGateVmRecordSlotEbxAbi(g_GameManagerSlotBase, index);
}

} // namespace th10
