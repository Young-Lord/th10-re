// TH10 0x43af30 — ending MIDI playback scheduler (ending.cpp cluster).
//
// This is the caller side of the event interpreter 0x43b110
// (EndingMidiSequencer.cpp, InterpretEndingMidiEventBlockStackAbi); it is
// published as an update callback in the ending-state function pointer
// table at 0x46f80c. Every update it:
//   1. computes the running playback time in registers: the accumulated
//      time (+0x130:+0x134, written by the interpreter's tempo meta
//      handling) plus `sext64(ticks_per_quarter +0x120) *
//      i64(elapsed_delta +0x128:+0x12c) * 1000 / sext64(tempo +0x124)`
//      (the native __allmul/__allmul/__alldiv chain 0x456640/0x456640/
//      0x456b80). The field itself is NOT stored back here — the native
//      keeps the running value in EBX:ESI and re-derives it after every
//      interpreted event.
//   2. advances the volume fade: while the gate (+0x2e0) is set and the
//      counter (+0x2e8) is below the duration (+0x2e4), the scale (+0x2c8)
//      is 1 - counter/duration and the cached ftol(scale*128) value
//      (+0x2cc) is refreshed through the fade-step helper 0x43b7a0 when it
//      changes; when the counter reaches the duration the scale is stored
//      as 0 directly.
//   3. walks the event-block array (+0x138, stride 0x20, count +0x118) and
//      interprets each active block while its sign-extended +0x04 end time
//      is at or before the running time, re-deriving the time after every
//      event,
//   4. increments the elapsed delta by one tick and, when the walk never
//      saw an active block, calls the stop helper 0x43ace0.
//
// Event blocks (0x20 bytes): +0x00 active flag, +0x04 end time, +0x0c
// running status, +0x14 stream cursor, +0x18/+0x1c saved marks.
#include "EndingMidiPlayer.hpp"

#include "EndingMidiSequencer.hpp"
#include "Th10Types.hpp"

namespace th10 {

namespace {

typedef long long i64;
typedef unsigned long long u64;

const float kOne = 1.0f;     // TH10 flt_470afc
const float kScale = 128.0f; // TH10 flt_470bf4

// TH10 0x463b2c (_ftol2): x87 conversion, round half away from zero.
i32 FloatToI32(float value)
{
    return value >= 0.0f ? static_cast<i32>(value + 0.5f)
                         : static_cast<i32>(value - 0.5f);
}

inline u32 LoadU32At(const void *base, u32 offset)
{
    return *reinterpret_cast<const u32 *>(
        static_cast<const u8 *>(base) + offset);
}

inline void StoreU32At(void *base, u32 offset, u32 value)
{
    *reinterpret_cast<u32 *>(static_cast<u8 *>(base) + offset) = value;
}

inline i32 LoadI32At(const void *base, u32 offset)
{
    return static_cast<i32>(LoadU32At(base, offset));
}

inline i64 LoadI64At(const void *base, u32 offset)
{
    return static_cast<i64>(LoadU32At(base, offset)) |
           (static_cast<i64>(LoadI32At(base, offset + 4U)) << 32);
}

// TH10 0x43b7a0. Native EDI = context plus one stack argument; kept as a
// boundary (it re-emits the faded controller-7 message for the channels
// touched by the new fade value).
void EndingMidiFadeStepEdiStackAbi(void *context, u32 flag);

// TH10 0x43ace0. Native EDI = context; kept as a boundary (the "every block
// is finished" path that stops the playback).
void EndingMidiPlaybackFinishedEdiAbi(void *context);

// Running-time contribution: sext64(+0x120) * i64(+0x128:+0x12c) * 1000 /
// sext64(+0x124). The C++ i64 arithmetic matches the native CRT helpers.
i64 TempoContribution(const u8 *ctx)
{
    const i64 ticks_per_quarter = static_cast<i64>(LoadI32At(ctx, 0x120U));
    const i64 elapsed_delta = LoadI64At(ctx, 0x128U);
    const i64 tempo = static_cast<i64>(LoadI32At(ctx, 0x124U));
    return ticks_per_quarter * elapsed_delta * 1000 / tempo;
}

// Fade update (native 0x43afa6..0x43affa and the 0x43b0fb early out).
void UpdateEndingMidiFade(u8 *ctx)
{
    if (LoadU32At(ctx, 0x2e0U) == 0) {
        return;
    }
    const i32 counter = LoadI32At(ctx, 0x2e8U);
    const i32 duration = LoadI32At(ctx, 0x2e4U);
    if (counter >= duration) {
        StoreU32At(ctx, 0x2c8U, 0U);
        return;
    }

    const float scale =
        kOne - static_cast<float>(counter) / static_cast<float>(duration);
    *reinterpret_cast<float *>(ctx + 0x2c8U) = scale;

    const i32 faded = FloatToI32(scale * kScale);
    if (faded != LoadI32At(ctx, 0x2ccU)) {
        EndingMidiFadeStepEdiStackAbi(ctx, 0);
    }
    StoreU32At(ctx, 0x2ccU, static_cast<u32>(faded));
    StoreU32At(ctx, 0x2e8U, static_cast<u32>(counter + 1));
}

} // namespace

void UpdateEndingMidiPlayback(void *context)
{
    u8 *const ctx = static_cast<u8 *>(context);

    i64 now = LoadI64At(ctx, 0x130U) + TempoContribution(ctx);

    UpdateEndingMidiFade(ctx);

    const i32 block_count = LoadI32At(ctx, 0x118U);
    const u8 *const blocks =
        reinterpret_cast<const u8 *>(LoadU32At(ctx, 0x138U));

    // Native quirk: the "any active block" flag ([esp+0x14]) is only ever
    // *set* inside the walk; when the walk never sees an active block the
    // flag keeps the stale value of the __allmul argument slot, which holds
    // sext32(+0x120) (the ticks-per-quarter scalar). Reproduced literally.
    i32 any_active = LoadI32At(ctx, 0x120U);

    for (i32 index = 0; index < block_count; ++index) {
        u8 *const block = const_cast<u8 *>(blocks) + 0x20U * index;
        if (LoadU32At(block, 0U) == 0) {
            continue;
        }
        any_active = 1;

        for (;;) {
            // The native sign-extends the 32-bit end time (cdq) and compares
            // the pair against the running time with unsigned piecewise
            // compares; both values are small positives, so the i64 compare
            // below is equivalent.
            const i64 block_end = static_cast<i64>(LoadI32At(block, 4U));
            if (block_end > now) {
                break;
            }

            InterpretEndingMidiEventBlockStackAbi(ctx, block);

            // Re-derive the running time from the (possibly tempo-updated)
            // accumulated field after every interpreted event.
            now = LoadI64At(ctx, 0x130U) + TempoContribution(ctx);

            if (LoadU32At(block, 0U) == 0) {
                break;
            }
        }
    }

    // Elapsed delta += 1 (64-bit increment).
    const u64 delta = static_cast<u64>(LoadU32At(ctx, 0x128U)) |
                      (static_cast<u64>(LoadU32At(ctx, 0x12cU)) << 32);
    const u64 next_delta = delta + 1U;
    StoreU32At(ctx, 0x128U, static_cast<u32>(next_delta & 0xffffffffULL));
    StoreU32At(ctx, 0x12cU,
               static_cast<u32>((next_delta >> 32) & 0xffffffffULL));

    if (any_active == 0) {
        EndingMidiPlaybackFinishedEdiAbi(ctx);
    }
}

} // namespace th10
