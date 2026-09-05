#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x00404f30. Native inputs ESI = VM record, stack = ANM manager-work
// pointer, EAX = script index; the register ABI remains a thunk boundary.
void InitializePlayerMainVmEsiStackAbi(void *vm, void *anm_work,
                                       i32 script_index);

// TH10 0x0043e710. Native inputs ECX = ANM manager-work, EAX = VM,
// EBX = script index carried in from the caller.
void AssignAnmScriptToVmEcxEaxBbxAbi(void *anm_work, void *vm,
                                     i32 script_index);

// TH10 0x00426520. Native inputs ESI = player, EAX = selected entry file
// name; returns zero on success and -1 when the load fails.
i32 LoadPlayerShotDataEsiEaxAbi(void *player, const char *entry_name);

} // namespace th10
