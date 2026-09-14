#include "EclScriptVm.hpp"

#include <math.h>
#include <string.h>

#include "PlayerMotionHelpers.hpp"
#include "Th10Platform.hpp"

namespace th10 {

namespace {

// ---------------------------------------------------------------------------
// Native boundaries (register ABIs kept as extern thunks; see evidence).
// ---------------------------------------------------------------------------

// TH10 0x00452493 operator new / 0x004524a1 operator delete.
void *NativeAlloc(u32 bytes);
void NativeFree(void *block);

// TH10 0x00450470 (ResolveScriptTableIndexEaxAbi). EAX = the script's label
// request object (its +8 holds the table size and +0x8c the name table);
// resolves a label name to table_index + 16, or 0.
i32 ResolveScriptTableIndexEaxAbi(i32 request);

// TH10 0x00450190. Native EAX = manager. Sub-script control helper used by
// opcode 21; body not yet reconstructed.
void EclVmOpcode21EaxAbi(void *manager);

// TH10 0x00450690. Native EAX = chunk cursor owner (ctx+8), ECX = value;
// pushes an integer onto the value stack.
void EclVmPushIntEaxEcxAbi(void *chunk, i32 value);

// TH10 0x004506d0. Native EAX = chunk cursor owner (ctx+8); pops/normalizes
// the top typed stack entry.
void EclVmPopTopEaxAbi(void *chunk);

// TH10 0x004505b0. Native EAX = chunk cursor owner (ctx+8), DL = type tag,
// stack = {byte size, const void* value}; copies a typed value onto the
// stack and advances the cursor under the 4096-byte limit.
void EclVmPushTypedEaxDlStackAbi(void *chunk, u8 type_tag, u32 size,
                                 const void *value);

// TH10 0x004501b0. Native ECX = out pair, stack = two floats; angle/vector
// helper used by opcode 81. Body not yet reconstructed.
void EclVmOpcode81HelperEcxEfxAbi(float out_pair[2], float wrapped_angle,
                                  float value);

// Script manager variable store (object at ctx->manager, vtable through its
// first dword):
//   slot +0x04 thiscall(i32 id) -> EAX  int variable fetch
//   slot +0x08 thiscall(i32 id) -> EAX  pointer fetch (0x450030 negative ids)
//   slot +0x0c fastcall(i64 id) -> ST0  float variable fetch
//   slot +0x10 fastcall(i64 id) -> EAX  string/pointer fetch (0x450070)
i32 ManagerGetVariableInt(void *manager, i32 id);
i32 ManagerGetVariablePointer(void *manager, i32 id);
float ManagerGetVariableFloatSt0(void *manager, long long id);
void *ManagerGetStringPointer(void *manager, long long id);

const u32 k_chunk_limit = 0x1000U;
const u8 k_type_float = 0x66U; // 'f'
const u8 k_type_int = 0x69U;   // 'i'

// TH10 __ftol2 (0x463b2c): x87 truncating float->int conversion.
i32 FloatToI32Truncate(float value)
{
    return static_cast<i32>(value);
}

u16 LoadU16At(const u8 *base)
{
    return static_cast<u16>(static_cast<u16>(base[0]) |
                            (static_cast<u16>(base[1]) << 8));
}

u32 LoadU32At(const u8 *base)
{
    return static_cast<u32>(base[0]) | (static_cast<u32>(base[1]) << 8) |
           (static_cast<u32>(base[2]) << 16) |
           (static_cast<u32>(base[3]) << 24);
}

void StoreU32At(u8 *base, u32 value)
{
    base[0] = static_cast<u8>(value);
    base[1] = static_cast<u8>(value >> 8);
    base[2] = static_cast<u8>(value >> 16);
    base[3] = static_cast<u8>(value >> 24);
}

// Typed stack entries inside ctx->chunk are {u8 type tag, 3 pad, 4 value}
// pairs; the cursor addresses the value dword of the top entry.
u8 *StackEntryType(EclRunContext *ctx, u32 cursor)
{
    return ctx->chunk + cursor;
}

u32 StackEntryValue(EclRunContext *ctx, u32 cursor)
{
    return LoadU32At(ctx->chunk + cursor + 4U);
}

float StackEntryValueAsFloat(EclRunContext *ctx, u32 cursor)
{
    const u32 raw = StackEntryValue(ctx, cursor);
    return *reinterpret_cast<const float *>(&raw);
}

// Raw 32-bit draw of the 0x4918b0 LCG pair, exactly as inlined inside
// 0x44e1a0: two half-steps (x = (state ^ 0x9630) - 0x6553; h = (x >> 14)
// + x * 4), state = low half each time, draw counter +2. Native oddity: the
// combined value duplicates the second half ((h2 << 16) | h2), unlike
// 0x44bb90 which combines (h1 << 16) | h2.
u32 LcgDrawRaw32Duplicated()
{
    extern u16 g_TimelinePrngStateB[4]; // TH10 DAT_004918b0, +0x4918b4 counter
    u32 x = (static_cast<u32>(*g_TimelinePrngStateB) ^ 0x9630U) - 0x6553U;
    const u32 h1 = ((x >> 14) & 0xFFFFU) + x * 4U;
    *g_TimelinePrngStateB = static_cast<u16>(h1);
    x = (static_cast<u32>(*g_TimelinePrngStateB) ^ 0x9630U) - 0x6553U;
    const u32 h2 = ((x >> 14) & 0xFFFFU) + x * 4U;
    *g_TimelinePrngStateB = static_cast<u16>(h2);
    const u32 counter = static_cast<u32>(g_TimelinePrngStateB[2]) |
                        (static_cast<u32>(g_TimelinePrngStateB[3]) << 16);
    const u32 next = counter + 2U;
    g_TimelinePrngStateB[2] = static_cast<u16>(next);
    g_TimelinePrngStateB[3] = static_cast<u16>(next >> 16);
    return (h2 << 16) | h2;
}

// fild with the signed 2^32 fix-up applied by every consumer.
double LcgDrawAsDouble()
{
    const u32 raw = LcgDrawRaw32Duplicated();
    double value = static_cast<double>(static_cast<i32>(raw));
    if (static_cast<i32>(raw) < 0)
        value += 4294967296.0;
    return value;
}

// Pop helpers. The cursor only moves while it stays >= 0 (native underflow
// guard); pops on an empty stack leave the output untouched.
bool PopInt(EclRunContext *ctx, i32 *out)
{
    const i32 value_cursor = static_cast<i32>(ctx->stack_cursor) - 4;
    if (value_cursor < 0)
        return false;
    ctx->stack_cursor = static_cast<u32>(value_cursor);
    u32 value = StackEntryValue(ctx, static_cast<u32>(value_cursor));
    const i32 type_cursor = value_cursor - 4;
    ctx->stack_cursor = static_cast<u32>(type_cursor);
    if (type_cursor >= 0 &&
        *StackEntryType(ctx, static_cast<u32>(type_cursor)) == k_type_float)
        value = static_cast<u32>(FloatToI32Truncate(
            *reinterpret_cast<const float *>(&value)));
    *out = static_cast<i32>(value);
    return true;
}

// Pop keeping the tag semantics of the float consumers: int-tagged entries
// reinterpret the bit pattern as an int and widen to float.
bool PopFloatTagged(EclRunContext *ctx, float *out, bool *was_int)
{
    const i32 value_cursor = static_cast<i32>(ctx->stack_cursor) - 4;
    if (value_cursor < 0)
        return false;
    ctx->stack_cursor = static_cast<u32>(value_cursor);
    float value = StackEntryValueAsFloat(ctx, static_cast<u32>(value_cursor));
    const i32 type_cursor = value_cursor - 4;
    ctx->stack_cursor = static_cast<u32>(type_cursor);
    bool is_int = false;
    if (type_cursor >= 0 &&
        *StackEntryType(ctx, static_cast<u32>(type_cursor)) == k_type_int) {
        is_int = true;
        value = static_cast<float>(static_cast<i32>(
            *reinterpret_cast<const u32 *>(ctx->chunk +
                                           static_cast<u32>(value_cursor) +
                                           4U)));
    }
    *out = value;
    if (was_int != 0)
        *was_int = is_int;
    return true;
}

void PushInt(EclRunContext *ctx, i32 value)
{
    EclVmPushTypedEaxDlStackAbi(ctx->chunk, k_type_int, 4U, &value);
}

void PushFloat(EclRunContext *ctx, float value)
{
    EclVmPushTypedEaxDlStackAbi(ctx->chunk, k_type_float, 4U, &value);
}

// Instruction argument access. Argument dwords live at ins+0x10+4*i; the
// 16-bit variable mask at ins+8; the argument-count byte at ins+0x0b.
bool ArgMaskBit(EclRunContext *ctx, u32 arg_index)
{
    const u16 mask = LoadU16At(reinterpret_cast<const u8 *>(ctx->ins) + 8U);
    return (mask & (1U << arg_index)) != 0U;
}

u32 RawArg(EclRunContext *ctx, u32 arg_index)
{
    return LoadU32At(reinterpret_cast<const u8 *>(ctx->ins) + 0x10U +
                     4U * arg_index);
}

// Jump target shared by opcodes 12/13/14: ins displacement at +0x10, new
// tick (integer, widened to float) at +0x14.
void ApplyJump(EclRunContext *ctx)
{
    const u8 *ins = reinterpret_cast<const u8 *>(ctx->ins);
    ctx->time = static_cast<float>(static_cast<i32>(LoadU32At(ins + 0x14U)));
    ctx->ins = reinterpret_cast<EclInstruction *>(
        reinterpret_cast<u8 *>(ctx->ins) +
        static_cast<i32>(LoadU32At(ins + 0x10U)));
}

bool PopTwoFloats(EclRunContext *ctx, float *a, float *b)
{
    // a = top of stack, b = the entry below it.
    if (!PopFloatTagged(ctx, a, 0))
        return false;
    if (!PopFloatTagged(ctx, b, 0))
        return false;
    return true;
}

bool PopTwoInts(EclRunContext *ctx, i32 *a, i32 *b)
{
    if (!PopInt(ctx, a))
        return false;
    if (!PopInt(ctx, b))
        return false;
    return true;
}

} // namespace

// ---------------------------------------------------------------------------
// 0x0044ff00
// ---------------------------------------------------------------------------
i32 EclVmEvalIntArg(EclRunContext *ctx, u32 arg_index)
{
    i32 result = static_cast<i32>(RawArg(ctx, arg_index));
    if (!ArgMaskBit(ctx, arg_index))
        return result;
    if (result < 0) {
        if (result == -1) {
            const i32 value_cursor =
                static_cast<i32>(ctx->stack_cursor) - 4;
            if (value_cursor >= 0) {
                ctx->stack_cursor = static_cast<u32>(value_cursor);
                result = static_cast<i32>(
                    StackEntryValue(ctx, static_cast<u32>(value_cursor)));
                const i32 type_cursor = value_cursor - 4;
                ctx->stack_cursor = static_cast<u32>(type_cursor);
                if (type_cursor >= 0 &&
                    *StackEntryType(ctx, static_cast<u32>(type_cursor)) ==
                        k_type_float)
                    result = FloatToI32Truncate(
                        *reinterpret_cast<const float *>(&result));
            }
        } else {
            result = ManagerGetVariableInt(ctx->manager, result);
        }
    } else {
        result = static_cast<i32>(LoadU32At(ctx->chunk + ctx->arg_base +
                                            static_cast<u32>(result)));
    }
    return result;
}

// ---------------------------------------------------------------------------
// 0x0044ff80
// ---------------------------------------------------------------------------
float EclVmEvalFloatArg(EclRunContext *ctx, u32 arg_index, float fallback)
{
    if (!ArgMaskBit(ctx, arg_index))
        return fallback;
    const float raw = static_cast<float>(RawArg(ctx, arg_index));
    if (raw >= 30.0f) {
        return *reinterpret_cast<const float *>(
            ctx->chunk + ctx->arg_base +
            static_cast<u32>(FloatToI32Truncate(raw)));
    }
    if (raw == 0.125f) {
        bool was_int = false;
        float value = fallback;
        PopFloatTagged(ctx, &value, &was_int);
        return value;
    }
    return ManagerGetVariableFloatSt0(
        ctx->manager, static_cast<long long>(static_cast<i32>(raw)));
}

// ---------------------------------------------------------------------------
// 0x0044fe40
// ---------------------------------------------------------------------------
float EclVmEvalFloatArgFromIns(EclRunContext *ctx, u32 arg_index)
{
    const u32 raw = RawArg(ctx, arg_index);
    if (!ArgMaskBit(ctx, arg_index)) {
        return *reinterpret_cast<const float *>(&raw);
    }
    const float value = *reinterpret_cast<const float *>(&raw);
    if (value >= 30.0f) {
        return *reinterpret_cast<const float *>(
            ctx->chunk + ctx->arg_base +
            static_cast<u32>(FloatToI32Truncate(value)));
    }
    if (value == 0.125f) {
        bool was_int = false;
        float popped = value;
        PopFloatTagged(ctx, &popped, &was_int);
        return popped;
    }
    return ManagerGetVariableFloatSt0(
        ctx->manager, static_cast<long long>(static_cast<i32>(value)));
}

// ---------------------------------------------------------------------------
// 0x0044fdb0
// ---------------------------------------------------------------------------
i32 EclVmEvalIntArgFromIns(EclRunContext *ctx, u32 arg_index)
{
    const i32 raw = static_cast<i32>(RawArg(ctx, arg_index));
    if (!ArgMaskBit(ctx, arg_index))
        return raw;
    if (raw < 0) {
        if (raw == -1) {
            const i32 cursor = static_cast<i32>(ctx->stack_cursor);
            const i32 value_cursor = cursor - 4;
            if (value_cursor >= 0) {
                ctx->stack_cursor = static_cast<u32>(value_cursor);
                i32 value = static_cast<i32>(
                    StackEntryValue(ctx, static_cast<u32>(value_cursor)));
                ctx->stack_cursor = static_cast<u32>(cursor - 8);
                // Native quirk: the type byte of the entry below the value
                // is tested at the chunk start plus the decremented cursor.
                if (cursor - 8 >= 0 &&
                    *StackEntryType(ctx, static_cast<u32>(cursor - 8)) ==
                        k_type_float)
                    value = FloatToI32Truncate(
                        *reinterpret_cast<const float *>(&value));
                return value;
            }
            return raw;
        }
        return ManagerGetVariableInt(ctx->manager, raw);
    }
    return static_cast<i32>(LoadU32At(ctx->chunk + ctx->arg_base +
                                      static_cast<u32>(raw)));
}

// ---------------------------------------------------------------------------
// 0x00450030
// ---------------------------------------------------------------------------
void *EclVmResolvePointerArg(EclRunContext *ctx, u32 arg_index)
{
    const u32 raw = RawArg(ctx, arg_index);
    if (!ArgMaskBit(ctx, arg_index))
        return 0;
    if (static_cast<i32>(raw) < 0)
        return reinterpret_cast<void *>(static_cast<u32>(
            ManagerGetVariablePointer(ctx->manager,
                                      static_cast<i32>(raw))));
    return ctx->chunk + 8U + raw + ctx->arg_base;
}

// ---------------------------------------------------------------------------
// 0x00450070
// ---------------------------------------------------------------------------
void *EclVmResolveStringArg(EclRunContext *ctx, u32 arg_index)
{
    const u32 raw = RawArg(ctx, arg_index);
    if (!ArgMaskBit(ctx, arg_index))
        return 0;
    const float value = *reinterpret_cast<const float *>(&raw);
    if (value >= 30.0f)
        return ctx->chunk + ctx->arg_base +
               static_cast<u32>(FloatToI32Truncate(value)) + 8U;
    return ManagerGetStringPointer(
        ctx->manager, static_cast<long long>(static_cast<i32>(value)));
}

// ---------------------------------------------------------------------------
// 0x00450160
// ---------------------------------------------------------------------------
EclContextNode *FindEclContextNodeById(void *manager, u32 script_id)
{
    EclContextNode *node = reinterpret_cast<EclContextNode *>(
        static_cast<u8 *>(manager) + 0x1030U);
    if (node == 0)
        return 0;
    while (node->context->script_id != script_id) {
        node = node->next;
        if (node == 0)
            return 0;
    }
    return node;
}

// ---------------------------------------------------------------------------
// 0x0044df70
//
// Begins a sub-context frame. Decompile-verified ordering:
//  1. Entry cursor 0 seeds three zero dwords at chunk[0..12), cursor 12;
//     otherwise the saved dword is popped and rewritten one slot below the
//     entry cursor, followed by the parent pointer and instruction pointer
//     at chunk[cursor], each guarded by cursor + 4 < 4096.
//  2. Arguments from first_arg_index+1 up to the count byte at ins+0x0b are
//     copied as 8-byte {type tag, value} pairs: 'f'/'g' tags evaluate as
//     float (0x44ff80 with the raw dword as default), other tags evaluate as
//     int (0x44ff00); the destination width byte at tag+1 ('f') selects a
//     verbatim float store over a _ftol2 conversion.
//  3. The label name at ins+0x14 is resolved through 0x450470 with the
//     manager's root record; the result becomes the new current instruction
//     (0 = failure, which also clears the parent's instruction pointer and
//     reports -1).
// ---------------------------------------------------------------------------
i32 BeginEclSubFrame(EclRunContext *ctx, float initial_value,
                     EclRunContext *parent, u32 first_arg_index)
{
    (void)initial_value;
    EclInstruction *ins = parent->ins;
    const u8 *ins_bytes = reinterpret_cast<const u8 *>(ins);
    u32 cursor = ctx->stack_cursor;
    const bool empty_entry = (cursor == 0U);

    if (empty_entry) {
        StoreU32At(ctx->chunk, 0U); // chunk[0]
        ctx->stack_cursor = 4U;
    }

    // Argument copy loop. Pair base: type byte at
    // ins + args0 + 4*index + 20, value dword at +4.
    const u32 args0 = LoadU32At(ins_bytes + 0x10U);
    const i32 arg_count = static_cast<i32>(ins_bytes[0x0BU]);
    for (i32 arg = static_cast<i32>(first_arg_index) + 1;
         arg < arg_count; ++arg) {
        const u32 pair_base = args0 + 4U * static_cast<u32>(arg) + 20U;
        const u8 tag = ins_bytes[pair_base];
        const u8 dest_tag = ins_bytes[pair_base + 1U];
        const u32 raw = LoadU32At(ins_bytes + pair_base + 4U);
        u32 stored;
        if (tag == k_type_float || tag == 0x67U /* 'g' */) {
            const float fetched = EclVmEvalFloatArg(
                parent, static_cast<u32>(arg),
                *reinterpret_cast<const float *>(&raw));
            if (dest_tag == k_type_float) {
                stored = *reinterpret_cast<const u32 *>(&fetched);
            } else {
                stored = static_cast<u32>(FloatToI32Truncate(fetched));
            }
        } else {
            const i32 fetched =
                EclVmEvalIntArg(parent, static_cast<u32>(arg));
            if (dest_tag == k_type_float) {
                const float widened = static_cast<float>(fetched);
                stored = *reinterpret_cast<const u32 *>(&widened);
            } else {
                stored = static_cast<u32>(fetched);
            }
        }
        if (cursor + 4U < k_chunk_limit) {
            StoreU32At(ctx->chunk + cursor, stored);
            cursor += 4U;
            ctx->stack_cursor = cursor;
        }
    }

    if (!empty_entry) {
        // Pop the saved dword, then rewrite the frame header.
        const i32 pop_cursor = static_cast<i32>(ctx->stack_cursor) - 4;
        u32 saved = 0U;
        if (pop_cursor >= 0) {
            ctx->stack_cursor = static_cast<u32>(pop_cursor);
            saved = StackEntryValue(ctx, static_cast<u32>(pop_cursor));
        }
        ctx->stack_cursor = cursor;
        if (cursor >= 4U)
            StoreU32At(ctx->chunk + cursor - 4U, saved);
        if (cursor + 4U < k_chunk_limit) {
            StoreU32At(ctx->chunk + cursor,
                       *reinterpret_cast<const u32 *>(&parent));
            ctx->stack_cursor = cursor + 4U;
            cursor += 4U;
        }
        if (cursor + 4U < k_chunk_limit) {
            StoreU32At(ctx->chunk + cursor,
                       reinterpret_cast<u32>(ins));
            ctx->stack_cursor = cursor + 4U;
        }
    } else {
        ctx->stack_cursor = 4U;
        StoreU32At(ctx->chunk + 4U, 0U); // chunk[4] (parent slot)
        ctx->stack_cursor = 8U;
        if (8U + 4U < k_chunk_limit) {
            StoreU32At(ctx->chunk + 8U, reinterpret_cast<u32>(ins));
            ctx->stack_cursor = 12U;
        }
    }

    // Publish the new context, resolve the label, and arm the instruction.
    void *manager = *reinterpret_cast<void **>(
        reinterpret_cast<u32 *>(parent) + 1029U);
    u32 *current_slot = static_cast<u32 *>(manager) + 1U;
    const u32 previous_current = *current_slot;
    *current_slot = reinterpret_cast<u32>(ctx);
    const i32 resolved = ResolveScriptTableIndexEaxAbi(
        static_cast<i32>(*reinterpret_cast<u32 *>(
            static_cast<u8 *>(manager) + 0x1030U)));
    // Native order: instruction first, then the zeroed time dword; the
    // resolved value is re-read from the instruction slot for the check.
    ctx->ins = reinterpret_cast<EclInstruction *>(
        static_cast<u32>(resolved));
    *reinterpret_cast<u32 *>(ctx) = 0U; // ctx->time = 0
    if (ctx->ins != 0) {
        *current_slot = previous_current;
        return 0;
    }
    // Failure: the parent's current instruction is cleared and the new
    // context stays published as the manager's current record.
    parent->ins = 0;
    return -1;
}

// ---------------------------------------------------------------------------
// 0x004500d0
// ---------------------------------------------------------------------------
i32 SpawnEclSubContext(void *manager, float initial_value, u32 script_id,
                       u32 first_arg_index)
{
    u8 *raw = static_cast<u8 *>(NativeAlloc(0x1024U));
    EclRunContext *ctx = 0;
    if (raw != 0) {
        u32 *words = reinterpret_cast<u32 *>(raw);
        words[1026] = 0U; // stack cursor
        words[1027] = 0U; // argument base
        ctx = reinterpret_cast<EclRunContext *>(raw);
    }
    EclContextNode *node = static_cast<EclContextNode *>(NativeAlloc(0xCU));
    u32 *ctx_words = reinterpret_cast<u32 *>(ctx);
    ctx_words[1028] = script_id;                    // +0x1010
    ctx_words[1029] = reinterpret_cast<u32>(manager); // +0x1014
    ctx_words[0] = 0U;
    ctx_words[1] = 0U;
    ctx->rank_mask = *reinterpret_cast<const u8 *>(
        *reinterpret_cast<u32 *const *>(manager)[1] + 0x101CU);
    node->context = ctx;
    node->next = 0;
    node->prev = 0;
    EclContextNode *head = *reinterpret_cast<EclContextNode **>(
        static_cast<u8 *>(manager) + 0x1034U);
    if (head != 0) {
        node->next = head;
        head->prev = node;
    }
    *reinterpret_cast<EclContextNode **>(static_cast<u8 *>(manager) +
                                         0x1034U) = node;
    node->prev = reinterpret_cast<EclContextNode *>(
        static_cast<u8 *>(manager) + 0x1030U);
    return BeginEclSubFrame(
        ctx, initial_value,
        *reinterpret_cast<EclRunContext **>(static_cast<u8 *>(manager) + 4U),
        first_arg_index);
}

// ---------------------------------------------------------------------------
// 0x0044fd10
// ---------------------------------------------------------------------------
i32 RunEclContextListEdiStackAbi(void *manager, float delta)
{
    u8 *mgr = static_cast<u8 *>(manager);
    EclContextNode *node = reinterpret_cast<EclContextNode *>(mgr + 0x1030U);
    i32 first = 1;
    for (;;) {
        EclContextNode *next = node->next;
        *reinterpret_cast<u32 **>(mgr + 4U) =
            *reinterpret_cast<u32 **>(node);
        EclRunContext *ctx = *reinterpret_cast<EclRunContext **>(mgr + 4U);
        if (first == 0) {
            if (ExecuteEclInstruction(ctx, delta) != 0) {
                NativeFree(ctx);
                if (node->next != 0)
                    node->next->prev = node->prev;
                if (node->prev != 0)
                    node->prev->next = node->next;
                node->next = 0;
                node->prev = 0;
                NativeFree(node);
            }
        } else {
            // Root record: a failure aborts the whole walk with -1 and the
            // root is never freed here.
            if (ExecuteEclInstruction(ctx, delta) != 0)
                return -1;
            first = 0;
        }
        node = next;
        if (next == 0)
            break;
    }
    *reinterpret_cast<u32 **>(mgr + 4U) =
        reinterpret_cast<u32 *>(mgr + 8U);
    return 0;
}

// ---------------------------------------------------------------------------
// 0x0044e1a0
// ---------------------------------------------------------------------------
i32 ExecuteEclInstruction(EclRunContext *ctx, float delta)
{
    EclInstruction *ins = ctx->ins;
    if (ins == 0)
        return -1;

    // Time gate (fild of the integer tick, fcomp against ctx->time; the
    // parity branch at 0x44fb96 keeps the context waiting).
    if (static_cast<float>(ins->time) <= ctx->time)
        return 0;

    // Rank gate: instruction rank byte must intersect the context rank;
    // otherwise fall through to the advance tail (0x44fb77).
    if ((ctx->rank_mask & ins->rank_mask) == 0U)
        return 0;

    switch (ins->opcode) {
    case 10: {
        // Return: pops the {instruction, time} frame pushed by opcode 11.
        EclVmPopTopEaxAbi(ctx->chunk);
        if (ctx->stack_cursor == 0U)
            return 1; // 0x44fbb0: nothing to return to; context ends.
        i32 cursor = static_cast<i32>(ctx->stack_cursor) - 4;
        if (cursor >= 0) {
            ctx->stack_cursor = static_cast<u32>(cursor);
            const u32 value = StackEntryValue(ctx, static_cast<u32>(cursor));
            if (value != 0U)
                ctx->ins = reinterpret_cast<EclInstruction *>(value);
        }
        cursor = static_cast<i32>(ctx->stack_cursor) - 4;
        if (cursor >= 0) {
            ctx->stack_cursor = static_cast<u32>(cursor);
            const u32 value = StackEntryValue(ctx, static_cast<u32>(cursor));
            *reinterpret_cast<u32 *>(&ctx->time) = value;
        }
        if (ctx->ins == 0)
            return 1; // 0x44fbb2: ended.
        return 0;
    }
    case 11:
        // Call: pushes the return frame onto the context's own chunk.
        if (BeginEclSubFrame(ctx, delta, ctx, 0U) != 0)
            return 1; // 0x44fbb2.
        return 0;
    case 12:
        ApplyJump(ctx);
        return 0;
    case 13: {
        // Jump when the popped value is zero.
        i32 value = 0;
        PopInt(ctx, &value);
        if (value == 0)
            ApplyJump(ctx);
        return 0;
    }
    case 14: {
        // Jump when the popped value is non-zero.
        i32 value = 0;
        PopInt(ctx, &value);
        if (value != 0)
            ApplyJump(ctx);
        return 0;
    }
    case 15:
        SpawnEclSubContext(ctx->manager, delta, static_cast<u32>(-1), 0U);
        return 0;
    case 16: {
        // Start sub-script: arg 0 is a byte size, the dword after it is the
        // script id; arguments from index 1 move into the new frame.
        const u32 size = RawArg(ctx, 0U);
        const u32 id = LoadU32At(reinterpret_cast<const u8 *>(ins) +
                                 0x10U + 4U * ((size + 4U) >> 2));
        SpawnEclSubContext(ctx->manager, delta, id, 1U);
        return 0;
    }
    case 17: {
        const i32 id = EclVmEvalIntArgFromIns(ctx, 0U);
        EclContextNode *node = FindEclContextNodeById(
            ctx->manager, static_cast<u32>(id));
        if (node != 0)
            node->context->ins = 0;
        return 0;
    }
    case 18: {
        const i32 id = EclVmEvalIntArgFromIns(ctx, 0U);
        EclContextNode *node = FindEclContextNodeById(
            ctx->manager, static_cast<u32>(id));
        if (node != 0)
            node->context->flags |= 1U;
        return 0;
    }
    case 19: {
        const i32 id = EclVmEvalIntArgFromIns(ctx, 0U);
        EclContextNode *node = FindEclContextNodeById(
            ctx->manager, static_cast<u32>(id));
        if (node != 0)
            node->context->flags &= ~1U;
        return 0;
    }
    case 20: {
        const i32 id = EclVmEvalIntArgFromIns(ctx, 0U);
        EclContextNode *node = FindEclContextNodeById(
            ctx->manager, static_cast<u32>(id));
        if (node != 0)
            node->context->field_1018 =
                static_cast<u32>(EclVmEvalIntArgFromIns(ctx, 1U));
        return 0;
    }
    case 21:
        EclVmOpcode21EaxAbi(ctx->manager);
        return 0;
    case 40:
        EclVmPushIntEaxEcxAbi(ctx->chunk, EclVmEvalIntArgFromIns(ctx, 0U));
        return 0;
    case 41:
        EclVmPopTopEaxAbi(ctx->chunk);
        return 0;
    case 42:
        PushInt(ctx, EclVmEvalIntArgFromIns(ctx, 0U));
        return 0;
    case 43: {
        // Store int variable.
        void *slot = EclVmResolvePointerArg(ctx, 0U);
        if (slot == 0)
            return 0;
        i32 value = 0;
        bool was_int = false;
        float popped = 0.0f;
        if (PopFloatTagged(ctx, &popped, &was_int)) {
            if (was_int)
                *reinterpret_cast<u32 *>(slot) =
                    *reinterpret_cast<const u32 *>(&popped);
            else
                *reinterpret_cast<u32 *>(slot) = static_cast<u32>(
                    FloatToI32Truncate(popped));
        }
        return 0;
    }
    case 44:
        PushFloat(ctx, EclVmEvalFloatArgFromIns(ctx, 0U));
        return 0;
    case 45: {
        // Store float variable (int-tagged stack entries convert).
        void *slot = EclVmResolveStringArg(ctx, 0U);
        if (slot == 0)
            return 0;
        float value = 0.0f;
        bool was_int = false;
        if (PopFloatTagged(ctx, &value, &was_int)) {
            if (was_int)
                *reinterpret_cast<u32 *>(slot) = static_cast<u32>(
                    FloatToI32Truncate(value));
            else
                *reinterpret_cast<u32 *>(slot) =
                    *reinterpret_cast<const u32 *>(&value);
        }
        return 0;
    }
    case 50: {
        i32 a = 0, b = 0;
        if (!PopTwoInts(ctx, &a, &b))
            return 0;
        PushInt(ctx, a + b);
        return 0;
    }
    case 52: {
        i32 a = 0, b = 0;
        if (!PopTwoInts(ctx, &a, &b))
            return 0;
        PushInt(ctx, b - a);
        return 0;
    }
    case 54: {
        i32 a = 0, b = 0;
        if (!PopTwoInts(ctx, &a, &b))
            return 0;
        PushInt(ctx, a * b);
        return 0;
    }
    case 56: {
        i32 a = 0, b = 0;
        if (!PopTwoInts(ctx, &a, &b))
            return 0;
        PushInt(ctx, b / a);
        return 0;
    }
    case 58: {
        i32 a = 0, b = 0;
        if (!PopTwoInts(ctx, &a, &b))
            return 0;
        PushInt(ctx, b % a);
        return 0;
    }
    case 62: {
        // Comparison quirk: pushes 1 only for an unordered (NaN) result.
        float a = 0.0f, b = 0.0f;
        if (!PopTwoFloats(ctx, &a, &b))
            return 0;
        const bool ordered = (a < b) || (a > b) || (a == b);
        PushInt(ctx, ordered ? 0 : 1);
        return 0;
    }
    case 64: {
        float a = 0.0f, b = 0.0f;
        if (!PopTwoFloats(ctx, &a, &b))
            return 0;
        PushInt(ctx, b >= a ? 1 : 0);
        return 0;
    }
    case 66: {
        float a = 0.0f, b = 0.0f;
        if (!PopTwoFloats(ctx, &a, &b))
            return 0;
        PushInt(ctx, b > a ? 1 : 0);
        return 0;
    }
    case 68: {
        float a = 0.0f, b = 0.0f;
        if (!PopTwoFloats(ctx, &a, &b))
            return 0;
        PushInt(ctx, b <= a ? 1 : 0);
        return 0;
    }
    case 70: {
        float a = 0.0f, b = 0.0f;
        if (!PopTwoFloats(ctx, &a, &b))
            return 0;
        PushInt(ctx, b < a ? 1 : 0);
        return 0;
    }
    case 72: {
        // Compare against the constant flt_470b04 = 0.0f; pushes 1 for an
        // ordered not-equal result.
        float a = 0.0f;
        if (!PopFloatTagged(ctx, &a, 0))
            return 0;
        const bool ordered_ne = (a == a) && a != 0.0f;
        PushInt(ctx, ordered_ne ? 1 : 0);
        return 0;
    }
    case 73: {
        i32 a = 0, b = 0;
        if (!PopTwoInts(ctx, &a, &b))
            return 0;
        PushInt(ctx, (a != 0 || b != 0) ? 1 : 0);
        return 0;
    }
    case 74: {
        i32 a = 0, b = 0;
        if (!PopTwoInts(ctx, &a, &b))
            return 0;
        PushInt(ctx, (a != 0 && b != 0) ? 1 : 0);
        return 0;
    }
    case 75: {
        i32 a = 0, b = 0;
        if (!PopTwoInts(ctx, &a, &b))
            return 0;
        PushInt(ctx, a ^ b);
        return 0;
    }
    case 76: {
        i32 a = 0, b = 0;
        if (!PopTwoInts(ctx, &a, &b))
            return 0;
        PushInt(ctx, a | b);
        return 0;
    }
    case 77: {
        i32 a = 0, b = 0;
        if (!PopTwoInts(ctx, &a, &b))
            return 0;
        PushInt(ctx, a & b);
        return 0;
    }
    case 78: {
        // Decrement variable: initial from argument 0, slot from argument
        // 0 as well (both resolved with the mask rules); stores n-1 and
        // pushes the pre-decrement value.
        const i32 initial = EclVmEvalIntArgFromIns(ctx, 0U);
        void *slot = EclVmResolvePointerArg(ctx, 0U);
        const i32 decremented = initial - 1;
        if (slot != 0)
            *reinterpret_cast<u32 *>(slot) =
                static_cast<u32>(decremented);
        PushInt(ctx, initial);
        return 0;
    }
    case 79: {
        // fsin of the popped value (int-tagged entries widen).
        float a = 0.0f;
        if (!PopFloatTagged(ctx, &a, 0))
            return 0;
        PushFloat(ctx, static_cast<float>(sin(static_cast<double>(a))));
        return 0;
    }
    case 80: {
        float a = 0.0f;
        if (!PopFloatTagged(ctx, &a, 0))
            return 0;
        PushFloat(ctx, static_cast<float>(cos(static_cast<double>(a))));
        return 0;
    }
    case 81: {
        // Polar helper: evaluates args 3/2, wraps the arg-2 angle, runs the
        // 0x4501b0 vector helper, and stores the pair into the arg 0/1
        // slots.
        const float value = EclVmEvalFloatArgFromIns(ctx, 3U);
        const float angle = WrapAngleToPi(
            EclVmEvalFloatArgFromIns(ctx, 2U));
        float pair[2] = {0.0f, 0.0f};
        EclVmOpcode81HelperEcxEfxAbi(pair, angle, value);
        *reinterpret_cast<u32 *>(EclVmResolveStringArg(ctx, 0U)) =
            *reinterpret_cast<const u32 *>(&pair[0]);
        *reinterpret_cast<u32 *>(EclVmResolveStringArg(ctx, 1U)) =
            *reinterpret_cast<const u32 *>(&pair[1]);
        return 0;
    }
    case 83: {
        // Time shift: ctx->time -= (float)evaluated integer argument.
        const i32 value = EclVmEvalIntArgFromIns(ctx, 0U);
        ctx->time -= static_cast<float>(value);
        return 0;
    }
    case 84: {
        i32 a = 0;
        if (!PopInt(ctx, &a))
            return 0;
        PushInt(ctx, -a);
        return 0;
    }
    case 85: {
        float a = 0.0f;
        if (!PopFloatTagged(ctx, &a, 0))
            return 0;
        PushFloat(ctx, -a);
        return 0;
    }
    case 86: {
        // var(arg0) = arg1^2 + arg2^2.
        const float a = EclVmEvalFloatArgFromIns(ctx, 1U);
        const float b = EclVmEvalFloatArgFromIns(ctx, 2U);
        *reinterpret_cast<float *>(EclVmResolveStringArg(ctx, 0U)) =
            a * a + b * b;
        return 0;
    }
    default:
        // Opcodes 0 (advance), 1 (end), 2-9, 22-29, 30 (Val/Str/Block
        // debug print), 31-39, 46-49, 51/53/55/57 (float arithmetic),
        // 59/60/61/63/65/67/69/71 (remaining compares/jumps), 82, 87:
        // bodies not yet byte-verified; the native default falls through
        // to the advance tail.
        return 0;
    }
}

} // namespace th10
