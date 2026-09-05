#include <string.h>

#include "Th10Platform.hpp"
#include "Th10Types.hpp"

namespace th10 {

namespace {

struct MainChainWindowClass {
    u32 style;
    void *window_procedure;
    i32 class_extra_bytes;
    i32 window_extra_bytes;
    void *instance;
    void *icon;
    void *cursor;
    void *background_brush;
    const char *menu_name;
    const char *class_name;
};

typedef char AssertMainChainWindowClassSize[
    sizeof(MainChainWindowClass) == 0x28 ? 1 : -1];

extern u8 g_MainChainFallbackWindowMode; // DAT_00491d65
extern void *g_MainChainApplicationInstance; // DAT_004924f8
extern void *g_MainChainWindow; // DAT_004924f0
extern void *g_MainChainD3DWindow; // DAT_00491c70
extern u32 g_MainChainWindowInputEnabled; // DAT_004924fc
extern u32 g_MainChainWindowMouseState; // DAT_00492500
extern u32 g_MainChainRuntimeFlags; // DAT_00491ff4
extern void *g_MainChainMidiOutput; // DAT_00491fe8
extern const char g_MainChainWindowTitle[];

extern void *TH10_STDCALL GetStockObject(i32 object);
extern void *TH10_STDCALL LoadCursorA(void *instance, const char *cursor_id);
extern u16 TH10_STDCALL RegisterClassA(const MainChainWindowClass *window_class);
extern void *TH10_STDCALL CreateWindowExA(u32 extended_style,
    const char *class_name, const char *window_name, u32 style, i32 x, i32 y,
    i32 width, i32 height, void *parent, void *menu, void *instance,
    void *parameter);
extern i32 TH10_STDCALL GetSystemMetrics(i32 index);
extern i32 TH10_STDCALL SendMessageA(void *window, u32 message,
                                     u32 wparam, i32 lparam);
extern void TH10_STDCALL Sleep(u32 milliseconds);
extern i32 TH10_STDCALL DefWindowProcA(void *window, u32 message,
                                       u32 wparam, i32 lparam);
extern i32 TH10_STDCALL SetForegroundWindow(void *window);
extern i32 TH10_STDCALL ShowCursor(i32 show);
extern void *TH10_STDCALL SetCursor(void *cursor);
extern void DeliverMainChainMidiWindowNotification(void *midi_output);

} // namespace

i32 TH10_STDCALL MainChainWindowProcedure(void *window, u32 message,
                                           u32 wparam, i32 lparam)
{
    if (message == 0x10) {
        g_MainChainRuntimeFlags |= 0x80U;
        return 1;
    }
    if (message == 0x14)
        return 1;
    if (message == 0x1c) {
        g_MainChainWindowMouseState = wparam == 0 ? 1 : 0;
        g_MainChainWindowInputEnabled = wparam;
        return DefWindowProcA(window, message, wparam, lparam);
    }
    if (message == 0x20) {
        if (g_MainChainFallbackWindowMode == 0 &&
            g_MainChainWindowMouseState == 0) {
            (void)ShowCursor(0);
            (void)SetCursor(0);
        } else {
            (void)SetCursor(LoadCursorA(0,
                reinterpret_cast<const char *>(0x7f00)));
            (void)ShowCursor(1);
        }
        return 1;
    }
    if (message == 0x201)
        (void)SetForegroundWindow(window);
    else if (message == 0x3c9 && g_MainChainMidiOutput != 0)
        DeliverMainChainMidiWindowNotification(g_MainChainMidiOutput);

    return DefWindowProcA(window, message, wparam, lparam);
}

i32 CreateMainChainWindow()
{
    MainChainWindowClass window_class;
    memset(&window_class, 0, sizeof(window_class));
    window_class.background_brush = GetStockObject(4);
    window_class.cursor = LoadCursorA(0, reinterpret_cast<const char *>(0x7f00));
    window_class.instance = g_MainChainApplicationInstance;
    window_class.window_procedure = reinterpret_cast<void *>(
        MainChainWindowProcedure);
    window_class.class_name = "BASE";
    g_MainChainWindowInputEnabled = 1;
    g_MainChainWindowMouseState = 0;
    (void)RegisterClassA(&window_class);

    if (g_MainChainFallbackWindowMode == 0) {
        g_MainChainD3DWindow = CreateWindowExA(0, window_class.class_name,
            g_MainChainWindowTitle, 0x00cf0000, 0, 0, 0x280, 0x1e0, 0, 0,
            g_MainChainApplicationInstance, 0);
    } else {
        const i32 frame_x = GetSystemMetrics(7);
        const i32 frame_y = GetSystemMetrics(8);
        const i32 caption = GetSystemMetrics(4);
        g_MainChainD3DWindow = CreateWindowExA(0, window_class.class_name,
            g_MainChainWindowTitle, 0x100a0000, static_cast<i32>(0x80000000U),
            static_cast<i32>(0x80000000U), frame_x * 2 + 0x280,
            caption + 0x1e0 + frame_y * 2, 0, 0,
            g_MainChainApplicationInstance, 0);
    }

    g_MainChainWindow = g_MainChainD3DWindow;
    if (g_MainChainD3DWindow == 0)
        return 1;

    (void)SendMessageA(g_MainChainD3DWindow, 0x112, 0xf020, 0);
    Sleep(0x10);
    (void)SendMessageA(g_MainChainWindow, 0x112, 0xf120, 0);
    return 0;
}

} // namespace th10
