#pragma once

#include "Th10Types.hpp"

namespace th10 {

// Stage-trigger object ("scene enemy") per-frame update and spawn-from-
// descriptor. The records are the 0x7f0-stride trigger objects described in
// SceneTriggerObject.hpp/.cpp; 0x406d90 (the instruction-queue interpreter)
// and the expire helpers live there.

// TH10 0x406240. Native stdcall, one stack argument = the trigger object
// (ret 4); the outgoing argument slot is reused as the effect-id output
// when 0x448db0 spawns the timeout effect. Runs the object state machine:
// state 1 runs the instruction queue plus the flag-driven feature updates,
// states 2/3 integrate the screen-space velocity, and the timeout-region
// check moves the object to the expiring state. Returns the result of the
// object's setup-opcode dispatch (nonzero means the object released
// itself), or -1 after the release path.
i32 UpdateSceneTriggerObjectStackAbi(void *object);

// TH10 0x4067d0. Native stdcall ret 0x10 (manager, column, row, float
// arg) with the spawn *descriptor record* passed in EBX — a register ABI
// quirk preserved here by promoting it to an explicit parameter. Scans the
// manager's 2000-slot trigger-object pool (base manager+0x60, stride
// 0x7f0, cursor at manager+0x10, state word +0x446, end sentinel state 5)
// for a free slot, initializes it from the descriptor (movement-mode
// position math, VM bind, per-kind tables, instruction-queue copy) and
// runs the queue once. Returns 1 when the pool scan budget is exhausted,
// 0 after a successful spawn.
i32 SpawnSceneTriggerFromDescriptorEbxStackAbi(void *manager,
                                               void *descriptor, i32 column,
                                               i32 row, float arg_c);

} // namespace th10
