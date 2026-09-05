#pragma once

#include "Th10Platform.hpp"
#include "Th10Types.hpp"

namespace th10 {

// TH10 0x00427950. Native thiscall with ECX = option record; returns zero.
i32 TH10_FASTCALL UpdateHomingOptionRecord(void *record);

// TH10 0x00427ad0. Native thiscall with ECX = option record; returns zero.
i32 TH10_FASTCALL UpdateAngularOptionRecord(void *record);

} // namespace th10
