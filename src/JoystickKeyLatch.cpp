// TH10 0x0044a950. Joystick key latch update: for the key record selected
// by ECX (records live at DAT_00474e30, stride 0x6a bytes = 106 u16s):
//   +0x00 current 16-bit key mask, +0x02 previous mask,
//   +0x04 pressed mask, +0x06 released mask,
//   +0x0a..+0x29 sixteen per-bit hold counters (u16, +26 threshold).
// For every input bit: held bits age their counter (wrapping into the
// pressed mask at 26 frames), released bits clear the counter. The pressed
// and released masks are recomputed from current ^ previous.
#include "Th10Types.hpp"
#include "Th10Platform.hpp"

namespace th10 {

namespace {

extern u16 g_JoystickKeyRecords; // TH10 dword_474e30 (array base)

const u32 k_record_stride_u16 = 53U;   // 0x6a bytes per record
const u32 k_bit_count = 16U;
const u16 k_hold_threshold = 0x1aU;    // 26 frames

u16 *RecordBase(u32 record_index)
{
    return &g_JoystickKeyRecords + k_record_stride_u16 * record_index;
}

} // namespace

// Native AX = new key mask, ECX = record index; returns the mask.
u16 LatchJoystickKeyStatesEcxAbi(u16 key_mask, u32 record_index)
{
    u16 *record = RecordBase(record_index);
    record[1] = record[0];               // previous <- current
    record[0] = key_mask;
    u16 bit = 1;
    record[2] = 0;                       // auto-repeat mask
    u16 scan = key_mask;
    u16 *counter = record + 5;
    for (u32 index = k_bit_count; index != 0U; --index) {
        if ((scan & 1U) != 0U) {
            ++*counter;
            if (*counter >= k_hold_threshold) {
                record[2] |= bit;
                *counter -= 8;
            }
        } else {
            *counter = 0;
        }
        scan >>= 1;
        ++counter;
        bit = static_cast<u16>(bit << 1);
    }

    const u16 current = record[0];
    const u16 changed = current ^ record[1];
    record[3] = current & changed;       // pressed-now mask
    record[4] = changed & ~current;      // released-now mask
    return key_mask;
}

} // namespace th10
