#pragma once

#include "CallbackScheduler.hpp"

namespace th10 {

#pragma pack(push, 4)
struct RegistrationDrawOwner {
    u32 flags;
    u8 unknown_0004[8];
    ChainElem *draw_record;
    u8 unknown_0010[4];
    double last_tick;
    u32 phase_count;
    u32 frame_accumulator;
    double displayed_fps;
    double elapsed_window;
    float sampled_fps;
    u8 unknown_0038[0x54];
};
#pragma pack(pop)

typedef char AssertRegistrationDrawOwnerSize[
    sizeof(RegistrationDrawOwner) == 0x8c ? 1 : -1];
typedef char AssertRegistrationDrawOwnerRecordOffset[
    offsetof(RegistrationDrawOwner, draw_record) == 0xc ? 1 : -1];
typedef char AssertRegistrationDrawOwnerCounterOffset[
    offsetof(RegistrationDrawOwner, frame_accumulator) == 0x20 ? 1 : -1];
typedef char AssertRegistrationDrawOwnerFpsOffset[
    offsetof(RegistrationDrawOwner, sampled_fps) == 0x34 ? 1 : -1];

RegistrationDrawOwner *CreateRegistrationDrawOwner(); // TH10 0x00413350
i32 TH10_FASTCALL RegistrationDrawCallback(RegistrationDrawOwner *owner);
// Semantic body of TH10 0x00413450. Its native entry takes the owner in EBX
// and returns with plain ret, which remains a separate thunk boundary.
void DestroyRegistrationDrawOwner(RegistrationDrawOwner *owner);

} // namespace th10
