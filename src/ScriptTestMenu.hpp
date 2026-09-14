#pragma once

#include "Th10Types.hpp"

namespace th10 {

// ECL script test menu manager ("Spt Test", TH10 DAT_00477844, 0x2e4 bytes).
// Layout:
//   +0x0000 flag word (bit 1 set by the initializer)
//   +0x0008 update chain element, +0x000c draw chain element
//   +0x0010 embedded thread control (continuation worker for the load)
//   +0x0034 char* file-name list, +0x0038 entry count (../../data/*.ecl)
//   +0x003c menu cursor (0 = file list, 1 = script list, 2 = quit)
//   +0x0044 cursor limit seed (3), +0x010c selected flag seed (1)
//   +0x0114 file-list cursor (+0x118 mirror), +0x011c count mirror
//   +0x01e4 flag seed (1), +0x01ec script cursor (+0x01f0 mirror)
//   +0x01f4 flag mirror, +0x02bc flag mirror
//   +0x02c4/+0x02c8/+0x02cc Float3 position readout, +0x02d0 limit (1000)
//   +0x02d4 continuation-busy flags (bit 1 cleared before scheduling)
//   +0x02d8 loaded ECL viewer object (0x1098 bytes, vtable 0x46d0f0),
//           +0x02dc running context-list manager (0x103c bytes),
//           +0x02e0 decoded section list from the file load

// TH10 0x0042bb30. Native EDX = manager. Observable behavior: zero the
// 0x2e4-byte manager, set flag bit 1 and publish DAT_00477844 (all the
// native's explicit field seeds are erased by its trailing memset — quirk).
void *InitializeScriptTestManagerEdxAbi(void *manager);

// TH10 0x0042bbd0. Native EBX = manager. Creates the two disabled scheduler
// records: update callback 0x0042c580 (jmp 0x0042bfc0) at calculation
// priority 5 and draw callback 0x0042c590 (ECX -> EDI, call 0x0042c360) at
// draw priority 0x27. Returns 0.
i32 RegisterScriptTestSchedulerRecordsEbxAbi(void *manager);

// TH10 0x0042bf30. operator new(0x2e4) + initialize + register. On an
// allocation failure the native still runs the registration with a null
// manager (which stores through null+8) — quirk preserved.
void *CreateScriptTestManager();

// TH10 0x0042bad0. Native ESI = manager, plain ret. Frees every non-null
// file-name string, then the list itself, and clears +0x34.
void ReleaseScriptTestMenuFileNamesEsiAbi(void *manager);

// TH10 0x0042bfc0. Native __thiscall (ECX = manager) reached through the
// jmp thunk 0x0042c580. Sub-state +0x30: 0 = enumerate ../../data/*.ecl and
// seed cursors, 1 = menu (cursor 0/1 shift their lists, confirm schedules
// the load continuation or starts the context list, cursor 2 requests game
// state 3), 4 = run the context list until it drains. Always returns 1.
i32 UpdateScriptTestMenuEcxAbi(void *menu);

// TH10 0x0042c360. Native EDI = manager (the draw callback thunk 0x0042c590
// copies the ECX callback argument there). Renders the menu lines through
// AsciiManager::AddFormatTextSelected and always returns 1.
i32 DrawScriptTestMenuEdiAbi(void *menu);

// TH10 0x0042be10 / 0x0042be20. Continuation worker entry (argument
// ignored; the manager is re-read from DAT_00477844) and body: formats the
// selected file name, releases the previous viewer/state, reloads the ECL
// through the shared 0x0044b360 loader, allocates the 0x1098-byte viewer,
// initializes it with the decoded section list and returns to sub-state 1.
i32 RunScriptTestLoadContinuation(void *menu);

// TH10 0x0042bc30. Native stack argument (`ret 4`), SEH frame. Full
// teardown: file list, both scheduler records (lock-bracketed removal), the
// decoded section list, the viewer object (base vtable 0x46d0f0 restore,
// buffer free, operator delete), the context-list manager (virtual release
// flag 1), the two large render-owner slots at +0x3ad084/+0x3ad088, then
// the DAT_00477844 clear, the +0x10 vtable (0x4703e4) restore and
// StopThreadControl on the embedded thread control.
void DestroyScriptTestMenuStackAbi(void *menu);

// TH10 0x0042bf80 (`ret 4`) and 0x0042bfa0 (plain ret): destroy plus the
// conditional/unconditional operator delete of the manager.
void *DeleteScriptTestMenuEsiStackAbi(void *manager, i32 free_flag);
void ReleaseScriptTestMenuEsiAbi(void *manager);

} // namespace th10
