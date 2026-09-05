* `0x42d920 RunManagerStateBody4`: audio settings menu. It drives the
  six-row cursor, applies 5-point clamped BGM/SE volume changes, rotates the
  presentation/audio mode, restores default values, coordinates menu entity
  stop words, and returns to state 2 after the exit delay.

* `0x433ef0 RunManagerStateBodyE`: score-record listing. It loads score
  records, constructs headers and list-row entities, streams text entries on
  alternating frames, maintains a ten-row scrolling window around the cursor,
  updates selection presentation, and releases all score/list resources when
  the menu is dismissed.

* `0x433570 RunManagerStateBody10`: replay-save naming screen. It parses
  twenty-five replay slots, selects a target, edits the 13-column character
  grid, commits the entered name through `0x4297b0`/`0x429b60`, reparses the
  saved replay object, and restores the title BGM during its exit cleanup.

* `0x432cb0 RunManagerStateBodyF`: post-run name-entry screen. It queues
  `bgm/th10_17.wav`, loads menu presentation scripts, exposes a 13-column
  alphabet grid and modifies an eight-byte name buffer at `manager+0x58dc`.
  Confirmation supports inserting alphabet characters or spaces, deleting a
  byte, and committing; cancellation removes one byte or leaves the screen.
  After six frames of exit cleanup it enters state `0x10`.

* `0x4315c0 RunManagerStateBodyC`: replay-selection screen. Substate 0
  prepares twenty-five numbered replay parser slots at `manager+0x59d4`,
  while the native body also scans user replay files into the remaining
  slots. Substate 2 moves through the replay rows, gates chapter selection
  from each parsed record, reserves feedback channels, and enters the replay
  setup state on confirmation. Substate 3 publishes the selected replay
  configuration and transitions to state 3; substate 5 releases all fifty
  parser objects and returns to state 2.

Live Ghidra MCP analysis of the locked TH10 image established four lifecycle
entry contracts and corresponding symbols. These are semantic boundaries for the state machine; the manager constructor/worker bodies are documented below
while the title in-place release `0x417c80` and the game-manager channel
controllers `0x42cdf0`/`0x42d260` remain separate targets.

* `0x4180e0 CreateTitleScreen`: allocates `0x60` bytes, clears the object,
  initializes `+0x24` through `0x418c40`, invokes the render-owner virtual
  callback at `+0x14`, stores the one stack argument at `+0x5c`, publishes
  `DAT_00477810`, sets flag bit `2`, and starts callback thread `0x417c70`.
  The function returns with `ret 4`.
* `0x418150 DestroyTitleScreen`: null-checks the object, invokes in-place
  teardown `0x417c80`, then frees it. The wrapper does not clear the global.
* `0x42cd50 CreateGameManager`: allocates `0x5acc` bytes, initializes via
  `0x42c920`, calls `0x44c150`, sets the worker fields, and starts the CRT
  thread at `0x42c9f0`.
* `0x40b940 CreateTransitionObject`: allocates and zeroes a `0x28`-byte
  record, sets flag bit `1`, publishes `DAT_00477700`, and calls `0x40b560`;
  failure tears down with `0x40b7b0` and frees the allocation.

The names and comments were written to the live Ghidra database through MCP.
No assembly substitute is used in the C++ reconstruction.

## Thread Bootstrap and Game-Manager Thread Control (update)

`0x00420ea0` is now implemented as `StartMainChainCallbackThread` in
`src/TitleGameManagerLifecycle.cpp`: it takes the context state lock
(`DAT_00491c28 + 0x6dc`), bumps the depth byte at `+0x6fa`, stops the
embedded `ThreadControl` at `+0x630`, seeds the entry `+0x644` and flags
`+0x63c`/`+0x638`, then starts the CRT thread and restores lock/depth.

`0x0044c150` was confirmed to be `StopThreadControl` operating on a
`ThreadControl` object rather than a no-argument periodic hook.  Both
`CreateGameManager` and `TeardownGameManagerInPlace` in
`src/TitleGameManagerLifecycle.cpp` now drive the game-manager worker through
the `ThreadControl` at `manager + 0x5ab0` (typed stop, seeded entry at
`+0x18`, active flag at `+0x10`, stop flag at `+0x0c`, CRT handle at `+0x04`,
thread id at `+0x08`).

## Game-Manager Constructor `0x0042c920` (update)

`ConstructGameManagerInPlace` is now implemented in the same file.  The native
body stores a destructor vtable (`0x0046ecf0`) and default seeds — including
the thread-control destructor marker `0x4703e4` at `+0x5ab0` — and then
unconditionally zeroes the full `0x5acc`-byte allocation:

```text
0042c9cf  mov ecx,0x16b3      ; 0x16b3 dwords == 0x5acc bytes
0042c9d4  mov edi,edx          ; this
0042c9d6  rep stosd            ; zeroes every word including +0x5ab0
0042c9d8  mov eax,[edx+4]
0042c9db  or  eax,0x2
0042c9df  mov [edx+4],eax      ; dword +4 bit 1 survives
0042c9e2  mov [0x0047784c],edx ; DAT_0047784c = this
```

None of the pre-memset stores survive, so the reconstruction models the
observable net result directly: `memset(manager, 0, 0x5acc)`, dword `+4`
`|= 2`, publish `DAT_0047784c`, return the manager.  The compiler-emitted
dead seeds are the defaults of the two embedded worker structures that
`InstallGameManagerWorkerChannels` later re-establishes (see below).

## Game-Manager Worker `0x0042c9f0` / `0x42caa0` (update)

`0x0042c9f0` is a CRT worker entry (argument unused; it re-reads the global
manager) and `0x0042caa0` is its channel-installation helper.  Both are now
implemented in `src/TitleGameManagerLifecycle.cpp`:

1. `InstallGameManagerWorkerChannels(manager)` (`0x42caa0`) creates two
   `ChainElem` records bound to the manager through `CallbackSchedulerApi`:
   a calculation record (priority `6`) stored at `manager + 0x0c` and a draw
   record (priority `3`) stored at `manager + 0x10`, each with its enabled
   bit cleared.  It then requests the two title data `ManagerWork` slots
   `0x19` ("title.anm", `.rdata 0x0046f0ac`) and `0x1a` ("title_v.anm",
   `.rdata 0x0046f0a0`) into `manager + 0x14` / `+0x18`.  A null result
   reports through `0x0044b810` (receiver `ECX = 0x00474f70`, message at
   `0x0046cb68`) and returns `-1` while leaving the published words in place.
   Success stores `1` at `manager + 0xf4`, clears `DAT_004918a4`, returns `0`.
2. `GameManagerWorkerThread` (`0x0042c9f0`) runs the installer; on `-1` it
   selects a fallback shared status (`DAT_00491fb8`) from runtime-flag
   bit 12 — `((~(flags >> 12) & 1) | 2)` — and returns.  On success it calls
   `KERNEL32!Sleep` (IAT slot `0x004660ac`) with `0x10` ms per iteration
   until the global-lifecycle manager's `+0x3ec` frame counter reaches
   `0x12c` or runtime-flag bit `0x80` requests an early exit, releases the
   loading slot at the render owner `+0x3ad070` (via `0x00447810` +
   `0x004524a1`), enables the calculation record at `manager + 0x0c`, and
   returns zero.

The two channel adapters the installation registers are modeled as
`GameManagerCalculationChannel` (`0x0042d2e0`) and `GameManagerDrawChannel`
(`0x0042d2f0`) in the same file.  Their native bodies are single-instruction
`mov ecx, eax; jmp` forwarders, so they simply call the game-manager
calculation controller `0x0042cdf0` / draw controller `0x0042d260` (both
separate targets) with the manager argument.

The teardown at `0x0042cb60` removes exactly these two records
(`+0x0c`/`+0x10`) under the callback lock via
`CallbackSchedulerApi::RemoveSynchronized`, matching the lock/activity-depth
pattern the native code shows around each `0x00449f60` call.  `DAT_00491be4`
is the `CallbackScheduler *` global (not an integer token) in this module.

## Title Sub-Object Initializer And Callback Worker (update)

`0x00418c40` `InitializeTitleScreenSubObject` and `0x00417c70`
`TitleScreenCallbackWorker` are now implemented in the same file.

`0x00418c40` clears the `+0x24` sub-object for `0x34` bytes, seeds two
600-unit extents (`+0x16`/`+0x18`), byte flags `+0x1a..+0x22`, dword `+0 =
0x100003`, bit `0x100` at `+0x30`, and copies the geometry defaults from
`DAT_00474e88/DAT_00474e8c/DAT_00474e90/DAT_00474e94` plus the word at
`DAT_00474e98` into `+0x04..+0x14`.  In `CreateTitleScreen` this initializer
runs before the whole `0x60`-byte allocation is zeroed, so its writes do not
survive there; the function is modeled as the standalone reusable
initializer it appears to be (native ABI is fastcall with an unused ECX
argument and a zero return).

`0x00417c70` is a one-call CRT wrapper:

```cpp
unsigned int __stdcall TitleScreenCallbackWorker()   // 0x00417c70
{
    return RunTitleScreenStartupBody(g_TitleScreen); // 0x00417870
}
```

It forwards the published title screen (`DAT_00477810`) to the startup
orchestration at `0x00417870`, which waits for the main-chain owner, registers
the title calc/draw records, and initializes the whole game-mode subsystem
set; that body remains a separate target.

The two title-screen record callbacks are implemented in the same file:

* `0x004187d0` `TitleScreenDrawCallback`: unless the title boot flag bit 2 at
  `+0x58` is set, zeroes the render-owner words `+0x4c/+0x50/+0x54/+0x58` and
  returns one.  A register-variant twin at `0x004187a0` reads the title
  pointer from `EAX` instead of `ECX`.
* `0x004187c0` `TitleScreenCalcCallback`: native body is `push ecx; call
  0x00418190; ret`, i.e. a forwarder that pushes the title screen and
  tail-invokes the per-frame calc controller at `0x00418190` (a separate
  target that dispatches mode starts, the "main" stage load, frame counters,
  and the calc/draw record enables across the whole mode-manager set).

## Startup / calc helper leaf notes

`0x00418b80` `ResetMainChainFrameStateBlock` is now implemented in
`src/TitleGameManagerLifecycle.cpp`; the following remaining helper carries
its owner in a register and is recorded here for a later transcription:

* `0x00417770` `sub_417770` is `__usercall` with owner in `EAX`; it drains an
  intrusive node list whose head is at `owner + 0x18` (`v1 = *(owner+0x18)`).
  For each node it saves `next = node[2]`, invokes the node vtable slot `+16`
  (`(*(node_vtbl+16))(node)`), unlinks through `node[1]`/`node[2]`
  (`*(node[1]+8) = node[2]`, and when present `node[2][4] = node[1]`), frees
  the node with `0x004524a1`, and continues until the saved next is null.

## Game-manager state helpers

* `0x0042c5c0` `SetGameManagerState` is now implemented in the same file
  (native: EAX = manager, ECX = state).  It stores the target state at
  `manager + 0x1c`, clears `+0x20`, and seeds/resets the `+0x2b0` prev
  counter / `+0x2b4` counter / `+0x2b8` float accumulator / `+0x2bc` rate
  pointer / `+0x2c0` flag block in the same idiom as
  `ResetMainChainFrameStateBlock`, returning the manager.  The manager calc
  controller calls it for every mode transition (`state 1/2/0xc/0xf/...`).
* `0x0042c670` `SpawnManagerEntityFromScript` is now implemented in the same
  file (native: EDI = script id, stack = manager; disasm-verified).  It
  allocates a `0x3ac`-byte pool VM record from the render-owner pool
  (`AllocatePoolVmEsiAbi`), marks record kind `0xf` at `+0x20` and flag bit
  `0x40000000` at `+0x35c`, assigns the script through `0x00449870`, links
  the record into the owner entity lists (`0x004489d0`) and stores the
  assigned id at `manager + 0x2c4 + 4*script_id`.
* `0x0042c770` `SetManagerSlotEntityStopWord` is now implemented in the same
  file (native: EAX manager, ECX slot, SI value).  It resolves the record
  stored at `manager + 0x2c4 + 4*slot` through the owner entity lists
  (`0x004491c0`), and when non-null writes the value to `record + 0x304`
  (the entity stop word), recursing into children when `record + 0x18` is
  null — the same entity/child traversal the manager teardown uses.
* The menu-body entity helpers `0x004497d0` `ResolveChildEntityByKind`,
  `0x00449470` `SetEntityStateWordByHandleSlot`, `0x00449250`
  `SetEntityStopWordByIdAndRun`, `0x004495e0` `ClearEntityFlag2ByHandleSlot`
  and the cursor widget `0x0044bea0` `ShiftManagerSelector` are now
  implemented and exported from `src/GameManagerState.cpp`.  All share the
  owner-entity resolution (`0x004491c0`) plus the two list idioms: the
  inline child-node chain embedded at `entity + 0x10` (`0x004497d0`) and the
  `+0x14` child list with `+0x18` count used by the stop-word / flag / run
  helpers.  The cursor record lives at `manager + 0x24` with its disabled-row
  list at `+0xb4` (count `+0xf8`), which is exactly the manager region the
  difficulty menu writes when trimming locked rows.  The slot stop/release
  pair `0x0042c6d0` `ReleaseManagerSlotEntity` (stop word 1 + slot clear)
  and `0x0042c750` `Call42C750` (0x00449250 forward with stop word 3), the
  cursor finalize `0x0044be70` `Call44BE70` (step the +0x8c count back,
  reload value/maximum from the +0x0c/+0x4c per-step arrays, clear the +0xd4
  disabled count) and the input poll `0x0040ace0` `PollMenuInputState`
  (masks the two u16 words at the 0x474e30 bank) are implemented and
  exported from the same file.  The shared menu feedback boundary
  `0x0043dc90` `ReserveContextChannel` (twelve channels of the 0x492590
  context: keys at `+0x620`, counts at `+0x650`, `0x200`-byte entry blocks
  at `+0x680`, per-kind scratch at `+0x408`) is implemented there as well.
* State bodies `0x00430320` `RunManagerStateBody6` (character select),
  `0x004306a0` `RunManagerStateBody7` (shot-type select) and `0x00430a60`
  `RunManagerStateBody8` (difficulty select) are implemented in
  `src/GameManagerStateBodies.cpp`, along with the six-row stage select
  `0x00430ff0` `RunManagerStateBody9`.  Body 6 drives sub-states over the
  `+0x20` step with a jump-table dispatch of `0..4`, spawns the `0x77`/`0x78`
  character scripts plus slots `0x5e`/`0x62`, and confirms/backs out through
  the `0x474e36` flag gates (`0xa` -> sub 4 -> state 2; `0x1001` -> set the
  selected script's stop word, resolve/focus the picked child kind and gate
  to sub 3 / state 7).  Body 8 runs the difficulty menu from the stage value
  at `0x474c68`, spawning script `stage + 0x96` and `0x64`; in extra mode
  (`0x474c74 == 4`) it trims locked rows out of the cursor's disabled list
  using the unlock bytes at `DAT_00477c3c + 3*stage + 0x1d888`, hides
  difficulty children when the `0x329d`-stride record words at `+0x4d0` /
  `+0x484c` / `+0x8bc8` are zero, and gates exits through the `0x2b4`
  counters (10/40-frame thresholds) into states `3`/`7`/`9` with the
  `0x474ca0` bit-`0x10` extra/finalize choice.  Body 7 runs the shot-type
  select on slots `0x63`/`0x7d` (id word 302), trims whole shot rows in extra
  mode with the six unlock bytes at `DAT_00477c3c + 0x1d888`, moves the
  cursor with the `0x40`/`0x80` input bits, toggles the four weapon children
  (kinds `cursor+121`, `cursor+123`, `122-cursor`, `124-cursor`) with stop
  words `6/6/1/1` on confirm, and exits into state 8 after persisting the
  selection into `DAT_00474c68` (adjusting the BGM handle record when the
  stage changed) or back into state 6.  Body 9 runs the six-row stage select
  from scripts `0x6a` and `DAT_00474c68 + 0x6b`, clamps the remembered row
  from `DAT_00491c08`, gates confirm on the per-record clear byte at
  `DAT_00477c3c[0x437c*(3*DAT_00474c68 + DAT_00474c6c) + 0x30*DAT_00474c74 +
  8*row + 0x4e9]`, scans the active save bank's nine clear bytes
  (`0x497aaa` / `0x497ad9`, chosen by 0x0044b010) into `DAT_00474cac`, and on
  the frame-40 finalize starts the stage with `DAT_00474c7c`/`0x474c80` =
  row + 1.

## Game-manager calculation controller `0x0042cdf0` (implemented)

The controller and draw controller `0x0042d260` are implemented in
`src/TitleGameManagerLifecycle.cpp` (`GameManagerCalculationChannel` /
`GameManagerDrawChannel` adapters plus `RunGameManagerCalculationBody` /
`RunGameManagerDrawBody`).  The transition helpers `0x0042c5c0` /
`0x0042c670` / `0x0042c770` were exported to `src/GameManagerState.cpp`
(`SetGameManagerState` / `SpawnManagerEntityFromScript` /
`SetManagerSlotEntityStopWord`) so the per-state bodies can live in their own
module; the state-1 calc body `0x0042d300` is implemented in
`src/GameManagerStateBodies.cpp` (`RunManagerStateBody1`).

`0x0042cdf0` receives the manager in `EAX` (its two channel adapters
`0x0042d2e0`/`0x0042d2f0` tail-jump here) and drives the whole manager state
machine from `manager + 0x1c`:

* **Demo/attract preamble** for states `1`/`2`: increments `dword_474CA4`;
  when `dword_474E30 & 0x160b` is set it resets that counter and skips the
  load block.  Otherwise once `dword_474CA4 >= 900` it sets `dword_474CA0`
  bit `0x20`, copies the `.rdata` string `off_476FF8[dword_474CA8]` into the
  `0x477710` scratch (byte copy, stop on NUL), parses it through
  `0x004296f0`, rotates `dword_474CA8` by `+1 mod 4`, scans eight
  stride-`0x24` records from `parse + 0xb0` for the first nonzero slot,
  stores that index to `dword_474C7C`/`0x474C80`, writes `dword_491FB8 = 12`,
  and copies the parse object's `+0x50/+0x54/+0x58` into the
  `0x474C68/0x474C6C/0x474C74` selector block (shifting `0x474C74` into
  `0x474C78`) while setting `dword_477848 = &unk_474788 + index*0x30`; then
  destroys the parse object (`0x004294a0` + `0x004524a1`), resets the counter
  and stores `manager[+0x1c] != 1` into `dword_491C00`.
* **Dispatch** over `manager[+0x1c]` to per-state leaves: `0` performs the
  owner-slot clears/releases (`0x004493e0` on `0x491c10 + 0x3ad06c/74/78/84/
  88`, plus `0x00409e50` on `0x4776e0 + 0x89a4`) and then BGM/gate setup;
  `1..0x10` dispatch to `0x42d300/0x42d420/0x42d920/0x42f540/0x430320/
  0x4306a0/0x430a60/0x430ff0/0x4315c0/0x431ee0/0x433ef0/0x432cb0/0x433570`,
  states `3`/`0xa`/`0xd` route through `0x00420c00` after writing a fallback
  `dword_491fb8` from runtime-flag bit 12, and `0xf`/`0xc` are reached from
  the preamble's transition block.
* **Common tail** (`0x0042d19f`, disasm-arbitrated: ghidra reading correct,
  idalib constant-folded differently):
  ```text
  42d1a0  mov edx,[ebx+0x2b4]   ; counter
  42d1a4  mov ecx,[ebx+0x2bc]   ; pointer to rate float
  42d1aa  mov [ebx+0x2b0],edx   ; previous counter
  42d1b0  flds  (%ecx)
  42d1b4  fcomps 0x00470b68     ; low threshold
  ...    branch to 0x42d1f1 when not above low / unordered
  42d1c0  flds  (%ecx)
  42d1c2  fcomps 0x00470b64     ; high threshold
  ...    branch to 0x42d1f1 when below high / unordered
  ; in-window (low < rate < high):
  42d1cf  flds 0x2b8(%ebx)      ; accumulator += 0x00470afc (1.0)
  42d1d6  fadds 0x00470afc
  42d1de  mov [ebx+0x2b4],edx + 1
  42d1e4  fstps 0x2b8(%ebx)
  return 1
  ; out-of-window / NaN:
  42d1f1  flds (%ecx); fadds 0x2b8(%ebx); fsts 0x2b8(%ebx)
  42d1ff  call 0x00463b2c        ; clock/ticks source
  42d205  mov [ebx+0x2b4],eax
  return 1
  ```
  Net effect: when the rate float at `manager+0x2bc` lies strictly between the
  `.rdata` thresholds `0x00470b68` and `0x00470b64` (≈ near 1.0), the
  accumulator `+0x2b8` advances by the `0x00470afc` step and the frame
  counter `+0x2b4` increments; otherwise the accumulator tracks the rate
  exactly and `+0x2b4` snapshots the `0x00463b2c` tick value.  Either way the
  function returns one.





