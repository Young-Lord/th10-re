// TH10 ECL/misc small instruction helpers: the title-background script
// group-liveness test (0x403990) and the effect spawn-wait list tick
// (0x40e6a0).

#include "Th10Types.hpp"
#include "StageEffectHelpers.hpp"

namespace th10 {

extern void *g_StageHostObject; // TH10 DAT_004776f8 (stage host; NOT read by
                                // the effect-spawn path below — native
                                // 0x40e6a0 never touches that global)

// Boundaries.
extern i32 FloatToIntBoundary(float value); // TH10 0x463b2c (ftol)
// TH10 0x448db0 raw. Native contract (userpurge): EDI = vec3 position
// (inside 0x448db0 x += g_AsciiObjectScrollX TH10 0x470b4c and
// y += g_AsciiObjectScrollY 0x470b48), ECX = VM/record, EBX = script
// index, EAX = out-id slot, remaining arguments on the stack. The
// reconstruction's leading `void *host` parameter is a placeholder
// mapping, not the native register assignment.
extern i32 SpawnStageEffectRawEdxStackAbi(void *host, i32 script_index,
                                          const float position[3],
                                          void *id_slot); // 0x448db0 raw
// TH10 0x43ee30 liveness probe boundary (record bank entry walker).
extern void IsTrackedRecordLiveBoundary(void *tracked);

namespace {
const float kXOffset224 = 224.0f;    // TH10 0x470b4c
const float kYOffset16 = 16.0f;      // TH10 0x470b48
const float kFrameWindowLow = 0.98f; // TH10 0x470b68
const float kFrameWindowHigh = 1.01f; // TH10 0x470b64
} // namespace

// TH10 0x00403990. Native EBX = the title background script context
// (`ret` with EAX = 0). For each of the context's tracked script records
// ([ebx+0x10] count over the [ebx+0x14] pointer array) carrying flag bit
// 0 and a non-negative sub-entry count at +0x1c: walk the record's sub-
// entry chain (stride word at +0x2, entry slot at +0x6 indexes the
// 0x3ac-stride record bank at [ebx+0x17c]) and count live records
// (0x43ee30 liveness probe; [rec+0x390] nonzero). When a whole record's
// chain has no live entry, clear its flag bit 0. Returns 0.
i32 CheckTitleScriptGroupLivenessEbxAbi(void *context) {
    u8 *bytes = static_cast<u8 *>(context);
    const i32 record_count =
        *reinterpret_cast<i32 *>(*reinterpret_cast<void **>(bytes + 0x10));
    if (record_count <= 0) {
        return 0;
    }
    void **records = *reinterpret_cast<void ***>(bytes + 0x14);
    u8 *record_bank = *reinterpret_cast<u8 **>(bytes + 0x17c);

    for (i32 r = 0; r < record_count; ++r) {
        u8 *record = static_cast<u8 *>(records[r]);
        if ((*record & 1) == 0) {
            continue;
        }
        if (*reinterpret_cast<i32 *>(record + 0x1c) < 0) {
            continue;
        }
        bool any_live = false;
        u8 *entry = record + 0x1c;
        for (;;) {
            const i32 slot =
                *reinterpret_cast<i16 *>(entry + 0x6);
            u8 *tracked = record_bank + slot * 0x3ac;
            IsTrackedRecordLiveBoundary(tracked);
            if (*reinterpret_cast<u32 *>(tracked + 0x390) != 0) {
                any_live = true;
            }
            const i16 stride =
                *reinterpret_cast<i16 *>(entry + 0x2);
            entry += stride;
            if (*reinterpret_cast<i16 *>(entry) < 0) {
                break;
            }
        }
        if (!any_live) {
            *record = static_cast<u8>(*record & ~1u);
        }
    }
    return 0;
}

// TH10 0x0040e6a0. Native EBP = the wait-list owner record, one stack
// argument (ret 4, unused by the body). Walks the record's +0x58 linked
// list (next at node+0x4): when a node's +0x2480 flags intersect the
// "expire" masks (bits 0x10|0x40 in the low byte or 0x4000|0x8000 in the
// high byte) and the low byte is negative-signed, spawns the node's
// effect: when the +0x2448 script index is non-negative, calls 0x448db0
// (call at 0x40e6ef) with EDI = the node's +0x1068 vec3 position, EBX =
// script index, the out-id slot (= this function's own stack argument)
// pushed on the stack, and the VM/record argument fetched at 0x40e6e4
// from g_AsciiHudConditionalState + 0x30 + 4*[node+0x244c]. NOTE: this
// spawn path never reads the DAT_004776f8 stage host — 0x40e6a0 has
// exactly 3 xrefs in the canonical binary and none of the referencing
// sites are effect-spawn code; the +0x244c slot indexes the conditional
// state's VM pointer array, it is not an effect id handed to a host.
// Afterwards the record's +0x40 mirror and frame accumulator advance with
// the (0.98, 1.01) window rule. Returns nothing.
void TickEffectSpawnWaitListEbpStackAbi(void *owner_record) {
    u8 *bytes = static_cast<u8 *>(owner_record);
    u8 *node = *reinterpret_cast<u8 **>(bytes + 0x58);
    while (node != 0) {
        u8 *next = *reinterpret_cast<u8 **>(node + 0x4);
        const u32 flags = *reinterpret_cast<u32 *>(node + 0x2480);
        const bool expire =
            ((flags & 0x50) != 0) || ((flags & 0xc000) != 0);
        const bool signed_low = (flags & 0x80) == 0; // low byte >= 0 test
        if (expire && !signed_low) {
            const i32 script_index =
                *reinterpret_cast<i32 *>(node + 0x2448);
            if (script_index >= 0) {
                const u32 slot = *reinterpret_cast<u32 *>(node + 0x244c);
                i32 id_slot_value = script_index;
                void *id_slot = &id_slot_value; // native reuses the arg slot
                SpawnStageEffectRawEdxStackAbi(
                    g_StageHostObject, script_index, 0, id_slot);
                (void)slot;
            }
            *reinterpret_cast<u32 *>(node + 0x2480) |= 0x20000U;
        }
        node = next;
    }

    // Frame accumulator: +0x40 mirror of the +0x44 counter, +0x48 rate
    // float, +0x4c window reference.
    *reinterpret_cast<u32 *>(bytes + 0x40) =
        *reinterpret_cast<u32 *>(bytes + 0x44);
    i32 &counter = *reinterpret_cast<i32 *>(bytes + 0x44);
    float &rate = *reinterpret_cast<float *>(bytes + 0x48);
    const float reference = *reinterpret_cast<float *>(bytes + 0x4c);
    if (reference > kFrameWindowLow && reference < kFrameWindowHigh) {
        ++counter;
        rate += 1.0f;
    } else {
        rate += reference;
        counter = FloatToIntBoundary(rate);
    }
    (void)kXOffset224;
    (void)kYOffset16;
}

} // namespace th10
