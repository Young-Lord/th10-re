#pragma once

#include "CallbackScheduler.hpp"
#include "Th10Types.hpp"

namespace th10 {

// TH10 0x00448860. Native entry receives owner in EAX/__fastcall ECX.
i32 UpdateLargeRenderOwnerListA(void *owner);
// TH10 0x00448900.
i32 UpdateLargeRenderOwnerListB(void *owner);
// TH10 0x00448980. Native entry receives kind in EAX and owner in EDI.
i32 DrawLargeRenderOwnerKindChain(void *owner, u32 kind);
// TH10 0x00448bb0. Native entry receives node in ESI and owner in EDI.
void DestroyRenderOwnerNodeOnSetupFailure(void *owner, void *node);
// TH10 0x004462f0. Native entry receives owner in EAX.
void InitializeLargeRenderOwner(void *owner);

ChainCallback GetLargeRenderOwnerCallback(u32 index);

} // namespace th10
