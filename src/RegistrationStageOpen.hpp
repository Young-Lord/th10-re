#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x00413350. Allocates the 0x8c registration draw owner, zeroes
// it, publishes it at DAT_00477708 with flag bit 1, allocates the 0x24
// scheduler callback node for RegistrationDrawCallback (0x00413690),
// binds the owner at node+0x20 and registers it on the draw scheduler
// (slot 0x2f). Returns the owner (0 when the allocation failed, in
// which case the node is still registered against a null owner).
void *CreateRegistrationDrawOwner();

// TH10 0x00413dc0-family release used by 0x004136c0. ESI = owner.
// Releases the six entity handles at owner+0x40..+0x54 through
// ReleaseEntityById and clears them.
void ReleaseRegistrationOwnerHandleChainEsiAbi(void *owner);

// TH10 0x00413740. EAX = main chain context. Sets flag bit 4 of
// +0x9eb4 and clears the +0x9ecc staging pointer.
void SetManagerRegistrationFlagEaxAbi(void *manager);

// TH10 0x00413760 / 0x00413780. EAX = main chain context. Flag bit
// 4 / bit 5 reads of +0x9eb4.
i32 ReadManagerRegistrationFlagEaxAbi(const void *manager);
i32 ReadManagerPostRegistrationFlagEaxAbi(const void *manager);

// TH10 0x00413980. EBX = main chain context. Requests manager work
// slot 6 for the stage script, fails with the logger on the reserved
// global when the slot is unavailable, opens the scene script
// resource, then installs the stage calc callback 0x00415ae0 (slot
// 0x18) and draw callback 0x00415af0 (slot 0x2b) at manager+8/+0xc.
i32 OpenStageScriptSequenceEbxAbi(void *manager);

// TH10 0x00414570. EBX = main chain context. Frees the registration
// owner at +0x9eb8 (releasing its handle chain first) and, unless
// DAT_00474ca0 bit 0/3 is set, also releases the manager work buffer
// at DAT_00491c10+0x3ad0dc and the +0x9ebc script pointer, clearing
// +0x9e80.
void ReleaseRegistrationOwnerFromManagerEbxAbi(void *manager);

} // namespace th10
