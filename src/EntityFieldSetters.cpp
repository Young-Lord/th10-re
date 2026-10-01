// TH10 small entity field setters / render-flag getters.
//
// One family (0x401ce0 / 0x401d60 / 0x405170 / 0x405190 / 0x4087a0 /
// 0x40c960) writes a pair or scalar into an entity record and raises the
// "geometry dirty" bits in the +0x35c flag word (bit 3 mask 0x8 for the
// size/position setters, bit 2 mask 0x4 for the rotation setter). The
// second family (0x408780 / 0x408790 / 0x4087c0) reads bits of the
// entity's +0x58 render-flags dword.
//
// Native register ABIs (EAX = record, stack args) remain thunk boundaries.

#include "Th10Types.hpp"
#include "VmRecord.hpp"

namespace th10 {

namespace {

u32 &DirtyFlags(u8 *entity) { // +0x35c dirty-flag word
    return reinterpret_cast<VmRecord *>(entity)->flags;
}

} // namespace

// TH10 0x00401ce0. Native EAX = record, stack (a, b); ret 8. Stores the
// pair at +0x4c/+0x50 and raises dirty bit 0x8 once (after both stores).
void SetEntitySizePairEaxStackAbi(void *entity, u32 a, u32 b) {
    u8 *bytes = static_cast<u8 *>(entity);
    VmRecord &vm = *reinterpret_cast<VmRecord *>(entity);
    vm.width = a;
    vm.height = b;
    DirtyFlags(bytes) |= 0x8u;
}

// TH10 0x00401d60. Native EAX = record, stack (a, b); ret 8. Same pattern
// into +0x3c/+0x40.
void SetEntityPositionPairEaxStackAbi(void *entity, u32 a, u32 b) {
    u8 *bytes = static_cast<u8 *>(entity);
    *reinterpret_cast<u32 *>(bytes + 0x3c) = a;
    *reinterpret_cast<u32 *>(bytes + 0x40) = b;
    DirtyFlags(bytes) |= 0x8u;
}

// TH10 0x00405170. Native EAX = record, stack (value); ret 4. +0x40 setter.
void SetEntityField40EaxStackAbi(void *entity, u32 value) {
    u8 *bytes = static_cast<u8 *>(entity);
    *reinterpret_cast<u32 *>(bytes + 0x40) = value;
    DirtyFlags(bytes) |= 0x8u;
}

// TH10 0x00405190. Native EAX = record, stack (value); ret 4. +0x3c setter.
void SetEntityField3cEaxStackAbi(void *entity, u32 value) {
    u8 *bytes = static_cast<u8 *>(entity);
    *reinterpret_cast<u32 *>(bytes + 0x3c) = value;
    DirtyFlags(bytes) |= 0x8u;
}

// TH10 0x004087a0. Native EAX = record, stack (value); ret 4. Writes the
// rotation at +0x2c and raises dirty bit 0x4.
void SetEntityRotationEaxStackAbi(void *entity, u32 value) {
    u8 *bytes = static_cast<u8 *>(entity);
    *reinterpret_cast<u32 *>(bytes + 0x2c) = value;
    DirtyFlags(bytes) |= 0x4u;
}

// TH10 0x0040c960. Native EAX = record, stack (a, b); ret 8. Writes the
// +0x34/+0x38 pair; no dirty-flag side effects on this record type.
void SetEntityScalePairEaxStackAbi(void *entity, u32 a, u32 b) {
    u8 *bytes = static_cast<u8 *>(entity);
    *reinterpret_cast<u32 *>(bytes + 0x34) = a;
    *reinterpret_cast<u32 *>(bytes + 0x38) = b;
}

// TH10 0x00408780. Native EAX = record: bit 2 (0x4) of the +0x58 render
// flags.
i32 GetEntityRenderFlag2EaxAbi(void *entity) {
    return (*reinterpret_cast<u32 *>(
                static_cast<u8 *>(entity) + 0x58) >> 2) & 1;
}

// TH10 0x00408790. Native EAX = record: the native computes
// ((flags >> 2) | flags) & 1 — the OR with the unshifted value makes bit 0
// of +0x58 count as well.
i32 GetEntityRenderFlag2Or0EaxAbi(void *entity) {
    const u32 flags = *reinterpret_cast<u32 *>(
        static_cast<u8 *>(entity) + 0x58);
    return ((flags >> 2) | flags) & 1;
}

// TH10 0x004087c0. Native EAX = record: bit 1 (0x2) of the +0x58 flags.
i32 GetEntityRenderFlag1EaxAbi(void *entity) {
    return (*reinterpret_cast<u32 *>(
                static_cast<u8 *>(entity) + 0x58) >> 1) & 1;
}

} // namespace th10
