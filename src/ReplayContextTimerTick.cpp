// TH10 0x004297d0 - the shared replay-context timer tick, plus its frame
// writer helper 0x0042a8a0.
//
// The tick services the 0x2d4-byte replay game-context record (see
// StagePracticeReplayContext.cpp). Depending on the keep-alive flag at
// record+0x10 it either records the live HUD state into the per-stage
// replay writer segments (flag clear) or replays a stage's word/byte
// streams back into the HUD globals (flag set). The native body receives
// the record in EDI (0x42a3d0 moves the scheduler's ECX argument there
// before calling); the reconstruction keeps the plain pointer ABI.
#include <string.h>

#include "StagePracticeReplayContext.hpp"

#include "Th10Types.hpp"

namespace th10 {

namespace {

// TH10 DAT_00477810 - the game-manager object; the tick is inert without it.
extern u8 *g_ManagerObject810; // TH10 DAT_00477810

// Input state bank (current key words at +0/+4, saved word at +0x2c) and
// the HUD word family around it:
//   0x474e5c current state word          0x474e5e previous state word
//   0x474e5a hold-repeat counter         0x474e62/0x474e64 extra HUD words
const u32 k_input_bank = 0x474e30U;
const u32 k_state_word = 0x474e5cU;
const u32 k_previous_state_word = 0x474e5eU;
const u32 k_state_hold_counter = 0x474e5aU;
const u32 k_extra_word_a = 0x474e62U;
const u32 k_extra_word_b = 0x474e64U;
// TH10 0x491d78 flag word; bit 0x200 enables the hold-repeat latch below.
const u32 k_bgm_flag_word = 0x491d78U;
// TH10 DAT_00477708 pointer to the object whose +0x34 float feeds the
// per-second snapshot byte.
const u32 k_slow_rate_stats = 0x477708U;

// TH10 DAT_00470b0c / DAT_00470c34: the 0.5 rounding bias and the 256
// clamp limit used for the snapshot byte.
const float k_snapshot_bias = 0.5f;    // TH10 0x470b0c
const float k_snapshot_limit = 256.0f; // TH10 0x470c34

// TH10 0x0040ac20 (implemented in EclSelectMenu.cpp): thiscall input bank
// trigger/repeat update.
u16 AdvanceInputBankTriggersEcxAbi(void *bank);

// TH10 0x0042aa50 (implemented in TitleCalcCluster.cpp): allocates a fresh
// 0x6284 writer segment node for stage `index`, links it into the owner's
// per-stage bucket, and returns the owner (the native stores that return
// into record+0x9c, which is a self-pointer back to the record).
void *AllocateGameModeChainEntryEsiStackAbi(i32 index, void *owner);

// TH10 __ftol2 (0x463b2c): x87 truncating float->int conversion.
i32 FloatToI32Truncate(float value)
{
    return static_cast<i32>(value);
}

u16 LoadU16At(u32 address)
{
    const u8 *const source = reinterpret_cast<const u8 *>(address);
    return static_cast<u16>(static_cast<u16>(source[0])
                            | (static_cast<u16>(source[1]) << 8));
}

void StoreU16At(u32 address, u16 value)
{
    u8 *const target = reinterpret_cast<u8 *>(address);
    target[0] = static_cast<u8>(value);
    target[1] = static_cast<u8>(value >> 8);
}

u32 LoadPointerAt(u32 address)
{
    return *reinterpret_cast<const u32 *>(address);
}

} // namespace

// TH10 0x0042a8a0. Native usercall: EAX = the 0x6284 writer segment node,
// EDX = first word, stack = the other two words (ret 8). Appends the
// three-word HUD snapshot to the node's word stream at +0x5460 and reports
// whether the segment is full (3600 entries -> a new segment must be
// allocated for the next minute of the stage).
i32 WriteReplayFrameWordTripleEaxEdxStackAbi(void *writer, u16 first,
                                             u16 second, u16 third)
{
    u8 *const node = static_cast<u8 *>(writer);
    u32 cursor = *reinterpret_cast<const u32 *>(node + 0x5460U);

    *reinterpret_cast<u16 *>(cursor) = first;
    *reinterpret_cast<u16 *>(cursor + 2U) = second;
    *reinterpret_cast<u16 *>(cursor + 4U) = third;
    cursor += 6U;
    *reinterpret_cast<u32 *>(node + 0x5460U) = cursor;

    // Signed (cursor - node) / 6 >= 3600, via the native 0x2aaaaaab magic.
    const i32 entries
        = static_cast<i32>(cursor - reinterpret_cast<u32>(node)) / 6;
    return entries >= 0xe10 ? 1 : 0;
}

// TH10 0x004297d0. Native EDI = the replay context record; always returns 1.
//
// Record layout used here:
//   +0x00  pointer to the current 0x6284 writer segment (record+0x9c is a
//          self-pointer back to the record, so the tick reaches the segment
//          through *( *(record+0x9c) ))
//   +0x10  keep-alive flag (0 = record mode, non-zero = playback mode)
//   +0x9c  self-pointer (re-published after each segment roll)
//   +0xa4 + stage*0x24 .. per-stage playback slots (stride 0x24):
//            +0x00 word-triple stream cursor (6 bytes per entry)
//            +0x08 byte-stream cursor (1 byte per second)
//            +0x0c pointer to the stage limit block (+4 = frame limit)
//            +0x10 elapsed frame counter
//   +0x1c4 current per-second stream byte
//   +0x1c8 shared frame counter (the 30-frame cadence)
//   +0x1d0 current stage index (negative = nothing to play back)
i32 TickReplayContextTimers4297d0(void *record_raw)
{
    u8 *const record = static_cast<u8 *>(record_raw);

    if (g_ManagerObject810 == 0)
        return 1;

    if (*reinterpret_cast<const u32 *>(record + 0x10U) != 0U) {
        // ---------------------------------------------------------- playback
        const i32 stage = *reinterpret_cast<const i32 *>(record + 0x1d0U);
        if (stage < 0) {
            StoreU16At(k_state_word, 0);
            StoreU16At(k_extra_word_a, 0);
            StoreU16At(k_extra_word_b, 0);
            ++*reinterpret_cast<u32 *>(record + 0x1c8U);
            return 1;
        }

        StoreU16At(k_previous_state_word, LoadU16At(k_state_word));

        u8 *const slot = record + 0xa4U + static_cast<u32>(stage) * 0x24U;
        const u32 limit_block = *reinterpret_cast<const u32 *>(slot + 0x0cU);
        const i32 limit = *reinterpret_cast<const i32 *>(limit_block + 4U);
        if (*reinterpret_cast<const i32 *>(slot + 0x10U) < limit) {
            const u32 entry = *reinterpret_cast<const u32 *>(slot);
            StoreU16At(k_state_word, *reinterpret_cast<const u16 *>(entry));
            StoreU16At(k_extra_word_a,
                       *reinterpret_cast<const u16 *>(entry + 2U));
            StoreU16At(k_extra_word_b,
                       *reinterpret_cast<const u16 *>(entry + 4U));

            // One stream byte per second feeds record+0x1c4.
            const u32 byte_cursor = *reinterpret_cast<const u32 *>(slot + 8U);
            *reinterpret_cast<u8 *>(record + 0x1c4U)
                = *reinterpret_cast<const u8 *>(byte_cursor);
            *reinterpret_cast<u32 *>(slot) = entry + 6U;
            if (*reinterpret_cast<const u32 *>(record + 0x1c8U) % 30U == 0U)
                *reinterpret_cast<u32 *>(slot + 8U) = byte_cursor + 1U;
        } else {
            // Stage stream exhausted: silence the HUD words. The elapsed
            // counter keeps advancing on this path as well.
            StoreU16At(k_state_word, 0);
            StoreU16At(k_extra_word_a, 0);
            StoreU16At(k_extra_word_b, 0);
        }

        ++*reinterpret_cast<u32 *>(slot + 0x10U);
        ++*reinterpret_cast<u32 *>(record + 0x1c8U);
        return 1;
    }

    // ----------------------------------------------------------- recording
    StoreU16At(k_previous_state_word, LoadU16At(k_state_word));

    u32 state = *reinterpret_cast<const u32 *>(k_input_bank) & 0x1f7U;
    if ((LoadU16At(k_bgm_flag_word) & 0x200U) != 0U) {
        if ((state & 1U) != 0U) {
            // Hold-repeat latch: count consecutive held frames and raise
            // state bit 0x4 once the counter saturates at 8. The masked
            // state word is always published first; the saturating store
            // below happens on top of it.
            u16 hold = static_cast<u16>(LoadU16At(k_state_hold_counter) + 1U);
            StoreU16At(k_state_hold_counter, hold);
            if (hold >= 8U) {
                state |= 4U;
                StoreU16At(k_state_hold_counter, 8U);
                StoreU16At(k_state_word, static_cast<u16>(state));
            }
        } else {
            StoreU16At(k_state_hold_counter, 0U);
        }
    }
    StoreU16At(k_state_word, static_cast<u16>(state));

    // thiscall trigger/repeat update over the fixed input bank.
    AdvanceInputBankTriggersEcxAbi(reinterpret_cast<void *>(k_input_bank));

    // Every 30 frames: snapshot one byte plus the current word triple.
    if (*reinterpret_cast<const u32 *>(record + 0x1c8U) % 30U == 0U) {
        // value = *(0x477708)->f34 + 0.5, clamped: below 256 -> truncating
        // conversion, otherwise 255. The native fcom branches NaN into the
        // conversion path (_ftol2 of NaN -> 0), so mirror that explicitly.
        const float value
            = *reinterpret_cast<const float *>(LoadPointerAt(k_slow_rate_stats)
                                               + 0x34U)
              + k_snapshot_bias;
        u8 snapshot;
        if (value != value)
            snapshot = 0U; // NaN: native _ftol2 produces 0x80000000
        else if (value < k_snapshot_limit)
            snapshot = static_cast<u8>(FloatToI32Truncate(value));
        else
            snapshot = 0xffU;

        // writer = *(record+0x9c)->+0 (record+0x9c is a self-pointer).
        const u32 holder = LoadPointerAt(
            reinterpret_cast<u32>(record) + 0x9cU);
        u8 *const writer = reinterpret_cast<u8 *>(LoadPointerAt(holder));
        const u32 cursor = *reinterpret_cast<const u32 *>(writer + 0x6274U);
        *reinterpret_cast<u8 *>(cursor) = snapshot;
        *reinterpret_cast<u32 *>(writer + 0x6274U) = cursor + 1U;
    }

    const i32 full = WriteReplayFrameWordTripleEaxEdxStackAbi(
        reinterpret_cast<void *>(LoadPointerAt(LoadPointerAt(
            reinterpret_cast<u32>(record) + 0x9cU))),
        LoadU16At(k_state_word), LoadU16At(k_extra_word_a),
        LoadU16At(k_extra_word_b));
    if (full == 0) {
        ++*reinterpret_cast<u32 *>(record + 0x1c8U);
        return 1;
    }

    // Segment rolled over (3600 entries = one minute): allocate the next
    // 0x6284 writer node for the current stage; the helper returns the
    // record, which the native re-publishes into the +0x9c self-pointer.
    const i32 stage = *reinterpret_cast<const i32 *>(record + 0x1d0U);
    *reinterpret_cast<u32 *>(record + 0x9cU) = reinterpret_cast<u32>(
        AllocateGameModeChainEntryEsiStackAbi(stage, record));
    ++*reinterpret_cast<u32 *>(record + 0x1c8U);
    return 1;
}

} // namespace th10
