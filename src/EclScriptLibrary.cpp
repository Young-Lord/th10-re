#include "EclScriptLibrary.hpp"

namespace th10 {

namespace {

// TH10 0x474c74. Current difficulty index; the ECL object stores
// 1 << index in its +0x1024 difficulty bitmask byte.
const u32 k_difficulty_index = 0x474c74U;

u32 ReadGlobalU32(u32 address)
{
    const u8 *const source = reinterpret_cast<const u8 *>(address);
    return static_cast<u32>(source[0]) | (static_cast<u32>(source[1]) << 8)
         | (static_cast<u32>(source[2]) << 16)
         | (static_cast<u32>(source[3]) << 24);
}

void StoreU32(u8 *record, u32 offset, u32 value)
{
    record[offset + 0] = static_cast<u8>(value);
    record[offset + 1] = static_cast<u8>(value >> 8);
    record[offset + 2] = static_cast<u8>(value >> 16);
    record[offset + 3] = static_cast<u8>(value >> 24);
}

u32 LoadU32(const u8 *record, u32 offset)
{
    return static_cast<u32>(record[offset])
         | (static_cast<u32>(record[offset + 1]) << 8)
         | (static_cast<u32>(record[offset + 2]) << 16)
         | (static_cast<u32>(record[offset + 3]) << 24);
}

// Replaces one bit in the +0x2480 flag dword with an incoming bit.
void ReplaceFlagBit(u8 *record, u32 bit, u32 incoming)
{
    const u32 flags = LoadU32(record, 0x2480U);
    StoreU32(record, 0x2480U, (flags & ~bit) | (incoming ? bit : 0U));
}

} // namespace

void *CreateEclScriptObjectEaxStackAbi(const u32 *descriptor,
                                       void *list_owner, i32 ctor_arg)
{
    void *const raw = ::operator new(0x2518U);
    u8 *record;
    if (raw != 0) {
        record = static_cast<u8 *>(ConstructEclScriptObjectEsiStackAbi(
            raw, ctor_arg));
    } else {
        record = 0;
    }

    // The native keeps writing through the (possibly null) record.
    StoreU32(record, 0x1094U, descriptor[0]);
    StoreU32(record, 0x1098U, descriptor[1]);
    StoreU32(record, 0x109cU, descriptor[2]);
    StoreU32(record, 0x23f8U, descriptor[3]);
    StoreU32(record, 0x23fcU, descriptor[5]);
    StoreU32(record, 0x2408U, descriptor[4]);
    ReplaceFlagBit(record, 0x800U, descriptor[6] & 1U);

    const u32 difficulty = ReadGlobalU32(k_difficulty_index);
    record[0x1024U] = static_cast<u8>(1U << difficulty);

    for (u32 i = 0; i < 0x20U; ++i) {
        record[0x1138U + i] =
            reinterpret_cast<const u8 *>(descriptor + 8)[i];
    }

    // Score-anim block: initialize once (flag bit 0 at +0x2468), then
    // unconditionally arm it with mode 2 and the 2.0f rate.
    const u32 anim_state = LoadU32(record, 0x2468U);
    if ((anim_state & 1U) == 0U) {
        StoreU32(record, 0x245cU, 0U);
        StoreU32(record, 0x2458U, static_cast<u32>(-999999));
        StoreU32(record, 0x2460U, 0U);
        StoreU32(record, 0x2464U, 0x476f78U); // &flt_476F78
        StoreU32(record, 0x2468U, anim_state | 1U);
    }
    StoreU32(record, 0x245cU, 2U);
    StoreU32(record, 0x2460U, 0x40000000U);
    StoreU32(record, 0x2458U, 1U);

    ReplaceFlagBit(record, 0x40000U, descriptor[7] & 1U);

    RunEclScriptSetupStackAbi(record + 0x1044U);

    // Bit 0x8000 of the flag dword remaps the +0x2408 kind (1 -> 10,
    // 4 -> 11).
    if ((LoadU32(record, 0x2480U) & 0x8000U) != 0U) {
        u32 kind = LoadU32(record, 0x2408U);
        if (kind == 1U) {
            kind = 10U;
        } else if (kind == 4U) {
            kind = 11U;
        }
        StoreU32(record, 0x2408U, kind);
    }

    // Presentation pair: par-count kind from the owner list parity and a
    // per-file accent value driven by the descriptor's embedded kind pair
    // (only when record+0x1128 == 1).
    u8 *const owner = static_cast<u8 *>(list_owner);
    StoreU32(record, 0x2444U,
             (LoadU32(owner, 0x64U) & 1U) + 2U);
    StoreU32(record, 0x2448U, 359U);
    if (LoadU32(record, 0x1128U) == 1U) {
        switch (LoadU32(record, 0x112cU)) {
        case 0x00U:
        case 0x14U:
        case 0x31U:
            StoreU32(record, 0x2448U, 359U);
            break;
        case 0x05U:
        case 0x19U:
        case 0x32U:
            StoreU32(record, 0x2448U, 356U);
            break;
        case 0x0aU:
        case 0x1eU:
        case 0x33U:
            StoreU32(record, 0x2448U, 362U);
            break;
        case 0x0fU:
        case 0x23U:
            StoreU32(record, 0x2448U, 365U);
            break;
        default:
            break;
        }
    }
    StoreU32(record, 0x244cU, 0U);

    // Append the embedded list node (record+0x116c; next at +0x1170, prev
    // at +0x1174) after the owner's current last node.
    u8 *const node = record + 0x116cU;
    const u32 head = LoadU32(owner, 0x58U);
    if (head != 0U) {
        const u32 last = LoadU32(owner, 0x5cU);
        const u32 last_next = LoadU32(reinterpret_cast<const u8 *>(last), 4U);
        if (last_next != 0U) {
            StoreU32(node, 4U, last_next);
            StoreU32(reinterpret_cast<u8 *>(last_next), 8U,
                     reinterpret_cast<u32>(node));
        }
        StoreU32(reinterpret_cast<u8 *>(last), 4U,
                 reinterpret_cast<u32>(node));
        StoreU32(node, 8U, last);
    } else {
        StoreU32(owner, 0x58U, reinterpret_cast<u32>(node));
    }
    StoreU32(owner, 0x5cU, reinterpret_cast<u32>(node));
    StoreU32(owner, 0x60U, LoadU32(owner, 0x60U) + 1U);
    StoreU32(owner, 0x64U, LoadU32(owner, 0x64U) + 1U);

    return record;
}

} // namespace th10
