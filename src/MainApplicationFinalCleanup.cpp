#include <string.h>

#include "MainApplicationFinalCleanup.hpp"
#include "Th10Platform.hpp"

namespace th10 {

namespace {

struct MainChainPointerTable {
    void *entries[0x2800];
    u32 release_entries;
};

typedef char AssertMainChainPointerTableSize[
    sizeof(MainChainPointerTable) == 0xa004 ? 1 : -1];

extern u8 g_MainChainConfigBytes[0x34]; // TH10 DAT_00491d48
extern void *g_GlobalMidiOutput; // TH10 DAT_00491fe8
extern char g_MainChainErrorBuffer[]; // TH10 DAT_00474f70
extern char *g_MainChainErrorCursor; // TH10 DAT_00476f70
extern u8 g_MainChainErrorShowDialog; // TH10 DAT_00476f74
extern Win32CriticalSection g_MainChainCriticalSections[7]; // 0x492274
extern u32 g_ScreenSaverWasActive; // TH10 DAT_0049251c
extern u32 g_LowPowerWasActive; // TH10 DAT_00492520
extern u32 g_PowerOffWasActive; // TH10 DAT_00492524
extern MainChainPointerTable *g_MainChainPointerTable; // TH10 DAT_00491be0

extern void SaveMainChainBufferNativeCount(const char *path, const void *data,
                                           u32 bytes); // TH10 0x0044b540
extern void DestroyGlobalMidiOutputStageOneEdiAbi(void *output); // 0x43ae20
extern void DestroyGlobalMidiOutputStageTwoEaxAbi(void *output); // 0x43a910
extern void FreeMainChainFinalAllocation(void *pointer); // TH10 0x004524a1
extern void ReleaseMainChainFinalBuffer(void *pointer); // TH10 0x00452422
extern void AppendMainChainErrorText(const char *text); // TH10 0x0044b810
extern void ShowMainChainErrorDialog(const char *text, const char *title,
                                     u32 flags);
extern void TH10_STDCALL DeleteCriticalSection(void *critical_section);
extern void SetMainChainSystemParameter(u32 action, u32 value, u32 flags);
extern void EnableMainChainIme(void *window, i32 enable);
extern i32 PeekMainChainMessage(void *message, void *window, u32 minimum,
                                u32 maximum, u32 remove_flags);
extern void TranslateMainChainMessage(const void *message);
extern void DispatchMainChainMessage(const void *message);

const char kErrorSeparator[] =
    "---------------------------------------------------------- \r\n";
const char kErrorLogTitle[] = "log";
const char kErrorLogPath[] = "./log.txt";
const char kConfigPath[] = "th10.cfg";
const char kRetryErrorMessage[] =
    "\x8d\xc4\x8b\x4e\x93\xae\x82\xf0\x97\x76\x82\xb7\x82\xe9"
    "\x83\x49\x83\x76\x83\x56\x83\x83\x93\x82\xaa\x95\xcf\x8d"
    "\x58\x82\xb3\x82\xea\x82\xbd\x82\xcc\x82\xc5\x8d\xc4\x8b"
    "\x4e\x93\xae\x82\xb5\x82\xdc\x82\xb7\r\n";

void DestroyGlobalMidiOutput()
{
    if (g_GlobalMidiOutput != 0) {
        DestroyGlobalMidiOutputStageOneEdiAbi(g_GlobalMidiOutput);
        void *const output = g_GlobalMidiOutput;
        if (output != 0) {
            DestroyGlobalMidiOutputStageTwoEaxAbi(output);
            FreeMainChainFinalAllocation(output);
        }
    }
    g_GlobalMidiOutput = 0;
}

void FlushMainChainErrorLog()
{
    if (g_MainChainErrorCursor == g_MainChainErrorBuffer)
        return;

    AppendMainChainErrorText(kErrorSeparator);
    if (g_MainChainErrorShowDialog != 0)
        ShowMainChainErrorDialog(g_MainChainErrorBuffer, kErrorLogTitle, 0x10);
    SaveMainChainBufferNativeCount(kErrorLogPath, g_MainChainErrorBuffer,
        static_cast<u32>(strlen(g_MainChainErrorBuffer)));
}

void DestroyMainChainPointerTable()
{
    MainChainPointerTable *const table = g_MainChainPointerTable;
    if (table == 0)
        return;

    if (table->release_entries != 0) {
        for (u32 index = 0; index != 0x2800; ++index) {
            if (table->entries[index] != 0)
                ReleaseMainChainFinalBuffer(table->entries[index]);
        }
    }
    FreeMainChainFinalAllocation(table);
    // The native final-exit path deliberately leaves DAT_00491be0 stale.
}

} // namespace

void FinalizeMainChainApplication()
{
    SaveMainChainBufferNativeCount(kConfigPath, g_MainChainConfigBytes,
                                   sizeof(g_MainChainConfigBytes));
    DestroyGlobalMidiOutput();
    FlushMainChainErrorLog();

    for (u32 index = 0; index != 7; ++index)
        DeleteCriticalSection(&g_MainChainCriticalSections[index]);

    SetMainChainSystemParameter(0x11, g_ScreenSaverWasActive, 2);
    SetMainChainSystemParameter(0x55, g_LowPowerWasActive, 2);
    SetMainChainSystemParameter(0x56, g_PowerOffWasActive, 2);
    EnableMainChainIme(0, 1);
    DestroyMainChainPointerTable();
}

void PrepareMainChainStartupRetry()
{
    g_MainChainErrorCursor = g_MainChainErrorBuffer;
    g_MainChainErrorBuffer[0] = 0;
    AppendMainChainErrorText(kRetryErrorMessage);
    if (g_MainChainErrorShowDialog == 0)
        EnableMainChainIme(0, 1);

    u8 message[0x1c];
    for (u32 count = 0; count != 60; ++count) {
        if (PeekMainChainMessage(message, 0, 0, 0, 1) != 0) {
            TranslateMainChainMessage(message);
            DispatchMainChainMessage(message);
        }
    }
}

} // namespace th10
