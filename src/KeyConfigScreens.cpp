#include "KeyConfigScreens.hpp"

#include "Th10Platform.hpp"

#include "JoystickConfigPoll.hpp"

namespace th10 {

namespace {

// TH10 0x474e30. Per-slot input record bank, stride 0x6a (shared with the
// joystick config words consumed by PollJoystickConfigBitmaskEcxStackAbi:
// +0x58 shot, +0x5a bomb, +0x5c bit2, +0x5e bit3, +0x68 bit8).
const u32 k_config_bank = 0x474e30U;
const u32 k_config_stride = 0x6aU;

// TH10 0x4924fc. Keyboard sampling gate; when zero the keyboard mask is 0.
const u32 k_keyboard_enabled = 0x4924fcU;

// TH10 0x491ff4. Input mode flags; bit 0x200 selects the DirectInput
// keyboard device over GetKeyboardState.
const u32 k_input_mode_flags = 0x491ff4U;
const u32 k_input_mode_dinput_keyboard = 0x200U;

// TH10 0x491c38. IDirectInputDevice8A for the keyboard. GetDeviceState is
// vtable +0x24, Acquire is vtable +0x1c.
const u32 k_keyboard_device = 0x491c38U;
const u32 k_keyboard_state_size = 0x100U;
const u32 k_di_get_device_state_offset = 0x24U;
const u32 k_di_acquire_offset = 0x1cU;
const i32 k_di_err_notacquired = static_cast<i32>(0x8007001eU);

extern i32 TH10_STDCALL GetKeyboardState(u8 *state);

// Repeat counter: a held key enters the +0x04 repeat mask once its counter
// reaches 0x1a, and the counter is then reloaded by subtracting 8.
const u16 k_repeat_threshold = 0x1aU;
const u16 k_repeat_reload_subtrahend = 0x08U;

// Record word offsets from the slot base.
const u32 k_rec_current = 0x00U;
const u32 k_rec_previous = 0x02U;
const u32 k_rec_repeat_mask = 0x04U;
const u32 k_rec_pressed = 0x06U;
const u32 k_rec_released = 0x08U;
const u32 k_rec_counters = 0x0aU;

// Key tables extracted from the native shift-chain bitmask builders. Each
// entry is one result bit (bit N of the mask = entry N); key-state bytes are
// pressed = 0x80, so a bit is set when the high bit of any of the entry's
// key bytes is set (the native ORs the raw bytes and keeps bit 7).
// Index base: GetKeyboardState fills the buffer by virtual-key code, the
// DirectInput GetDeviceState buffer is DIK-indexed.
const u32 k_mask_bit_count = 15U;

const u8 k_virtual_key_table[k_mask_bit_count][2] = {
    {0x5aU, 0x5aU}, // bit 0  (0x0001) Z
    {0x58U, 0x58U}, // bit 1  (0x0002) X
    {0x10U, 0x10U}, // bit 2  (0x0004) Shift
    {0x1bU, 0x1bU}, // bit 3  (0x0008) Escape
    {0x26U, 0x68U}, // bit 4  (0x0010) Up / Num8
    {0x28U, 0x62U}, // bit 5  (0x0020) Down / Num2
    {0x25U, 0x64U}, // bit 6  (0x0040) Left / Num4
    {0x27U, 0x66U}, // bit 7  (0x0080) Right / Num6
    {0x11U, 0x11U}, // bit 8  (0x0100) Left Ctrl
    {0x51U, 0x51U}, // bit 9  (0x0200) Q
    {0x53U, 0x53U}, // bit 10 (0x0400) S
    {0x50U, 0x24U}, // bit 11 (0x0800) P / Home
    {0x0dU, 0x0dU}, // bit 12 (0x1000) Return
    {0x44U, 0x44U}, // bit 13 (0x2000) D
    {0x52U, 0x52U}, // bit 14 (0x4000) R
};

const u8 k_dik_key_table[k_mask_bit_count][2] = {
    {0x2cU, 0x2cU}, // bit 0  (0x0001) DIK_Z
    {0x2dU, 0x2dU}, // bit 1  (0x0002) DIK_X
    {0x2aU, 0x2aU}, // bit 2  (0x0004) DIK_LSHIFT
    {0x01U, 0x01U}, // bit 3  (0x0008) DIK_ESCAPE
    {0xc8U, 0x48U}, // bit 4  (0x0010) DIK_UP / DIK_NUMPAD8
    {0xd0U, 0x50U}, // bit 5  (0x0020) DIK_DOWN / DIK_NUMPAD2
    {0xcbU, 0x4bU}, // bit 6  (0x0040) DIK_LEFT / DIK_NUMPAD4
    {0xcdU, 0x4dU}, // bit 7  (0x0080) DIK_RIGHT / DIK_NUMPAD6
    {0x1dU, 0x1dU}, // bit 8  (0x0100) DIK_LCONTROL
    {0x10U, 0x10U}, // bit 9  (0x0200) DIK_Q
    {0x1fU, 0x1fU}, // bit 10 (0x0400) DIK_S
    {0x19U, 0xc7U}, // bit 11 (0x0800) DIK_P / DIK_HOME
    {0x1cU, 0x1cU}, // bit 12 (0x1000) DIK_RETURN
    {0x20U, 0x20U}, // bit 13 (0x2000) DIK_D
    {0x13U, 0x13U}, // bit 14 (0x4000) DIK_R
};

// Numpad diagonals write fixed multi-bit combinations of the axis bits
// (native and/neg/sbb + immediate OR): Num7 = up|left, Num1 = down|left,
// Num9 = up|right, Num3 = down|right.
const u32 k_diagonal_count = 4U;

const u8 k_virtual_key_diagonals[k_diagonal_count] = {
    0x67U, 0x61U, 0x69U, 0x63U}; // Num7, Num1, Num9, Num3
const u16 k_virtual_key_diagonal_masks[k_diagonal_count] = {
    0x0050U, 0x0060U, 0x0090U, 0x00a0U};

const u8 k_dik_diagonals[k_diagonal_count] = {
    0x47U, 0x4fU, 0x49U, 0x51U}; // DIK_NUMPAD7/1/9/3
const u16 k_dik_diagonal_masks[k_diagonal_count] = {
    0x0050U, 0x0060U, 0x0090U, 0x00a0U};

u32 ReadGlobalU32(u32 address)
{
    const u8 *const source = reinterpret_cast<const u8 *>(address);
    return static_cast<u32>(source[0]) | (static_cast<u32>(source[1]) << 8)
         | (static_cast<u32>(source[2]) << 16)
         | (static_cast<u32>(source[3]) << 24);
}

u16 ReadRecordWord(const u8 *record, u32 offset)
{
    return static_cast<u16>(static_cast<u16>(record[offset])
                            | static_cast<u16>(record[offset + 1U] << 8U));
}

void WriteRecordWord(u8 *record, u32 offset, u16 value)
{
    record[offset] = static_cast<u8>(value & 0xffU);
    record[offset + 1U] = static_cast<u8>((value >> 8U) & 0xffU);
}

u16 KeyByte(const u8 *state, u32 key_index)
{
    return static_cast<u16>(state[key_index]);
}

// Native mask builder: fifteen hardcoded key tests over the sampled 256-byte
// key state plus the four numpad diagonal immediates.
u16 BuildKeyboardMask(const u8 *state, bool direct_input)
{
    const u8(*const key_table)[2] =
        direct_input ? k_dik_key_table : k_virtual_key_table;
    const u8 *const diagonals =
        direct_input ? k_dik_diagonals : k_virtual_key_diagonals;
    const u16 *const diagonal_masks =
        direct_input ? k_dik_diagonal_masks : k_virtual_key_diagonal_masks;

    u16 mask = 0U;
    for (u32 bit = 0U; bit < k_mask_bit_count; ++bit) {
        const u16 pressed =
            static_cast<u16>(KeyByte(state, key_table[bit][0])
                             | KeyByte(state, key_table[bit][1]));
        if ((pressed & 0x80U) != 0U) {
            mask = static_cast<u16>(mask | (1U << bit));
        }
    }
    for (u32 entry = 0U; entry < k_diagonal_count; ++entry) {
        if ((KeyByte(state, diagonals[entry]) & 0x80U) != 0U) {
            mask = static_cast<u16>(mask | diagonal_masks[entry]);
        }
    }
    return mask;
}

// Keyboard sampling: 0x4924fc == 0 or a failed DirectInput GetDeviceState
// yields mask 0 (the native Acquire-on-failure quirk is preserved; the state
// is never re-read after acquiring).
u16 SampleKeyboardMask()
{
    u8 state[k_keyboard_state_size];
    for (u32 i = 0; i < k_keyboard_state_size; ++i) {
        state[i] = 0U;
    }

    if (ReadGlobalU32(k_keyboard_enabled) == 0U) {
        return 0U;
    }

    if ((ReadGlobalU32(k_input_mode_flags) & k_input_mode_dinput_keyboard)
        != 0U) {
        void *device = reinterpret_cast<void *>(
            ReadGlobalU32(k_keyboard_device));
        void **const vtbl = *static_cast<void ***>(device);

        typedef i32 (TH10_STDCALL *GetDeviceStateFn)(void *device, u32 size,
                                                     void *buffer);
        const i32 got_state = reinterpret_cast<GetDeviceStateFn>(
            vtbl[k_di_get_device_state_offset / 4U])(device,
                                                     k_keyboard_state_size,
                                                     state);
        if (got_state != 0) {
            typedef i32 (TH10_STDCALL *AcquireFn)(void *device);
            (void)reinterpret_cast<AcquireFn>(
                vtbl[k_di_acquire_offset / 4U])(device);
            return 0U;
        }
        return BuildKeyboardMask(state, true);
    }

    (void)GetKeyboardState(state);
    return BuildKeyboardMask(state, false);
}

// Shared record epilogue: previous/current swap, per-bit repeat counters,
// and pressed/released edges computed from the final words.
void UpdateKeyConfigRecord(u8 *record, u32 poll_result)
{
    const u16 previous = ReadRecordWord(record, k_rec_current);
    const u16 current = static_cast<u16>(poll_result & 0xffffU);
    WriteRecordWord(record, k_rec_previous, previous);
    WriteRecordWord(record, k_rec_current, current);
    WriteRecordWord(record, k_rec_repeat_mask, 0U);

    u16 repeat_mask = 0U;
    u16 bit = 1U;
    for (u32 i = 0; i < 16U; ++i) {
        const u32 counter_offset = k_rec_counters + i * 2U;
        if ((current & bit) != 0U) {
            u16 counter = static_cast<u16>(
                ReadRecordWord(record, counter_offset) + 1U);
            if (counter >= k_repeat_threshold) {
                repeat_mask = static_cast<u16>(repeat_mask | bit);
                counter = static_cast<u16>(
                    counter - k_repeat_reload_subtrahend);
            }
            WriteRecordWord(record, counter_offset, counter);
        } else {
            WriteRecordWord(record, counter_offset, 0U);
        }
        bit = static_cast<u16>(bit << 1U);
    }
    WriteRecordWord(record, k_rec_repeat_mask, repeat_mask);

    const u16 changed = static_cast<u16>(current ^ previous);
    WriteRecordWord(record, k_rec_pressed,
                    static_cast<u16>(current & changed));
    WriteRecordWord(record, k_rec_released,
                    static_cast<u16>(changed & static_cast<u16>(~current)));
}

u8 *ConfigRecord(u32 config_slot)
{
    return reinterpret_cast<u8 *>(k_config_bank)
         + config_slot * k_config_stride;
}

} // namespace

u32 UpdateKeyConfigInputRecordEcxAbi(u32 config_slot)
{
    const u16 keyboard_mask = SampleKeyboardMask();
    const u32 result = PollJoystickConfigBitmaskEcxStackAbi(
        keyboard_mask, config_slot, 0U);
    UpdateKeyConfigRecord(ConfigRecord(config_slot), result);
    return result;
}

u32 UpdateKeyConfigInputRecordSlotZeroPollEcxAbi(u32 config_slot)
{
    const u16 keyboard_mask = SampleKeyboardMask();
    const u32 result =
        PollJoystickConfigBitmaskEcxStackAbi(keyboard_mask, 0U, 0U);
    UpdateKeyConfigRecord(ConfigRecord(config_slot), result);
    return result;
}

u32 PollKeyConfigMenuInputMask()
{
    const u16 keyboard_mask = SampleKeyboardMask();
    return PollJoystickConfigBitmaskEcxStackAbi(keyboard_mask, 0U, 0U);
}

} // namespace th10
