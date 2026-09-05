#include <string.h>

#include "Th10Platform.hpp"

namespace th10 {

namespace {

struct MainChainStartupInfo {
    u32 cb;
    char *reserved;
    char *desktop;
    char *title;
    u32 x;
    u32 y;
    u32 x_size;
    u32 y_size;
    u32 x_chars;
    u32 y_chars;
    u32 fill_attribute;
    u32 flags;
    u16 show_window;
    u16 reserved2;
    void *reserved2_data;
    void *standard_input;
    void *standard_output;
    void *standard_error;
};

typedef char AssertMainChainStartupInfoSize[
    sizeof(MainChainStartupInfo) == 0x44 ? 1 : -1];

extern void *g_MainChainInstanceMutex; // DAT_00497ba8
extern u32 g_MainChainRuntimeFlags; // DAT_00491ff4
extern u8 g_MainChainAlternateLaunchPath; // DAT_00492518

extern "C" void *TH10_STDCALL CreateMutexA(void *attributes,
                                             i32 initial_owner,
                                             const char *name);
extern "C" u32 TH10_STDCALL GetLastError();
extern "C" u32 TH10_STDCALL GetModuleFileNameA(void *module,
                                                 char *path, u32 capacity);
extern "C" u32 TH10_STDCALL GetConsoleTitleA(char *title, u32 capacity);
extern "C" void TH10_STDCALL GetStartupInfoA(MainChainStartupInfo *info);

extern void AppendMainChainDuplicateInstanceDiagnostic(); // 0x0044b8e0
extern i32 DoesMainChainFileExist(const char *path); // 0x0044b4d0
extern char *FindMainChainPathExtension(const char *path); // 0x00452930
extern i32 CompareMainChainPathExtensionNoCase(const char *left,
                                               const char *right); // 0x0046054e
// Native 0x0043a290 requires EBX=destination and EDI=capacity; this normal
// C++ boundary preserves its source/destination relationship.
extern void ResolveMainChainShellLinkPath(const char *source,
                                          char *destination,
                                          u32 destination_capacity);

} // namespace

i32 InitializeMainChainHostEnvironment()
{
    g_MainChainInstanceMutex = CreateMutexA(0, 1, "Touhou 10 App");
    if (GetLastError() == 0xb7U) {
        AppendMainChainDuplicateInstanceDiagnostic();
        return -1;
    }

    MainChainStartupInfo startup_info;
    startup_info.cb = sizeof(startup_info);
    memset(reinterpret_cast<u8 *>(&startup_info) + sizeof(startup_info.cb), 0,
        sizeof(startup_info) - sizeof(startup_info.cb));

    char module_path[0x105];
    char console_title[0x105];
    (void)GetModuleFileNameA(0, module_path, sizeof(module_path));
    (void)GetConsoleTitleA(console_title, sizeof(console_title));
    GetStartupInfoA(&startup_info);

    if (startup_info.title == 0) {
        g_MainChainRuntimeFlags |= 0x40U;
    } else {
        char *const extension = FindMainChainPathExtension(startup_info.title);
        if (DoesMainChainFileExist(startup_info.title) != 0 && extension != 0) {
            if (CompareMainChainPathExtensionNoCase(extension, ".lnk") == 0) {
                do {
                    ResolveMainChainShellLinkPath(startup_info.title,
                        console_title, 0x104);
                } while (CompareMainChainPathExtensionNoCase(
                    FindMainChainPathExtension(console_title), ".lnk") == 0);
            } else {
                strcpy(console_title, startup_info.title);
            }

            if (strcmp(module_path, console_title) != 0)
                g_MainChainAlternateLaunchPath = 1;
        }
        g_MainChainRuntimeFlags &= ~0x40U;
    }

    return g_MainChainInstanceMutex != 0 ? 0 : -1;
}

} // namespace th10
