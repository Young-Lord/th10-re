// TH10 color-record math, Float3 copy helpers, and tiny structural
// setters found across the 0x401f00-0x412800 range.
//
// The 6-float color record (Zun color: r g b a r2 g2 at +0x0..+0x14 plus
// four byte quantizations of the last four floats at +0x18..+0x1b) is
// manipulated by 0x404d40 (subtract), 0x404da0 (scale), 0x404e10 (add)
// and 0x405040 (set). The Float3 copies write a source triple into a
// fixed destination offset of a caller-selected record.

#include "Th10Platform.hpp"

namespace th10 {

struct ColorRecord6 {
    float values[6];
    u8 quantized[4];
};

struct Float3Raw {
    float x;
    float y;
    float z;
};

namespace {

// TH10 __ftol2 (0x463b2c) boundary: truncate the FPU top toward zero
// into the low byte. Modeled with the C cast chain the native produces.
u8 FloatToByte(float value) {
    return static_cast<u8>(static_cast<i32>(value));
}

void StoreQuantizedTail(ColorRecord6 *out, const float *src) {
    out->quantized[0] = FloatToByte(src[2]);
    out->quantized[1] = FloatToByte(src[3]);
    out->quantized[2] = FloatToByte(src[4]);
    out->quantized[3] = FloatToByte(src[5]);
}

} // namespace

// TH10 0x00404d40. Native EAX = minuend, ECX = subtrahend, ESI = out
// (register triple). out[i] = a[i] - b[i] for the six floats, then the
// four bytes are re-quantized from the result.
ColorRecord6 *SubtractColorRecord6EsiAbi(ColorRecord6 *out,
                                         const ColorRecord6 *a,
                                         const ColorRecord6 *b) {
    ColorRecord6 result;
    for (int i = 0; i < 6; ++i) {
        result.values[i] = a->values[i] - b->values[i];
    }
    StoreQuantizedTail(out, result.values);
    for (int i = 0; i < 6; ++i) {
        out->values[i] = result.values[i];
    }
    return out;
}

// TH10 0x00404da0. Native EAX = source, ESI = out, stack = scale (ret 4).
// out[i] = src[i] * scale, bytes re-quantized.
ColorRecord6 *ScaleColorRecord6StackAbi(ColorRecord6 *out,
                                        const ColorRecord6 *src,
                                        float scale) {
    ColorRecord6 result;
    for (int i = 0; i < 6; ++i) {
        result.values[i] = src->values[i] * scale;
    }
    StoreQuantizedTail(out, result.values);
    for (int i = 0; i < 6; ++i) {
        out->values[i] = result.values[i];
    }
    return out;
}

// TH10 0x00404e10. Native EAX = a, ECX = b, ESI = out. out[i] = a[i]+b[i],
// bytes re-quantized.
ColorRecord6 *AddColorRecord6EsiAbi(ColorRecord6 *out,
                                    const ColorRecord6 *a,
                                    const ColorRecord6 *b) {
    ColorRecord6 result;
    for (int i = 0; i < 6; ++i) {
        result.values[i] = a->values[i] + b->values[i];
    }
    StoreQuantizedTail(out, result.values);
    for (int i = 0; i < 6; ++i) {
        out->values[i] = result.values[i];
    }
    return out;
}

// TH10 0x00405040. Native ESI = out, six stack floats (ret 0x18). Stores
// all six floats and quantizes bytes 0..3 from floats 2..5.
ColorRecord6 *SetColorRecord6StackAbi(ColorRecord6 *out, float v0, float v1,
                                      float v2, float v3, float v4,
                                      float v5) {
    out->values[0] = v0;
    out->values[1] = v1;
    out->values[2] = v2;
    out->values[3] = v3;
    out->values[4] = v4;
    out->values[5] = v5;
    const float tail[6] = {v0, v1, v2, v3, v4, v5};
    StoreQuantizedTail(out, tail);
    return out;
}

// TH10 0x004050a0 twin kept for reference: re-quantize the four bytes of
// an existing record from its floats 2..5 (not in this batch; provided
// because 0x405040's contract depends on the same relation).
void QuantizeColorRecord6Bytes(ColorRecord6 *record) {
    record->quantized[0] = FloatToByte(record->values[2]);
    record->quantized[1] = FloatToByte(record->values[3]);
    record->quantized[2] = FloatToByte(record->values[4]);
    record->quantized[3] = FloatToByte(record->values[5]);
}

// ---- Float3 copy helpers -------------------------------------------------

namespace {

void CopyFloat3(u8 *destination, const float source[3]) {
    float *out = reinterpret_cast<float *>(destination);
    out[0] = source[0];
    out[1] = source[1];
    out[2] = source[2];
}

} // namespace

// TH10 0x00405000. Native EAX = source triple, ECX = record; copies the
// triple into record+0x24.
void CopyFloat3ToOffset24EaxEcxAbi(void *record, const float source[3]) {
    CopyFloat3(static_cast<u8 *>(record) + 0x24, source);
}

// TH10 0x00405020. Same, destination record+0x18.
void CopyFloat3ToOffset18EaxEcxAbi(void *record, const float source[3]) {
    CopyFloat3(static_cast<u8 *>(record) + 0x18, source);
}

// TH10 0x00405110. Same, destination record+0xc.
void CopyFloat3ToOffset0cEaxEcxAbi(void *record, const float source[3]) {
    CopyFloat3(static_cast<u8 *>(record) + 0xc, source);
}

// TH10 0x00405130. Same, destination record+0x0.
void CopyFloat3ToOffset00EaxEcxAbi(void *record, const float source[3]) {
    CopyFloat3(static_cast<u8 *>(record), source);
}

// TH10 0x0040b370. Same, destination record+0x340 (ECL script object
// position mirror).
void CopyFloat3ToOffset340EaxEcxAbi(void *record, const float source[3]) {
    CopyFloat3(static_cast<u8 *>(record) + 0x340, source);
}

// TH10 0x0040cee0. Same, destination record+0x24 (second site, bullet
// record layout).
void CopyFloat3ToOffset24SecondEaxEcxAbi(void *record,
                                         const float source[3]) {
    CopyFloat3(static_cast<u8 *>(record) + 0x24, source);
}

// TH10 0x0040cf60. Same, destination record+0x430.
void CopyFloat3ToOffset430EaxEcxAbi(void *record, const float source[3]) {
    CopyFloat3(static_cast<u8 *>(record) + 0x430, source);
}

// TH10 0x004053b0. Native EAX = out, three stack floats (ret 0xc).
void SetFloat3StackAbi(Float3Raw *out, float x, float y, float z) {
    out->x = x;
    out->y = y;
    out->z = z;
}

// TH10 0x0040c940. Native EAX = out: zero the triple.
void ZeroFloat3EaxAbi(Float3Raw *out) {
    out->x = 0.0f;
    out->y = 0.0f;
    out->z = 0.0f;
}

// TH10 0x00401f40. Native EAX = out: the sentinel triple used for "no
// entry": (-999999, 0, 0) as raw dwords.
void SetSentinelTripleEaxAbi(void *out) {
    u32 *values = static_cast<u32 *>(out);
    values[0] = 0xfff0bdc1U; // -999999
    values[1] = 0;
    values[2] = 0;
}

// TH10 0x00401f90. Native EAX = out, ECX = first dword: (value, 0, 0).
void SetFirstDwordTripleEaxEcxAbi(void *out, u32 value) {
    u32 *values = static_cast<u32 *>(out);
    values[0] = value;
    values[1] = 0;
    values[2] = 0;
}

// TH10 0x0040c920. Native EAX = counter, ECX = amount: subtract and
// return the new value.
i32 SubtractCounterEcxEaxAbi(i32 *counter, i32 amount) {
    *counter -= amount;
    return *counter;
}

// TH10 0x00409e10. Native ECX = record: index arithmetic
// (record[+0x28] * 3 + record[+0x2c]).
i32 ComputeTriplePlusIndexEcxEcxAbi(const void *record) {
    const u32 *fields = static_cast<const u32 *>(record);
    return static_cast<i32>(fields[0x28 / 4] * 3u + fields[0x2c / 4]);
}

// TH10 0x00412790. Native EAX = index, ECX = base, EDX = value: stores
// the dword at base + (index + 0x24a) * 0x10.
void StoreIndexedDword0x10StrideEaxEcxEdxAbi(u32 index, void *base,
                                             u32 value) {
    *reinterpret_cast<u32 *>(static_cast<u8 *>(base) +
                             (index + 0x24a) * 0x10) = value;
}

// TH10 0x00401fd0. Native ECX = cursor, EAX = count, stack = stride,
// EBX = callback. Walks `count` stride-sized slots calling the callback
// (which receives ECX = the current slot) on each; negative or zero
// counts do nothing.
typedef void (TH10_FASTCALL *StrideSlotCallback)(void *cursor);

void WalkStrideSlotsEaxEbxAbi(void *cursor, i32 count, u32 stride,
                              StrideSlotCallback callback) {
    --count;
    if (count < 0) {
        return;
    }
    i32 remaining = count + 1;
    while (remaining != 0) {
        callback(cursor);
        cursor = static_cast<u8 *>(cursor) + stride;
        --remaining;
    }
}

} // namespace th10
