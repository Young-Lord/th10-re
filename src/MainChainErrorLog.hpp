#pragma once

#include "Th10Types.hpp"

namespace th10 {

// Error-log object layout (the static base is DAT_00474f70):
//   +0x0000 char text[0x2000];        // g_MainChainErrorBuffer
//   +0x2000 char *cursor;             // g_MainChainErrorCursor (DAT_00476f70)
//   +0x2004 u8 show_dialog_on_exit;   // g_MainChainErrorShowDialog

// TH10 0x0044b810. Native ECX = log object, stack = printf format + args
// (cdecl, callee returns the format pointer in EAX). Locks the error-log
// critical section (DAT_004922bc, nesting byte DAT_0049231f), vsprintf's
// into an 8 KiB stack buffer and appends to the object buffer when the
// text would still end below object + 0x1fff.
char *AppendMainChainErrorLogFormatEcxEfxAbi(void *log_object,
                                             const char *format, ...);

// TH10 0x0044b8e0. Native EDI = log object, stack = printf format + args.
// Same append as above with a 0x200-byte stack buffer, and additionally
// raises the exit-dialog flag at object + 0x2004 (the error path used by
// e.g. LoadRenderOwnerCachedSurfaceFromFile).
char *AppendMainChainErrorLogFormatEdiEfxAbi(void *log_object,
                                             const char *format, ...);

} // namespace th10
