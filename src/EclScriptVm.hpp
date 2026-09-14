#pragma once

#include <stddef.h>

#include "Th10Types.hpp"

namespace th10 {

// ---------------------------------------------------------------------------
// TH10 enemy-script ("ECL") virtual machine core.
//
// The script manager keeps a doubly-linked list of active run contexts
// (manager+0x1030 fake head node, manager+0x1034 first node). Each context is
// a 0x1024-byte allocation:
//
//   +0x0000 f32  time        remaining/next tick compared against ins->time
//   +0x0004 ptr  ins         current ECL instruction record
//   +0x0008     chunk[4096]  value stack / argument / variable scratch
//   +0x1008 u32  stack_cursor pop cursor into the chunk (0 == empty)
//   +0x100C u32  arg_base     variable-slot base for the current instruction
//   +0x1010 u32  script_id    sub-script id (0x450160 lookup key)
//   +0x1014 ptr  manager      owning script manager (vtable'd variable store)
//   +0x1018 u32  field_1018   written by opcode 20
//   +0x101C u8   rank_mask    difficulty/rank gate copied from the parent
//   +0x1020 u32  flags        bit 0 toggled by opcodes 18/19
//
// Instruction records are byte strings laid out as:
//   +0x00 i32 time, +0x04 i16 opcode, +0x08 u16 variable mask,
//   +0x0A u8 rank mask, +0x10.. argument dwords (arg 0 may be a byte size,
//   see opcode 16), followed by one type byte per argument.
// ---------------------------------------------------------------------------

struct EclInstruction {
    i32 time;
    i16 opcode;
    u16 variable_mask;
    u8 rank_mask;
    u8 pad_0b[5]; // +0x0b..0x0f; argument dwords start at +0x10.
    u32 args[1];
};

struct EclRunContext {
    float time;
    EclInstruction *ins;
    u8 chunk[0x1000];
    u32 stack_cursor;
    u32 arg_base;
    u32 script_id;
    void *manager;
    u32 field_1018;
    u8 rank_mask;
    u8 pad_101d[3];
    u32 flags;
};

struct EclContextNode {
    EclRunContext *context;
    EclContextNode *next;
    EclContextNode *prev;
};

typedef char AssertEclRunContextSize[
    sizeof(EclRunContext) == 0x1024 ? 1 : -1];
typedef char AssertEclRunContextStackCursorOffset[
    offsetof(EclRunContext, stack_cursor) == 0x1008 ? 1 : -1];
typedef char AssertEclRunContextScriptIdOffset[
    offsetof(EclRunContext, script_id) == 0x1010 ? 1 : -1];

// TH10 0x0044ff00. Evaluates instruction argument `arg_index` as an integer.
// Without the mask bit the raw dword is returned. With the mask bit:
// value >= 0 reads a variable slot (ctx+8+arg_base+value); -1 pops a typed
// stack entry (converting through _ftol2 when tagged 'f'); other negatives
// fetch from the manager variable store (vtable slot +4).
i32 EclVmEvalIntArg(EclRunContext *ctx, u32 arg_index);

// TH10 0x0044ff80. Float argument evaluation with a caller-supplied default.
// value >= 30 reads a chunk slot as float; 0.125 pops (int-tagged entries
// convert through the bit pattern); other positives fetch via manager vtable
// slot +0xc (returns in ST0).
float EclVmEvalFloatArg(EclRunContext *ctx, u32 arg_index, float fallback);

// TH10 0x0044fe40. Float argument evaluation reading the raw dword from the
// instruction record itself (mask-clear case returns that dword as float).
float EclVmEvalFloatArgFromIns(EclRunContext *ctx, u32 arg_index);

// TH10 0x0044fdb0. Integer argument evaluation like EclVmEvalIntArg but the
// raw value is always read from the instruction record (mask-clear case).
i32 EclVmEvalIntArgFromIns(EclRunContext *ctx, u32 arg_index);

// TH10 0x00450030. Resolves argument `arg_index` to a chunk pointer or a
// manager-store lookup (vtable slot +8); returns 0 when the mask bit is
// clear.
void *EclVmResolvePointerArg(EclRunContext *ctx, u32 arg_index);

// TH10 0x00450070. Resolves argument `arg_index` (raw value read as float)
// to a chunk pointer (value >= 30) or a manager-store string lookup
// (vtable slot +0x10); returns 0 when the mask bit is clear.
void *EclVmResolveStringArg(EclRunContext *ctx, u32 arg_index);

// TH10 0x00450160. Walks the manager's context-node list and returns the
// node whose context has the given script id, or null.
EclContextNode *FindEclContextNodeById(void *manager, u32 script_id);

// TH10 0x004500d0. Allocates a fresh 0x1024 run context plus its list node,
// links the node into the manager list, inherits the rank byte from the
// current context, and begins the sub-script frame via 0x44df70.
i32 SpawnEclSubContext(void *manager, float initial_value, u32 script_id,
                       u32 first_arg_index);

// TH10 0x0044df70. Begins a sub-context frame: pops a saved dword (when the
// parent stack is non-empty), stores {saved, parent context, instruction} at
// the frame head, copies the remaining instruction arguments (with 'f'-type
// float/int conversion) into the new chunk under the 4096-byte limit, and
// resolves the label name at ins+0x14 to the start index (0x450470).
// Returns 0 on success, -1 when the label cannot be resolved.
i32 BeginEclSubFrame(EclRunContext *ctx, float initial_value,
                     EclRunContext *parent, u32 first_arg_index);

// TH10 0x0044fd10. Runs every active context of the manager once: publishes
// each node's context at manager+4, executes one instruction (0x44e1a0) and
// unlinks + frees finished contexts. A failure on the first node returns -1
// without releasing it; the tail end writes manager+4 = manager+8.
i32 RunEclContextListEdiStackAbi(void *manager, float delta);

// TH10 0x0044e1a0. Executes the current instruction of `ctx`.
// Return 1 = the context has finished (caller frees it), 0 = keep running,
// -1 = hard failure. Opcodes outside the verified set (see
// docs/evidence/ecl-script-vm.md) keep the native no-op/advance behaviour.
i32 ExecuteEclInstruction(EclRunContext *ctx, float delta);

// TH10 0x0040e5f0. Native stdcall boundary = script manager; runs the enemy
// death sequence and always returns 1. Implemented in EnemyDeathEffects.cpp.
i32 TriggerEnemyDeathSequenceStdcallAbi(void *script_manager);

} // namespace th10
