// TH10 system/UI support functions (0x00439660-0x0043bc40 cluster):
//   0x00439660 lock-free twin of the frame clock (0x00439540).
//   0x0043a3a0 the replay-overwrite dialog procedure (writes the 0x100
//     "allow overwrite" gate flag into DAT_00491d78).
//   0x0043a5c0 MIDI-out device-name query, 0x0043a6f0/0x0043a700 byte
//     swaps used by the big-endian MIDI readers.
//   0x0043ba90 input re-acquisition with the pause-axis bits re-merged.
//   0x0043bc40 replay camera submission through the raw D3D9 vtable.
#include <string.h>

#include "Th10Types.hpp"
#include "Th10Platform.hpp"
#include "MainChainRender.hpp"
#include "MainChainInput.hpp"

namespace th10 {

namespace {

struct Win32LargeInteger {
    long long quad_part;
};

struct MidiOutCapsA {
    u16 manufacturer_id;
    u16 product_id;
    u32 driver_version;
    char product_name[32];
    u16 technology;
    u16 voices;
    u16 notes;
    u16 channel_mask;
    u32 support;
};

extern Win32LargeInteger g_MainChainPerformanceFrequency; // DAT_00492508
extern Win32LargeInteger g_MainChainPerformanceBase;      // DAT_00492510
extern double g_MainChainFrameClockEpoch;                 // DAT_00492540

extern "C" i32 TH10_STDCALL QueryPerformanceCounter(
    Win32LargeInteger *value);
extern "C" i32 TH10_STDCALL timeBeginPeriod(u32 milliseconds);
extern "C" u32 TH10_STDCALL timeGetTime();
extern "C" i32 TH10_STDCALL timeEndPeriod(u32 milliseconds);
extern "C" void *TH10_STDCALL GetDlgItem(void *dialog, i32 item_id);
extern "C" i32 TH10_STDCALL SendMessageA(void *window, u32 message,
                                         u32 w_param, u32 l_param);
extern "C" i32 TH10_STDCALL EndDialog(void *dialog, i32 result);
extern "C" u32 TH10_STDCALL IsDlgButtonChecked(void *dialog, i32 button_id);
extern "C" u32 TH10_STDCALL midiOutGetDevCapsA(u32 device_id,
                                               MidiOutCapsA *caps,
                                               u32 caps_size);

const u32 k_message_check_radio = 0x0f1U;
const u32 k_still_active = 259U;

} // namespace

// TH10 0x00439660. Native entry without arguments; double result in ST(0).
// Same clock as 0x00439540 but without the critical-section guard.
double GetMainChainFrameTimeUnlocked()
{
    double sample;
    if (g_MainChainPerformanceFrequency.quad_part != 0) {
        Win32LargeInteger now;
        (void)QueryPerformanceCounter(&now);
        sample = static_cast<double>(now.quad_part -
            g_MainChainPerformanceBase.quad_part) /
            static_cast<double>(g_MainChainPerformanceFrequency.quad_part);
    } else {
        (void)timeBeginPeriod(1);
        const double ticks = static_cast<double>(timeGetTime());
        (void)timeEndPeriod(1);
        sample = ticks * 0.001;
    }
    if (g_MainChainFrameClockEpoch > sample)
        g_MainChainFrameClockEpoch = sample;
    return sample - g_MainChainFrameClockEpoch;
}

// TH10 0x0043a3a0. Native stdcall dialog procedure.
//
// WM_INITDIALOG (272): check the 202 checkbox. WM_COMMAND (273): ID 201
// (OK) ends with 6, ID 203 (cancel) with 7; both fold the checkbox state
// into the 0x100 bit of the gate dword.
i32 ReplayOverwriteDialogProc(void *dialog, u32 message, u32 w_param,
                              u32 l_param)
{
    (void)l_param;
    extern u32 g_ReplayOverwriteGate; // TH10 DAT_00491d78

    if (message == 272U) {
        (void)SendMessageA(GetDlgItem(dialog, 202),
                           k_message_check_radio, 1U, 0U);
        return 0;
    }
    if (message != 273U)
        return 0;

    const u16 command = static_cast<u16>(w_param);
    if (command == 201U) {
        if (IsDlgButtonChecked(dialog, 202) == 1U)
            g_ReplayOverwriteGate |= 0x100U;
        else
            g_ReplayOverwriteGate &= ~0x100U;
        (void)EndDialog(reinterpret_cast<void *>(dialog), 6);
        return 0;
    }
    if (command != 203U)
        return 0;

    if (IsDlgButtonChecked(dialog, 202) == 1U)
        g_ReplayOverwriteGate |= 0x100U;
    else
        g_ReplayOverwriteGate &= ~0x100U;
    (void)EndDialog(reinterpret_cast<void *>(dialog), 7);
    return 0;
}

// TH10 0x0043a5c0. Native ECX = device id, stack = destination buffer.
void GetMidiOutDeviceNameEcxStackAbi(u32 device_id, char *destination)
{
    MidiOutCapsA caps;
    memset(&caps, 0, sizeof(caps));
    (void)midiOutGetDevCapsA(device_id, &caps, 0x34U);
    strcpy(destination, caps.product_name);
}

// TH10 0x0043a6f0. Native AX in / AX out: swaps the two bytes.
u16 SwapEndian16AxAbi(u16 value)
{
    return static_cast<u16>((value << 8) | (value >> 8));
}

// TH10 0x0043a700. Native stack argument: full byte reversal (b0 b1 b2 b3
// -> b3 b2 b1 b0).
u32 SwapEndian32StackAbi(u32 value)
{
    return (value >> 24) | ((value >> 8) & 0x0000ff00U) |
           ((value << 8) & 0x00ff0000U) | (value << 24);
}

// TH10 0x0043ba90. Re-initializes the input chain and re-merges the two
// key-state bytes (bits 0x200/0x400 of DAT_00491ff4 from DAT_00491c38/3c).
u32 RefreshMainChainInputAxes(void)
{
    extern u32 g_MainChainInputStateWord;  // TH10 DAT_00491ff4
    extern u8 g_InputAxisByteA;            // TH10 DAT_00491c38
    extern u8 g_InputAxisByteB;            // TH10 DAT_00491c3c
    extern MainChainContext *g_MainChainContextGlobal;

    g_MainChainInputStateWord &= 0xfffff9ffU;
    (void)InitializeMainChainInputSemantic(g_MainChainContextGlobal);

    const u32 state = g_MainChainInputStateWord;
    const u32 merged_a = (state ^ (static_cast<u32>(g_InputAxisByteA != 0)
                                       << 9U)) & 0x200U;
    const u32 merged = state ^ merged_a;
    const u32 merged_b = (merged ^ (static_cast<u32>(g_InputAxisByteB != 0)
                                        << 10U)) & 0x400U;
    const u32 result = merged ^ merged_b;
    g_MainChainInputStateWord = result;
    return result;
}

// TH10 0x0043bc40. Native ESI = clear color word. Clears the target
// (D3DCLEAR_TARGET|ZBUFFER with the ESI word as color) and presents; on
// device loss the presentation parameters are re-set first, twice.
void SubmitReplayCameraFrameEsiAbi(void *clear_color_word)
{
    extern D3D9Device *g_MainChainD3DDevice; // TH10 DAT_00491c30
    extern u8 g_D3D9PresentationParameters;  // TH10 DAT_00491d0c
    D3D9Device *device = g_MainChainD3DDevice;

    typedef i32 (TH10_STDCALL *D3DClearFn)(D3D9Device *, u32, const void *,
        u32, u32, float, u32);
    typedef i32 (TH10_STDCALL *D3DPresentFn)(D3D9Device *, const void *,
        const void *, void *, const void *);
    typedef i32 (TH10_STDCALL *D3DResetFn)(D3D9Device *, void *);

    void **const vtable = device->vtable;
    const D3DClearFn clear =
        reinterpret_cast<D3DClearFn>(vtable[0xac / 4]);
    const D3DPresentFn present =
        reinterpret_cast<D3DPresentFn>(vtable[0x44 / 4]);
    const D3DResetFn reset =
        reinterpret_cast<D3DResetFn>(vtable[0x40 / 4]);

    (void)clear(device, 0, 0, 3U,
                reinterpret_cast<u32>(clear_color_word), 1.0f, 0);
    if (present(device, 0, 0, 0, 0) < 0)
        (void)reset(device, &g_D3D9PresentationParameters);
    (void)clear(device, 0, 0, 3U,
                reinterpret_cast<u32>(clear_color_word), 1.0f, 0);
    if (present(device, 0, 0, 0, 0) < 0)
        (void)reset(device, &g_D3D9PresentationParameters);
}

} // namespace th10
