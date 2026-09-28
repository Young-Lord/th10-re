// Main-chain error log appenders (TH10 0x0044b810 / 0x0044b8e0). Every
// fatal/verbose message in the game funnels through these two printf-style
// appenders; the finished buffer is flushed by FinalizeMainChainApplication
// (log.txt + optional dialog).
#include "MainChainErrorLog.hpp"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "Th10Platform.hpp"

namespace th10 {

namespace {

extern Win32CriticalSection g_ErrorLogLock;   // TH10 stru_4922bc
extern u8 g_ErrorLogActivityDepth;            // TH10 byte_49231f

extern "C" void TH10_STDCALL EnterCriticalSection(void *critical_section);
extern "C" void TH10_STDCALL LeaveCriticalSection(void *critical_section);

// Offsets inside the error-log object (see MainChainErrorLog.hpp).
const u32 kLogCursorOffset = 0x2000U;
const u32 kLogDialogFlagOffset = 0x2004U;

// Shared tail: copy the formatted stack text to the cursor when it would
// still end below the object's 0x2000-byte text window. The EDI-ABI entry
// additionally raises the dialog flag while still holding the lock.
// Returns the format pointer (the native EAX result on both entries).
char *AppendErrorLogText(void *log_object, char *formatted,
                         const char *format, bool raise_dialog_flag)
{
    EnterCriticalSection(&g_ErrorLogLock);
    ++g_ErrorLogActivityDepth;

    const u32 text_bytes = static_cast<u32>(strlen(formatted));
    char *cursor = *reinterpret_cast<char **>(
        static_cast<char *>(log_object) + kLogCursorOffset);
    // Native check: (cursor + len) < object + 0x1fff, unsigned.
    if (reinterpret_cast<u32>(cursor + text_bytes)
        < reinterpret_cast<u32>(static_cast<char *>(log_object) + 0x1fffU)) {
        strcpy(cursor, formatted);
        cursor += text_bytes;
        *reinterpret_cast<char **>(
            static_cast<char *>(log_object) + kLogCursorOffset) = cursor;
        *cursor = '\0';
    }

    if (raise_dialog_flag)
        *(static_cast<u8 *>(log_object) + kLogDialogFlagOffset) = 1U;

    LeaveCriticalSection(&g_ErrorLogLock);
    --g_ErrorLogActivityDepth;
    return const_cast<char *>(format);
}

} // namespace

char *AppendMainChainErrorLogFormatEcxEfxAbi(void *log_object,
                                             const char *format, ...)
{
    // Native ECX = log_object; the formatted text lives in an 8 KiB
    // (0x2004 incl. security cookie) stack frame.
    char buffer[0x2000];
    va_list arguments;
    va_start(arguments, format);
    (void)vsprintf(buffer, format, arguments);
    va_end(arguments);
    return AppendErrorLogText(log_object, buffer, format, false);
}

char *AppendMainChainErrorLogFormatEdiEfxAbi(void *log_object,
                                             const char *format, ...)
{
    // Native EDI = log_object; 0x200-byte stack buffer. The dialog flag is
    // raised unconditionally, even when the append itself was skipped.
    char buffer[0x200];
    va_list arguments;
    va_start(arguments, format);
    (void)vsprintf(buffer, format, arguments);
    va_end(arguments);

    char *const result =
        AppendErrorLogText(log_object, buffer, format, true);
    return result;
}

} // namespace th10
