#include "JoystickConfigPoll.hpp"

#include "Th10Platform.hpp"

namespace th10 {

namespace {

// TH10 0x474e30. Per-slot input config bank, stride 0x6a (53 u16 words).
// The five mapped words hold signed button indices; a negative word means
// "unassigned". Word offsets from the slot base:
//   +0x58 shot (result bit 0),  +0x5a bomb (bit 1),
//   +0x5c (bit 2),              +0x5e (bit 3),
//   +0x68 (bit 8).
const u32 k_config_bank = 0x474e30U;

// TH10 0x4918b8. winmm JOYCAPS array, 0x194-byte entries:
// wXmin +0x20, wXmax +0x22, wYmin +0x24, wYmax +0x26.
const u32 k_joy_caps = 0x4918b8U;
const u32 k_joy_caps_stride = 0x194U;

// TH10 0x491ff4. Input mode flags; bit 0x400 selects the DirectInput
// controller path over the winmm joyGetPosEx path.
const u32 k_input_mode_flags = 0x491ff4U;

// TH10 0x491c3c. LPDIRECTINPUTDEVICE8 per joystick index.
const u32 k_device_table = 0x491c3cU;

// TH10 0x491d5e / 0x491d60. Signed DirectInput axis deadzone thresholds
// (X and Y respectively).
const u32 k_deadzone_x = 0x491d5eU;
const u32 k_deadzone_y = 0x491d60U;

const u32 k_di_device_state_size = 0x110U;
const u32 k_di_buttons_offset = 0x30U;
const u32 k_di_acquire_retry_limit = 0x190U;
const i32 k_di_err_notacquired = static_cast<i32>(0x8007001eU);

struct JoyInfoEx {
    u32 size;
    u32 flags;
    u32 axes[4];
    u32 buttons;
};

extern "C" u32 TH10_STDCALL joyGetPosEx(u32 id, JoyInfoEx *info);

typedef i32 (TH10_STDCALL *DeviceMethod0)(void *device);
typedef i32 (TH10_STDCALL *DeviceMethodGetState)(void *device, u32 size,
                                              void *buffer);

u16 ConfigWord(u32 config_slot, u32 word_offset)
{
    const u8 *const slot = reinterpret_cast<const u8 *>(k_config_bank)
                         + config_slot * 0x6aU;
    u16 word;
    for (u32 i = 0; i < sizeof(word); ++i) {
        reinterpret_cast<u8 *>(&word)[i] = slot[word_offset + i];
    }
    return word;
}

u16 CapsWord(u32 joystick_index, u32 word_offset)
{
    const u8 *const caps = reinterpret_cast<const u8 *>(k_joy_caps)
                         + joystick_index * k_joy_caps_stride;
    u16 word;
    for (u32 i = 0; i < sizeof(word); ++i) {
        reinterpret_cast<u8 *>(&word)[i] = caps[word_offset + i];
    }
    return word;
}

void *DevicePointer(u32 joystick_index)
{
    void *device;
    const u8 *const entry = reinterpret_cast<const u8 *>(k_device_table)
                          + joystick_index * 4U;
    for (u32 i = 0; i < sizeof(device); ++i) {
        reinterpret_cast<u8 *>(&device)[i] = entry[i];
    }
    return device;
}

u32 ReadGlobalU32(u32 address)
{
    const u8 *const source = reinterpret_cast<const u8 *>(address);
    return static_cast<u32>(source[0]) | (static_cast<u32>(source[1]) << 8)
         | (static_cast<u32>(source[2]) << 16)
         | (static_cast<u32>(source[3]) << 24);
}

short ReadGlobalI16(u32 address)
{
    return static_cast<short>(ReadGlobalU32(address) & 0xffffU);
}

// Native failure exits do `mov ax, si` on top of the failing API result:
// the API's high word survives and only the low word carries the
// accumulated bitmask.
u32 FailureResult(u32 api_result, u32 accumulated)
{
    return (api_result & 0xffff0000U) | (accumulated & 0xffffU);
}

// Maps one config word onto one result bit: the word is a signed button
// index; negative disables. The native comparison idiom (and/neg/sbb)
// yields the mapped bit when (buttons & (1 << index)) != 0.
u32 ButtonBit(const u32 buttons, const i32 index, const u32 result_bit)
{
    if (index < 0) {
        return 0U;
    }
    return ((buttons & (1U << index)) != 0U) ? result_bit : 0U;
}

u32 PollWinmmJoystick(u32 accumulated, u32 config_slot, u32 joystick_index)
{
    JoyInfoEx info;
    for (u32 i = 0; i < sizeof(info); ++i) {
        reinterpret_cast<u8 *>(&info)[i] = 0U;
    }
    info.size = 0x34U;
    info.flags = 0xffU;
    const u32 status = joyGetPosEx(joystick_index != 0U ? 1U : 0U, &info);
    if (status != 0U) {
        return FailureResult(status, accumulated);
    }

    u32 result = accumulated;
    result |= ButtonBit(info.buttons, static_cast<i32>(ConfigWord(config_slot, 0x58)), 0x1U);
    result |= ButtonBit(info.buttons, static_cast<i32>(ConfigWord(config_slot, 0x5a)), 0x2U);
    result |= ButtonBit(info.buttons, static_cast<i32>(ConfigWord(config_slot, 0x5e)), 0x8U);
    result |= ButtonBit(info.buttons, static_cast<i32>(ConfigWord(config_slot, 0x5c)), 0x4U);
    result |= ButtonBit(info.buttons, static_cast<i32>(ConfigWord(config_slot, 0x68)), 0x100U);

    // Axis thresholds from the winmm caps: middle at (min+max)/2 with a
    // quarter-range dead zone; all arithmetic is 32-bit unsigned.
    const u32 x_min = CapsWord(joystick_index, 0x20U);
    const u32 x_max = CapsWord(joystick_index, 0x22U);
    const u32 y_min = CapsWord(joystick_index, 0x24U);
    const u32 y_max = CapsWord(joystick_index, 0x26U);

    const u32 x_quarter = (x_max - x_min) >> 2;
    const u32 x_right = ((x_min + x_max) >> 1) + x_quarter;
    const u32 x_left = ((x_min + x_max) >> 1) - x_quarter;
    if (x_right < info.axes[0]) {
        result |= 0x80U;
    }
    if (info.axes[0] < x_left) {
        result |= 0x40U;
    }

    const u32 y_quarter = (y_max - y_min) >> 2;
    const u32 y_down = ((y_min + y_max) >> 1) + y_quarter;
    const u32 y_up = ((y_min + y_max) >> 1) - y_quarter;
    if (y_down < info.axes[1]) {
        result |= 0x20U;
    }
    if (info.axes[1] < y_up) {
        result |= 0x10U;
    }
    return result;
}

// The DirectInput path keeps polling and re-acquiring, but every exit from
// the failed-Poll branch returns the accumulated low word unchanged; the
// device state is only read when Poll already succeeded.
u32 PollDirectInputJoystick(u32 accumulated, u32 config_slot,
                            u32 joystick_index)
{
    void *device = DevicePointer(joystick_index);
    void **const vtbl = *static_cast<void ***>(device);

    if (reinterpret_cast<DeviceMethod0>(vtbl[0x64 / 4])(device) >= 0) {
        u8 state[k_di_device_state_size];
        for (u32 i = 0; i < k_di_device_state_size; ++i) {
            state[i] = 0U;
        }
        const i32 got_state = reinterpret_cast<DeviceMethodGetState>(
            vtbl[0x24 / 4])(device, k_di_device_state_size, state);
        if (got_state < 0) {
            return FailureResult(static_cast<u32>(got_state), accumulated);
        }

        const u8 *const buttons = state + k_di_buttons_offset;
        u32 result = accumulated;
        const i32 shot = static_cast<short>(ConfigWord(config_slot, 0x58));
        const i32 bomb = static_cast<short>(ConfigWord(config_slot, 0x5a));
        const i32 bit2 = static_cast<short>(ConfigWord(config_slot, 0x5c));
        const i32 bit3 = static_cast<short>(ConfigWord(config_slot, 0x5e));
        const i32 bit8 = static_cast<short>(ConfigWord(config_slot, 0x68));

        if (shot >= 0) {
            result |= static_cast<u32>(buttons[shot]) >> 7;
        }
        if (bomb >= 0) {
            result |= (static_cast<u32>(buttons[bomb]) >> 6) & 0x2U;
        }
        if (bit3 >= 0) {
            result |= (static_cast<u32>(buttons[bit3]) >> 4) & 0x8U;
        }
        if (bit2 >= 0) {
            result |= (static_cast<u32>(buttons[bit2]) >> 5) & 0x4U;
        }
        if (bit8 >= 0) {
            result |= (static_cast<u32>(buttons[bit8] & 0x80U)) << 1;
        }

        // Signed long axis values compared against the signed deadzone
        // words; 0x10/0x20 are up/down, 0x40/0x80 are left/right.
        i32 axis_x;
        i32 axis_y;
        for (u32 i = 0; i < 4; ++i) {
            reinterpret_cast<u8 *>(&axis_x)[i] = state[i];
            reinterpret_cast<u8 *>(&axis_y)[i] = state[4 + i];
        }
        const i32 deadzone_x = ReadGlobalI16(k_deadzone_x);
        const i32 deadzone_y = ReadGlobalI16(k_deadzone_y);
        if (axis_x > deadzone_x) {
            result |= 0x80U;
        }
        if (axis_x < -deadzone_x) {
            result |= 0x40U;
        }
        if (axis_y > deadzone_y) {
            result |= 0x20U;
        }
        if (axis_y < -deadzone_y) {
            result |= 0x10U;
        }
        return result;
    }

    // Poll failed: retry Acquire up to 0x190 times while it reports
    // DIERR_NOTACQUIRED, then return the input bitmask unchanged either
    // way (native never reads the state after re-acquiring).
    const i32 first_acquire =
        reinterpret_cast<DeviceMethod0>(vtbl[0x1c / 4])(device);
    if (first_acquire != k_di_err_notacquired) {
        return FailureResult(static_cast<u32>(first_acquire), accumulated);
    }
    i32 acquire = first_acquire;
    for (u32 attempt = 0U; attempt < k_di_acquire_retry_limit; ++attempt) {
        acquire = reinterpret_cast<DeviceMethod0>(vtbl[0x1c / 4])(device);
        if (acquire != k_di_err_notacquired) {
            break;
        }
    }
    return FailureResult(static_cast<u32>(acquire), accumulated);
}

} // namespace

u32 PollJoystickConfigBitmaskEcxStackAbi(u32 accumulated,
                                         u32 config_slot,
                                         u32 joystick_index)
{
    if ((ReadGlobalU32(k_input_mode_flags) & 0x400U) == 0U) {
        return PollWinmmJoystick(accumulated, config_slot, joystick_index);
    }
    return PollDirectInputJoystick(accumulated, config_slot, joystick_index);
}

// TH10 0x497bb0. The 224-byte polled button/axis byte array published by
// 0x0044a4e0 for the key-config screens.
const u32 k_button_byte_array = 0x497bb0U;
const u32 k_button_byte_count = 0xe0U;

void StoreGlobalBytes(u32 address, const u8 *bytes, u32 count)
{
    u8 *const target = reinterpret_cast<u8 *>(address);
    for (u32 i = 0; i < count; ++i) {
        target[i] = bytes[i];
    }
}

void ClearGlobalBytes(u32 address, u32 count)
{
    u8 *const target = reinterpret_cast<u8 *>(address);
    for (u32 i = 0; i < count; ++i) {
        target[i] = 0U;
    }
}

u8 *PollJoystickButtonBytesEcxEsiAbi(u32 device_index, u32 poll_value)
{
    ClearGlobalBytes(k_button_byte_array, k_button_byte_count);
    if ((ReadGlobalU32(k_input_mode_flags) & 0x400U) != 0U) {
        void *device = DevicePointer(device_index);
        void **const vtbl = *static_cast<void ***>(device);
        const i32 polled = reinterpret_cast<DeviceMethod0>(vtbl[0x64 / 4])(
            device);
        if (polled >= 0) {
            u8 state[k_di_device_state_size];
            for (u32 i = 0; i < k_di_device_state_size; ++i) {
                state[i] = 0U;
            }
            (void)poll_value;
            reinterpret_cast<DeviceMethodGetState>(vtbl[0x24 / 4])(
                device, k_di_device_state_size, state);
            StoreGlobalBytes(k_button_byte_array, state, k_button_byte_count);
        } else {
            // Same re-acquire loop as the 0x44a190 path: while Acquire
            // reports DIERR_NOTACQUIRED it retries up to 0x190 times and
            // never reads state afterwards.
            if (reinterpret_cast<DeviceMethod0>(vtbl[0x1c / 4])(device)
                == k_di_err_notacquired) {
                for (u32 attempt = 0U; attempt < k_di_acquire_retry_limit;
                     ++attempt) {
                    const i32 acquire = reinterpret_cast<DeviceMethod0>(
                        vtbl[0x1c / 4])(device);
                    if (acquire != k_di_err_notacquired) {
                        break;
                    }
                }
            }
        }
    } else {
        JoyInfoEx info;
        for (u32 i = 0; i < sizeof(info); ++i) {
            reinterpret_cast<u8 *>(&info)[i] = 0U;
        }
        info.size = 0x34U;
        info.flags = 0xffU;
        u32 status = joyGetPosEx(0U, &info);
        if (status == 0U) {
            u32 index = status; // 0
            u32 buttons = info.buttons;
            do {
                if ((buttons & 1U) != 0U) {
                    u8 *const array = reinterpret_cast<u8 *>(k_button_byte_array);
                    array[index] = 0x80U;
                }
                ++index;
                buttons >>= 1;
            } while (index < 0x20U);
        }
    }
    return reinterpret_cast<u8 *>(k_button_byte_array);
}

} // namespace th10
