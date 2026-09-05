# Transition Sound Adapter Operations

This note covers the operational initialization and playback entries of the
descriptive `TransitionSoundAdapter` family in `resources/th10.exe`:
`0x0044d110`, `0x0044d440`, `0x0044d550`, and `0x0044d600`.  The class name is
descriptive, not a recovered original symbol.  It supplements the ownership
boundary in `transition-sound-lifecycle.md`; it does not change it.

## Native entry boundaries

None of the four requested entries is an ordinary C++ member-function ABI.

| Address | Proven input ABI | Stack cleanup | Result |
| --- | --- | --- | --- |
| `0x0044d110` | `EAX = adapter`, `[ESP+4] = IDirectSoundBuffer*`, `[ESP+8] = refill_mode` | `ret 8` | `HRESULT`; zero after a successful `Unlock` |
| `0x0044d440` | `EAX = adapter`, `[ESP+4] = play_priority`, `[ESP+8] = play_flags` | `ret 8` | selected buffer's `Play` HRESULT, or an earlier HRESULT |
| `0x0044d550` | `EDI = adapter` | plain `ret` | bitwise OR of every `Stop` and `SetCurrentPosition` HRESULT |
| `0x0044d600` | `EDI = adapter` | plain `ret` | bitwise OR of every `SetCurrentPosition` HRESULT |

`0x0044d110` saves the first stack argument in `EBP`; the apparent first
argument stack slot is subsequently reused as `IDirectSoundBuffer::Lock`'s
first byte-count output.  `refill_mode` remains available at the original
second stack slot and is tested at `0x44d24b`.

`0x0044d440` preserves incoming `ECX` on the stack, then passes that stack slot
to `0x0044d300` as an output flag.  `ECX` is therefore not an input argument.
The two explicit stack values are ultimately passed directly to
`IDirectSoundBuffer::Play` as priority and flags.

The adapter fields used here are established by construction evidence:

```cpp
struct TransitionSoundAdapterOperationalPrefix {
    void **vtable;                       // +0x00
    IDirectSoundBuffer **slots;          // +0x04
    unsigned char unknown_0008[0x04];
    void *stream_source;                 // +0x0c, consumed by 0x44de10
    unsigned long slot_count;            // +0x10
    unsigned long remaining_ticks;       // +0x14
    unsigned long duration_ticks;        // +0x18
    unsigned long transition_phase;      // +0x1c
    unsigned long play_priority;         // +0x20
    unsigned long play_flags;            // +0x24
    unsigned char unknown_0028[0x04];
    unsigned long field_002c;            // cleared by 0x44d440
    unsigned long playback_active;       // +0x30
};
```

`+0x08` is passed as the requested Lock byte count.  Its broader semantic name
is not established.  The `stream_source` name only describes its direct use by
the copy helper; it does not assert a particular source class.

## `0x0044d110`: restore, lock, populate, unlock

The routine configures one supplied `IDirectSoundBuffer`, not necessarily slot
zero.  It first calls `GetStatus(&status)` and propagates a failing HRESULT.
When `status & DSBSTATUS_BUFFERLOST` is set, it calls `Restore()` repeatedly
until it returns zero.  On exactly `DSERR_BUFFERLOST` (`0x88780096`) it calls
the imported delay routine with ten milliseconds before retrying.  Any other
nonzero `Restore` result retries immediately; the binary has no error escape
from that loop.

It then invokes `Lock` with the following exact argument shape:

```cpp
buffer->Lock(
    0,                         // dwOffset
    adapter->field_0008,       // dwBytes
    &audio_ptr_1,
    &audio_bytes_1,
    0,                         // audio_ptr_2
    0,                         // audio_bytes_2
    0);                        // dwFlags
```

The first returned region is filled through `0x0044de10`, with `EAX` set to
`adapter + 0x0c`, the locked pointer, and the locked byte count.  That helper
copies from the source and reports the copied byte count through an output
pointer.  A negative result bypasses `Unlock` and is propagated, so the native
error path leaves a successful lock outstanding.

For a successful first copy, the remaining region has two distinct behaviors.

* If no bytes were copied, it fills the entire locked region with silence.
* If fewer than the locked byte count were copied and `refill_mode != 0`, it
  calls `0x0044dd40` with `ESI = stream_source` and `AL = 0`, then repeatedly
  calls `0x0044de10` until the region is filled.  Any helper failure is
  returned without an `Unlock`.
* If fewer than the locked byte count were copied and `refill_mode == 0`, it
  fills only the remainder with silence.

The silence byte is `0x80` when `*(unsigned short *)(stream_source + 0x90 +
0x2e) == 8`; otherwise it is zero.  This is direct WAVEFORMAT tag evidence for
the customary unsigned 8-bit PCM silence value, but does not establish the
complete structure type of `stream_source + 0x90`.

On the successful path it calls:

```cpp
buffer->Unlock(audio_ptr_1, audio_bytes_1, 0, 0);
```

The `Unlock` HRESULT is discarded and the function returns `0`.  In
particular, this is not an HRESULT-preserving wrapper.

## `0x0044d440`: choose, recover, initialize, and play

This is the adapter's configuration/start operation.

1. If `adapter->slots == NULL`, return `CO_E_NOTINITIALIZED`
   (`0x800401f0`).
2. Call `0x0044d370` with `EDI = adapter` to choose a non-null slot.  A null
   result returns `E_FAIL` (`0x80004005`).  The selector first scans for a
   non-null slot whose `GetStatus` bit 0 is clear; if none is found, it uses a
   pseudo-random modulo `slot_count` fallback.  It is not a fixed slot-zero
   selection.
3. Call `0x0044d300` with `ESI = selected_buffer` and an output flag.  That
   helper calls `GetStatus`; if the buffer-lost bit is set, it runs the same
   `Restore` retry loop and writes one to the output flag after recovery.  Its
   normal non-lost success return is `1`; recovery success returns `0`.
4. If recovery occurred, call `0x0044d110(adapter, selected_buffer, 0)` and
   fail immediately on a negative HRESULT.  Then call `0x0044d600` to reset
   every slot to byte position zero.  The return value from `0x0044d600` is
   ignored.
5. Clear `+0x1c`, `+0x14`, and `+0x18`, call `0x0044d4e0(adapter, 0)` to set
   slot zero's volume, set `+0x30 = 1`, store the two input dwords at `+0x20`
   and `+0x24`, and clear `+0x2c`.
6. Return the direct result of:

```cpp
selected_buffer->Play(0, adapter->play_priority, adapter->play_flags);
```

No result from the initial volume operation is checked.  State fields are
committed before `Play`, including when `Play` subsequently fails.

The direct caller at `0x0043da80` uses `(priority = 0, flags = 1)`, consistent
with `DSBPLAY_LOOPING`; other callers pass a dynamic second argument, so that
meaning should remain an API-level type rather than a hardcoded behavior name.

## `0x0044d550` and `0x0044d600`: bulk reset paths

Both entries first test only `adapter->slots`, returning `CO_E_NOTINITIALIZED`
when it is null.  A non-null slots array with a null element still faults when
the element is dereferenced.

`0x0044d550` clears `playback_active` before any per-slot call.  For every
unsigned index below `slot_count`, it calls `Stop()`, then
`SetCurrentPosition(0)`, and bitwise-ORs the HRESULT values.  After the loop
it clears `transition_phase`, then returns the OR accumulator.  Thus it
attempts every operation even after an earlier failure; the return is not the
first failure and is not guaranteed to be a canonical HRESULT.

`0x0044d600` uses the same unsigned loop and accumulator but calls only
`SetCurrentPosition(0)`.  It does not write adapter state fields.

## DirectSound vtable proof

The calls use the standard x86 COM/stdcall shape: the receiver is pushed as
the final stack argument, and each method removes all its arguments.  The
following slots are directly present in the requested routines or their
immediate operational helpers.

| Vtable offset | Index | Method | Direct use |
| --- | ---: | --- | --- |
| `+0x24` | 9 | `GetStatus(DWORD*)` | `0x44d110`, `0x44d300`, and the slot selector |
| `+0x2c` | 11 | `Lock(DWORD,DWORD,void**,DWORD*,void**,DWORD*,DWORD)` | `0x44d110` |
| `+0x30` | 12 | `Play(DWORD,DWORD,DWORD)` | `0x44d440` |
| `+0x34` | 13 | `SetCurrentPosition(DWORD)` | `0x44d550`, `0x44d600` |
| `+0x3c` | 15 | `SetVolume(LONG)` | `0x44d440` via `0x44d4e0` |
| `+0x48` | 18 | `Stop()` | `0x44d550` |
| `+0x4c` | 19 | `Unlock(void*,DWORD,void*,DWORD)` | `0x44d110` |
| `+0x50` | 20 | `Restore()` | `0x44d110`, `0x44d300` |

The `IDirectSoundBuffer` method names and offsets identify the interface
prefix.  They do not prove the concrete implementation type or that an
adapter owns all buffers passed to `0x0044d110`.

## C++ recovery boundary

A readable C++ layer should make the operational distinction explicit:

```cpp
HRESULT InitializeBuffer(TransitionSoundAdapter *adapter,
                         IDirectSoundBuffer *buffer,
                         bool refill_until_full);
HRESULT StartAdapter(TransitionSoundAdapter *adapter,
                     DWORD priority,
                     DWORD flags);
HRESULT StopAndRewindAll(TransitionSoundAdapter *adapter);
HRESULT RewindAll(TransitionSoundAdapter *adapter);
```

These are semantic signatures only.  Native-compatible exports need separate
x86 thunks for the `EAX` and `EDI` receivers documented above.  Do not model
the `0x0044d550` return as conventional HRESULT aggregation, or turn the
`0x0044d110` error paths into RAII cleanup: both would change observable
behavior.

## Verification basis

* `resources/th10.exe`, disassembly `0x0044d110-0x0044d2f0`,
  `0x0044d300-0x0044d3d6`, and `0x0044d440-0x0044d63c`.
* Direct callers at `0x0043da80`, `0x0043dab0`, `0x0043ddf0`,
  `0x0044ce80`, `0x0044d6a0`, `0x0044d8b0`, and `0x0044db00`.
* `IDirectSoundBuffer` vtable ordering, cross-checked against the constructor,
  transition tick, and destructor uses documented in the adjacent transition
  sound evidence notes.
