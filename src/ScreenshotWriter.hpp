#pragma once

#include "Th10Types.hpp"

namespace th10 {

struct MainChainContext;

// TH10 0x00420670. Native userpurge: EAX = output path, ESI = the main
// chain context (g_MainChainContext). Captures the current back buffer as
// a 640x480 24-bit BMP: the file header is built inside the context
// (+0x510..+0x51d), the file name is copied at +0x528, and the actual
// disk write happens on the _beginthread worker 0x00420540 so the game
// keeps running. The render-mode gate at +0xec accepts only mode 22
// (capture) and 23 (log-only); anything else logs the mother.cpp error
// and returns 1. Retained as a boundary for the native entry ABI.
i32 CaptureMainChainSnapshot(MainChainContext *context,
                             const char *output_path);

// TH10 0x00420540. The _beginthread worker: writes the 14-byte file
// header (0x492138), the 0x28-byte BITMAPINFOHEADER (0x492148) and the
// 0xE1000-byte pixel block (0x49214C) to the handle opened into
// 0x474C38, closing the handle and leaving the 0x4922A4 critical section
// (with the 0x49231E depth decrement) after every step, then frees both
// buffers and clears the thread handle 0x492134.
void SnapshotFileWriterThread();

} // namespace th10
