# ECL Script Test Menu — "Spt Test" (0x42bad0 / 0x42bb30 / 0x42bbd0 / 0x42bc30 / 0x42be10 / 0x42bf30 / 0x42bfc0 / 0x42c360)

Reconstructed in `src/ScriptTestMenu.cpp/.hpp`. A debug menu manager
published at `DAT_00477844` (0x2e4 bytes) that enumerates
`../../data/*.ecl`, loads the selected script through the shared whole-file
loader 0x0044b360, hands the decoded section list to a 0x1098-byte viewer
object (vtable 0x46d0f0) and runs the resulting context-list manager
(0x00450500, vtable 0x46d0d8) through `RunEclContextListEdiStackAbi`
(0x0044fd10). It is the smaller sibling of the select menu 0x0040a450
(src/EclSelectMenu.cpp) with its own continuation worker entry 0x0042be10.
`CreateScriptTestManager` (0x0042bf30) has **no static caller** — a debug
entry point reachable only by hand.

## Manager layout

`+0x8/+0xc` chain elements, `+0x10` embedded thread control (0x2b4 bytes),
`+0x34/+0x38` name list/count, `+0x3c` menu cursor (0 = file list, 1 =
script list, 2 = quit; `+0x40` mirror), `+0x44` seed (3), `+0x10c` seed (1),
`+0x114` file cursor (`+0x118` mirror), `+0x11c` count mirror, `+0x1e4`
seed (1), `+0x1ec` script cursor (`+0x1f0` mirror), `+0x1f4` mirror,
`+0x2bc` mirror, `+0x2c4/+0x2c8/+0x2cc` position readout (seeded
0 / 32.0f / 0), `+0x2d0` limit (1000), `+0x2d4` continuation-busy flags,
`+0x2d8` viewer object, `+0x2dc` context-list manager, `+0x2e0` decoded
section list.

## 0x0042bb30 — `InitializeScriptTestManagerEdxAbi`

The native body seeds ~17 fields (including the 0x4703e4 vtable at +0x10
and the 999/1 cursor seeds) and then `rep stos`-zeroes 0xb9 dwords
(0x2e4 bytes) — **every seed is erased**; only `[obj] |= 2`, the
`DAT_00477844` publication and the return survive. Implemented as the
observable tail with the dead stores documented.

## 0x0042bbd0 — `RegisterScriptTestSchedulerRecordsEbxAbi`

Creates two records via 0x00449ed0 with flags bit 1 **set** (enabled):
update callback `0x0042c580` (`jmp 0x42bfc0`) added to the calculation
chain at priority 5 through 0x00449ae0, draw callback `0x0042c590`
(`mov edi,ecx; call 0x42c360`) added to the draw chain at priority 0x27
through 0x00449b70. Stored at `manager+8/+0xc`. Returns 0.

## 0x0042bf30 — `CreateScriptTestManager`

`operator new(0x2e4)` → init → register. Native quirk: on an allocation
failure the registration still runs with a null manager (its `manager+8`
store faults) — preserved. The `register != 0` failure branch of the
native (destroy + delete + return 0) is dead but modeled.

## 0x0042bad0 — `ReleaseScriptTestMenuFileNamesEsiAbi`

Frees each non-null name, the list itself, and clears +0x34 — the native
stores the clear twice (0x42bb17/0x42bb1e, quirk noted).

## 0x0042bfc0 — `UpdateScriptTestMenuEcxAbi` (update body, callback 0x42c580)

Native `__thiscall` (ECX), stack-cookie prologue elided; always returns 1.
Dispatch on `+0x30` handles sub-states 0, 1 and 4 only.

- **0 — enumerate.** Releases the old name list, counts
  `../../data/*.ecl` entries with FindFirstFileA/FindNextFileA/FindClose,
  stores the count at `+0x38`, then `malloc(count*4)` (result stored
  **unchecked** — a failure writes through null, quirk) and a second
  enumeration pass copies each `cFileName` (inline strlen + malloc(len+1)
  + same-length copy). `FindNextFileA` failure breaks without advancing
  the write index. Seeds (native 0x42c298): `+0x44 = 3`, `+0x10c = 1`,
  then the three-way compares run against a **zeroed** index register, so
  every cursor seed is `value < 0 ? value - 1 : 0`:
  `+0x3c` from 3, `+0x114` from the count, `+0x1ec` from 1 — all yield 0.
  Also `+0x11c = count`, `+0x1e4/+0x1f4/+0x2bc = 1`, `+0x30 = 1`,
  `+0x2c4/+0x2cc = 0`, `+0x2c8 = 32.0f`, `+0x2d0 = 1000`.
- **1 — menu.** Copies `+0x3c` to `+0x40`, shifts on the raw byte masks
  (`byte DAT_00474e36 | byte DAT_00474e34`, 0x20 up / 0x10 down), then
  dispatches the shifted value:
  - **0:** copies `+0x114` to `+0x118`, shifts it on 0x80/0x40 byte masks;
    the `dword DAT_00474e36 & 0x1001` confirm clears bit 1 of `+0x2d4` and
    schedules the continuation worker 0x0042be10 via 0x0044c1c0
    (`EAX = manager+0x10`, `EDI = 0x42be10`, stack = manager) — dedicated
    boundary `StartScriptTestContinuationWorker` — and sets `+0x30 = 2`.
  - **1:** requires `+0x2d8 != 0` (else no-op); copies `+0x1ec` to
    `+0x1f0`, shifts it via `PollMenuInputState(0x80/0x40)` +
    `ShiftManagerSelector`; the `dword DAT_00474e38 & 0x1001` confirm
    releases the previous context-list manager (vtable slot 5, flag 1),
    reads `[viewer+0x8c][cursor*8]` and calls 0x00450500 to create the new
    context list (native EDI = the viewer, captured into the manager's
    `+0x102c`), stores it at `+0x2dc` and sets `+0x30 = 4`.
  - **2:** on `DAT_00474e36 & 0x1001`,
    `RequestGameStateTransitionEaxStackAbi(DAT_00491c28, 3)` (quit).
- **4 — run.** `RunEclContextListEdiStackAbi([+0x2dc], 1.0f)`, then when
  `[[list+4]+4] == 0` (unchecked inner deref, quirk — also taken with a
  null list) releases the manager and returns to sub-state 1.

## 0x0042c360 — `DrawScriptTestMenuEdiAbi` (draw body, callback 0x42c590)

Native EDI = manager, always returns 1. Renders through
`AsciiManager::AddFormatTextSelected` (0x00401690) with the color word
`+0x8974` of `DAT_004776e0`:

1. Header `"Spt Test\n"` at (0, 0, 0) — color untouched.
2. Sub-state 3 (fall-through case): color `0xffa0a0a0`,
   `"Pos %.3d %.3d"` with `(int)[+0x2c8], (int)[+0x2c4]` at (32, 16);
   reset color `0xffffffff`.
3. Sub-state 2: color `0xffff4040`, `"Loading %s"` with the selected file
   name `[+0x34][[+0x114]]` at (32, 16); reset color.
4. Sub-state 1: `"File %s"` (or `"File not found."`) at (42, 16);
   `"Ecl %s"` from `[viewer+0x8c][[+0x1ec]*8]` (or `"Ecl not load"`) at
   (42, 24); `"Quit"` at (42, 36); the caret `">"` at
   `(32, [+0x3c]*10.0f + 16.0f)` (`flt_470c1c`, `flt_470b48`); reset color.

Sub-states other than 1/2/3 draw only the header line.

## 0x0042be10 / 0x0042be20 — load continuation

The worker entry ignores its argument and re-reads `DAT_00477844` into
ESI. Body: `sprintf(local 0x104 buffer, "%s", [+0x34][[+0x114]])`;
releases `+0x2dc` (vtable slot 5, flag 1; the null store is written twice,
quirk) and frees `+0x2e0`; reloads the file with
`LoadMainChainFile(name, 0, 0)` (0x0044b360) into `+0x2e0`; allocates the
0x1098-byte viewer (`operator new`, vtable 0x46d0f0, dead stores at
+0x1090/+0x1094 erased by the full 0x426-dword zeroing; the earlier
+0x1010 claim was a misread — raw bytes 0x42bec0..0x42bece show only the
two 0x1090/0x1094 stores), stores it
at `+0x2d8` and calls its vtable+0 method with the section list — the call
is issued **unchecked**, so an allocation failure dereferences null
(quirk preserved). Seeds `+0x1f4 = [viewer+8]` and
`+0x1ec = [viewer+8] <= 0 ? count-1 : 0` (i.e. -1 when empty), then
`+0x30 = 1`. Returns 0.

## 0x0042bc30 — `DestroyScriptTestMenuStackAbi` (stack arg, `ret 4`, SEH frame elided)

1. `ReleaseScriptTestMenuFileNamesEsiAbi`.
2. Removes both chain elements (`+0x8` then `+0xc`) inside the scheduler
   lock with the activity-depth byte bracketed
   (`CallbackSchedulerApi::RemoveSynchronized`, matching the inline
   Enter/LeaveCriticalSection(0x492274) + byte_49231c sequence).
3. Frees `+0x2e0` (malloc domain).
4. Viewer `+0x2d8`: restores the base vtable `0x46d0f0` into `[viewer]`,
   frees `[viewer+0x8c]`, `operator delete(viewer)`, clears `+0x2d8`.
5. Releases `+0x2dc` (virtual release, flag 1).
6. Releases the large render-owner slots at `DAT_00491c10 + 0x3ad088` and
   `+0x3ad084` (0x00447810 + operator delete + null).
7. Clears `DAT_00477844`, restores the vtable `0x4703e4` at `manager+0x10`
   and stops the embedded thread control (0x0044c150, native ESI).

Delete wrappers: 0x0042bf80 (`ret 4`, conditional delete on flag bit 0) and
0x0042bfa0 (plain ret, null-checked delete).

## Boundaries

- `0x0044c1c0` (worker entry 0x0042be10) — boundary
  `StartScriptTestContinuationWorker`; `0x00450500` — boundary
  `CreateEclContextListStackAbi` (its EDI capture of the viewer into
  `+0x102c` and the 0x00450470 record init stay inside the ECL domain).
- `0x0044fd10` `RunEclContextListEdiStackAbi`, `0x0044b360`
  `LoadMainChainFile`, `0x0044bea0` `ShiftManagerSelector`, `0x0040ace0`
  `PollMenuInputState`, `0x0040ac90`
  `RequestGameStateTransitionEaxStackAbi`, `0x0044c150`
  `StopThreadControl`, the scheduler API, CRT malloc/free/new/delete and
  the FindFirstFileA family — reconstructed elsewhere or platform
  boundaries.
