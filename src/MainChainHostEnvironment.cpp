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
// TH10 0x0043a290. Native usercall: EBX = destination, EDI = capacity in
// wide chars, stack = source path (`ret 4`); returns 1 when a link
// resolved and 0 otherwise.
extern i32 ResolveMainChainShellLinkPath(const char *source,
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

namespace {

// CLSID_ShellLink (0x46954c), IID_IShellLinkA (0x46931c) and
// IID_IPersistFile (0x46997c) — byte-exact copies of the binary's GUIDs.
const u8 k_shell_link_clsid[16] = {
    0x01, 0x14, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46};
const u8 k_ishell_link_a_iid[16] = {
    0xee, 0x14, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46};
const u8 k_ipersist_file_iid[16] = {
    0x0b, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46};

extern "C" i32 TH10_STDCALL CoInitialize(void *reserved);
extern "C" void TH10_STDCALL CoUninitialize();
extern "C" i32 TH10_STDCALL CoCreateInstance(const u8 *clsid, void *outer,
                                             u32 class_context,
                                             const u8 *iid, void **object);
extern "C" i32 TH10_STDCALL MultiByteToWideChar(u32 code_page, u32 flags,
                                                const char *source,
                                                i32 source_bytes,
                                                u16 *destination,
                                                i32 destination_chars);

typedef i32 (TH10_STDCALL *ComQueryInterfaceFn)(void *self, const void *iid,
                                                void **out);
typedef u32 (TH10_STDCALL *ComReleaseFn)(void *self);
// IShellLinkA::GetPath (vtable slot 3, 0x0c).
typedef i32 (TH10_STDCALL *ShellLinkGetPathFn)(void *self, char *path,
                                               i32 capacity, void *find_data,
                                               u32 flags);
// IPersistFile::Load (vtable slot 5, 0x14).
typedef i32 (TH10_STDCALL *PersistFileLoadFn)(void *self, const u16 *path,
                                              u32 mode);

extern void *AllocateMainChainObject(u32 bytes); // TH10 0x452493
extern void FreeMainChainObject(void *object);   // TH10 0x4524a1

void *GetComSlot(void *object, u32 index)
{
    return static_cast<void **>(*static_cast<void **>(object))[index];
}

} // namespace

// TH10 0x0043a290 real body. Resolves a ".lnk" launch path through the
// shell link COM object into `destination`. The original runs
// CoInitialize/CoUninitialize around every call. Failure paths release
// the already-created COM objects exactly like the original; the operator
// new result for the wide-path scratch is not checked, matching the
// native.
i32 ResolveMainChainShellLinkPath(const char *source, char *destination,
                                  u32 destination_capacity)
{
    if (destination == 0)
        return 0;

    i32 resolved = 0;
    (void)CoInitialize(0);

    void *shell_link = 0;
    if (CoCreateInstance(k_shell_link_clsid, 0, 1U /* INPROC_SERVER */,
                         k_ishell_link_a_iid,
                         &shell_link) >= 0 && shell_link != 0) {
        void *persist_file = 0;
        if (reinterpret_cast<ComQueryInterfaceFn>(
                GetComSlot(shell_link, 0))(
                shell_link, k_ipersist_file_iid,
                &persist_file) >= 0 && persist_file != 0) {
            u16 *const wide_path = static_cast<u16 *>(
                AllocateMainChainObject(destination_capacity * 2U));
            // The returned character count is ignored, like the native.
            (void)MultiByteToWideChar(0, 0, source, -1, wide_path,
                static_cast<i32>(destination_capacity));
            if (reinterpret_cast<PersistFileLoadFn>(
                    GetComSlot(persist_file, 5))(
                    persist_file, wide_path, 0U /* STGM_READ */) >= 0) {
                u8 find_data[0x138]; // WIN32_FIND_DATAA scratch, unread
                if (reinterpret_cast<ShellLinkGetPathFn>(
                        GetComSlot(shell_link, 3))(
                        shell_link, destination,
                        static_cast<i32>(destination_capacity),
                        find_data, 0U) >= 0) {
                    resolved = 1;
                }
            }
            FreeMainChainObject(wide_path);
            (void)reinterpret_cast<ComReleaseFn>(
                GetComSlot(persist_file, 2))(persist_file);
        }
        (void)reinterpret_cast<ComReleaseFn>(
            GetComSlot(shell_link, 2))(shell_link);
    }

    CoUninitialize();
    return resolved;
}

} // namespace th10
