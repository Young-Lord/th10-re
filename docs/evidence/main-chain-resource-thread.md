# Main-Chain Resource Thread (`0x0043d080`)

## Scope

This document recovers the resource-thread entry at `0x0043d080` in
`resources/th10.exe`, its fixed 37-item WAV preload loop, the shared raw-data
slots, and the directly dependent consumer and teardown workers.  It is an
implementation plan for semantic C++ reconstruction.  Names such as
`MainChainResourceThread`, `RawWaveSlot`, and `SoundRoot` are descriptive;
they do not identify original source symbols.

The resource thread itself does not create DirectSound buffers.  It loads
archive entries into raw heap buffers.  A separate initialization worker takes
each buffer, clears its shared publication slot, builds a DirectSound buffer,
and releases the raw buffer.  Treating all of this as one synchronous
`LoadSounds()` routine would lose the observed cancellation and ownership
protocol.

## Thread ABI and Creation

`0x004201b0` directly invokes:

```cpp
CreateThread(0, 0, (LPTHREAD_START_ROUTINE)0x0043d080,
             (void *)0x00492590, 0, &temporary_thread_id);
```

It stores the returned handle at `0x004977ac`.  The entry at `0x0043d080`
does not access `[ESP+4]`, so the passed `0x00492590` has no established
meaning at this boundary.  The entry saves `ESI`, uses a plain `ret` rather
than `ret 4`, and does not explicitly set `EAX` before returning.  It is
therefore not directly representable as a conventional `DWORD WINAPI`
function, despite being passed to `CreateThread` as one.

A C++ implementation should use a real `DWORD WINAPI` adapter and keep the
semantic operation separate:

```cpp
void PreloadMainChainWaveResources();

DWORD WINAPI MainChainResourceThreadAdapter(void *) {
    PreloadMainChainWaveResources();
    return 0;
}
```

The adapter return value is a C++/Win32 boundary decision; it is not evidence
that the original entry returns a defined thread exit code.

## Exact Control Flow

The raw control flow is:

```text
i = 0
while (g_MainChainResourceGate != 2) {
    raw = LoadResourceEaxAbi(g_MainChainWaveNames[i], null, 0)
    g_MainChainRawWaveSlots[i] = raw
    if (raw == null)
        report("error : Sound ファイルが読込めません %s\\r\\n", name)
        return

    ++i
    if (i < 37)
        continue

    while (g_MainChainResourceGate == 0)
        Sleep(1)
    return
}
```

The first condition is checked before *every* load, not only at entry.  A
write of `2` therefore stops later work but does not cancel a currently active
`0x0044b360` call.  A non-null result is published before the next gate test.

The post-loop wait occurs only after all 37 successful loads.  It waits only
when the gate is exactly zero; any nonzero value, including `2`, releases the
thread.  The entry makes no completion-state write after the final load.

On an individual load failure, the slot for the failed index has already been
written as null.  Earlier successful slots stay published, the gate is not
updated, and no cleanup is attempted by this entry.  The error reporter is
called with `ECX = 0x00474f70`, format string `0x0046fdd4`, and the failing
name as its one format argument.

## Fixed Resource Table

`0x00474b40` is an array of exactly 37 `const char*` values.  The loop indexes
it and the output slots in the same ascending order.  These are archive leaf
names passed to `0x0044b360` in archive mode (`source_mode == 0`):

| Index | Name |
| ---: | --- |
| 0 | `se_plst00.wav` |
| 1 | `se_enep00.wav` |
| 2 | `se_pldead00.wav` |
| 3 | `se_power0.wav` |
| 4 | `se_power1.wav` |
| 5 | `se_tan00.wav` |
| 6 | `se_tan01.wav` |
| 7 | `se_tan02.wav` |
| 8 | `se_ok00.wav` |
| 9 | `se_cancel00.wav` |
| 10 | `se_select00.wav` |
| 11 | `se_gun00.wav` |
| 12 | `se_cat00.wav` |
| 13 | `se_lazer00.wav` |
| 14 | `se_lazer01.wav` |
| 15 | `se_enep01.wav` |
| 16 | `se_damage00.wav` |
| 17 | `se_item00.wav` |
| 18 | `se_kira00.wav` |
| 19 | `se_kira01.wav` |
| 20 | `se_kira02.wav` |
| 21 | `se_timeout.wav` |
| 22 | `se_graze.wav` |
| 23 | `se_powerup.wav` |
| 24 | `se_pause.wav` |
| 25 | `se_cardget.wav` |
| 26 | `se_option.wav` |
| 27 | `se_damage01.wav` |
| 28 | `se_timeout2.wav` |
| 29 | `se_invalid.wav` |
| 30 | `se_slash.wav` |
| 31 | `se_ch00.wav` |
| 32 | `se_ch01.wav` |
| 33 | `se_hint00.wav` |
| 34 | `se_extend.wav` |
| 35 | `se_bonus3.wav` |
| 36 | `se_water.wav` |

The table's storage order is descending address order, but the runtime index
order above is ascending from `0x00474b40`.  The 37 is an immediate loop
bound (`cmp esi, 0x25`), not a sentinel scan.

## Shared State and Ownership

The directly established shared globals are:

```text
0x004977ac  HANDLE/opaque result of the CreateThread call
0x004977b4  u32 resource gate; 0 waits after preload, 2 requests stop
0x004977c0  void* raw_wave_slots[37]
0x00474b40  const char* wave_names[37]
```

`0x004977c0` is also `0x00492590 + 0x5230`.  It is thus physically within the
large root object passed to `CreateThread`, even though the resource-thread
entry itself addresses it as an absolute global.  The raw-data array spans
`0x94` bytes.  The root's complete source type is not yet recovered.

`0x0043d120` confirms that each non-null raw slot is an individually owned
game-heap allocation: it iterates all 37 entries at root `+0x5230`, calls
`0x00452422` on each non-null value, then clears that slot.  It does not
inspect file size or WAV contents.  This cleanup is only reached on the
root's broader teardown path, so it owns resource-thread output that has not
yet been transferred to the consumer.

## Producer/Consumer Dependency

The primary consumer is the per-item worker `0x0043d390`, called by the
sound-root initialization loop at `0x0043cf60`:

```text
for (index = 0; index != 37; ++index) {
    if (InitializeOneSoundFromRawSlot(index, root, wave_names[index]) != 0)
        report the same load error and return failure;
}
```

The caller supplies a mixed ABI to `0x0043d390`:

```text
ECX = u32 index
EDX = root at 0x00492590
stack +0x04 = const char* resource_name
return EAX = 0 on success/no-op/cancel, -1 on a detected failure
ret 4
```

For an index, the worker first removes/release-clears an existing COM slot at
`root + 0x08 + 4 * index`.  It then polls `root + 0x5230 + 4 * index` until a
raw buffer is non-null, sleeping `10` ms per iteration.  If the shared gate
becomes `2` while waiting, it returns zero without consuming a raw buffer.

Once non-null, `0x0043d390` copies the raw pointer to a local and immediately
writes zero to the shared slot.  This is the ownership transfer point.  On
all later paths it, rather than the root teardown, releases the raw pointer
through `0x00452422`.

The worker verifies `RIFF` at byte zero and `WAVE` at byte eight, locates the
`fmt ` and `data` chunks through `0x0043d250`, creates/configures a
DirectSound buffer through the root's DirectSound interface, locks it, copies
the data chunk, unlocks it, and frees the raw buffer.  Failures in format
validation or DirectSound setup report a more specific error through
`0x0044b810`, free the transferred raw buffer, and return `-1`.  This proves
the resource-thread buffer is a complete source file image, not a PCM payload
or a retained archive handle.

The relationship is intentionally racy in the original program: publication,
polling, gate reads, and teardown use ordinary loads/stores rather than an
event, mutex, or interlocked operation.  A semantic C++ implementation may
use `volatile`-style shared state or a synchronization primitive, but changing
the shutdown ordering needs a separate behavioral decision.

## Gate Writers and Shutdown

### Full Root Sound Teardown: `0x0043d120`

This EAX/plain-`ret` entry is broader than raw producer-image cleanup. It
releases the BGM descriptor at `+0x1f84`; each of 128 BGM buffers at `+0x208`
and 128 sound buffers at `+0x008`; then the 37 raw images at `+0x5230`. When
the holder at `+0x610` is non-null, it kills timer 1 on `+0x60c`, destroys the
BGM stream, clears the root sound device, shuts down/releases `+0x608`,
deletes the transition control at `+0x5208`, releases/frees the holder, and
releases all 16 `+0x1ec0` track allocations.

Two observed paths write the stop value `2` to `0x004977b4`:

| Address | Context | Subsequent relevant action |
| --- | --- | --- |
| `0x00420284` | broader main-chain shutdown | continues process/global cleanup |
| `0x00438dda` | runtime shutdown path | calls `0x0043cc40`, then root teardown `0x0043d120` |

The latter sequence gives the required ownership ordering: request stop,
wait/close the relevant worker controls through `0x0043cc40`, then free raw
slots not already transferred to `0x0043d390`.  There is no evidence in this
entry that `0x004977ac` is joined or closed directly; do not invent that
responsibility for the resource-thread function itself.

`0x0043ccf0` is another dependent worker boundary: it calls sound-root
initialization, then uses the same gate's zero/nonzero convention before
setting `0x004977bc = 1`.  This completion word is not written by
`0x0043d080` and must not be treated as the resource preloader's completion
flag.

## Direct External Dependencies

| Address | ABI used here | Role |
| --- | --- | --- |
| `0x0044b360` | `EAX=name`, stack `(optional_out_size=0, source_mode=0)`, `ret 8` | Archive-mode loader; returns a caller-owned heap buffer or null. It holds its own shared resource-loader critical section. |
| `0x0044b810` | `ECX=0x00474f70`, stack format arguments, plain `ret` | Error-buffer append helper. The resource-thread failure call supplies format `0x46fdd4` and the failed name. |
| `KERNEL32!Sleep` (`0x004660ac`) | one stack `DWORD` | Called with `1` in the post-load gate loop; `0x0043d390` separately calls it with `10` while waiting for one slot. |
| `0x00452422` | one stack pointer | Game heap release used by the consumer and root teardown, not by the successful preload loop. |

The packed-loader's full ownership and lock behavior is recovered in
`packed-archive-resource-loader.md` and `version-data-initialization.md`.
The resource thread must not add a second free after `0x0044b360` succeeds:
until the consumer takes it, the raw-slot array or root teardown owns it.

## C++ Reconstruction Plan

Keep the fixed table and raw-slot storage explicit.  `std::vector`, automatic
buffers, and a generic future/promise abstraction would obscure both the
known slot layout and the transfer-to-null transition.

```cpp
enum { kMainChainWaveCount = 37 };

struct MainChainWavePreloadState {
    void *raw_wave_slots[kMainChainWaveCount]; // root + 0x5230
};

extern volatile long g_MainChainResourceGate;
extern const char *const g_MainChainWaveNames[kMainChainWaveCount];

void PreloadMainChainWaveResources(MainChainWavePreloadState *state) {
    for (unsigned long i = 0;
         i != kMainChainWaveCount && g_MainChainResourceGate != 2;
         ++i) {
        void *raw = LoadPackedResource(g_MainChainWaveNames[i], 0);
        state->raw_wave_slots[i] = raw;
        if (raw == 0) {
            AppendResourceLoadError(g_MainChainWaveNames[i]);
            return;
        }
    }

    while (g_MainChainResourceGate == 0)
        Sleep(1);
}
```

The shown `for` is semantic pseudocode: preserve the original early stop
before each load and do not run the post-loop wait after a failed load.  The
production implementation should also provide a separately named consumer
operation, such as `InitializeOneSoundFromRawSlot`, whose contract is:

1. Wait for a non-null slot unless cancellation state `2` is observed.
2. Move the pointer out by reading the slot and setting it to null before
   parsing it.
3. Own exactly one release of that raw allocation on every post-transfer path.
4. Publish the resulting DirectSound buffer in the root's distinct
   `+0x08 + 4 * index` slot, with its independent COM-release lifecycle.

For source-level safety, a modern implementation can replace polling with an
event/condition variable and use an atomic cancellation state.  Such a change
is not codegen-compatible and changes timing; retain the polling protocol in
the compatibility-oriented C++ layer until a broader thread-synchronization
decision is made.  Do not use the `CreateThread` argument as the resource
state merely because it points at the enclosing root: the original entry
proves it is unused.

## Verification Basis

* `resources/th10.exe`, disassembly `0x0043d080-0x0043d0ea`: preload loop,
  failure return, and post-load polling.
* `0x00474b40-0x00474bd3` and `0x0046fe80-0x004700bf`: 37 pointer entries
  and their exact ASCII filenames.
* `0x0043cf60-0x0043d03f` and `0x0043d390-0x0043d639`: sound-root consumer,
  raw-slot polling, transfer, WAV validation, DirectSound construction, and
  raw-buffer release.
* `0x0043d120-0x0043d249`: unconsumed raw-slot teardown.
* `0x00420270-0x00420394`, `0x00438dc0-0x00438df0`, and
  `0x0043ccf0-0x0043cd28`: observed gate writers and dependent worker flow.
* PE import table: `0x004660ac = KERNEL32!Sleep` and
  `0x00466118 = KERNEL32!CreateThread`.
