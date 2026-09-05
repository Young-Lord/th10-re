# Main-Chain Resource Thread Implementation (`0x0043d080`, `0x0043d390`)

## Scope

This note turns the already established producer/consumer facts for the
main-chain WAV preload path into an implementable C++ boundary.  It covers
the normal 37-item resource loop at `0x0043d080`, the per-slot consumer at
`0x0043d390`, and the caller loop at `0x0043cf60`.  It does not rename the
enclosing object, recover the original source class names, or claim ordinary
C++ ABIs for entries that demonstrably use registers.

The target is `resources/th10.exe`.  All names below are descriptive.

## Shared Object Boundary

The relevant portions of the large root at `0x00492590` are directly proven:

```text
root +0x0000  IDirectSound* sound_device
root +0x0008  IDirectSoundBuffer* sound_buffers[37]
root +0x0610  non-null gate for per-slot DirectSound construction
root +0x5230  unsigned char* raw_wave_images[37]
```

The two arrays have matching index spaces.  Their absolute aliases are:

```text
0x004977c0  root +0x5230, raw_wave_images[0]
0x004977b4  u32 resource_gate
0x00474b40  const char* wave_names[37]
```

`resource_gate == 2` is the observed cancellation value.  A zero value has a
different meaning at each waiting point: the producer waits for it to become
nonzero only after it has loaded all files, while the consumer does not wait
on zero itself and instead waits for its individual raw-image slot to become
non-null.

## Native ABIs

| Entry | Inputs | Stack cleanup | Result |
| --- | --- | --- | --- |
| `0x0043d080` | no observed input; ignores the `CreateThread` argument | plain `ret` | EAX is not explicitly defined on all returns |
| `0x0043cf60` caller loop | `ESI = index`, enclosing root in its preserved `ESI` before the loop | caller returns `ret 4` | zero after all slots; `-1` for cancellation or per-slot failure |
| `0x0043d390` | `ECX = index`, `EDX = root`, `[ESP+4] = resource_name` | `ret 4` | `0` success/no-op/cancel; `-1` parse or DirectSound failure |
| `0x0043d250` chunk finder | `EAX = initial scan count`, `ECX = first chunk`, `[ESP+4] = four-byte chunk name` | `ret 4` | matching chunk payload pointer, or null before its first scan |

For C++ source, use normal functions or methods such as
`PreloadWaveImages` and `ConsumeWaveImage`.  An eventual binary-oriented
build needs narrow thunks for the original entries rather than a false
`__thiscall` declaration for `0x0043d390`.

## Producer: Exact Normal Resource Loop

`0x0043d080` starts at index zero, checks the gate before every resource
load, and calls `0x0044b360` as follows:

```text
EAX = wave_names[index]
stack +04 = 0                  // optional output size
stack +08 = 0                  // packed-archive mode
```

The returned complete file image is immediately stored in
`raw_wave_images[index]`.  A null return is stored too, then an error is
appended and the thread returns.  It never frees earlier successful slots.
After index 36 succeeds, it sleeps for one millisecond while the gate is
exactly zero; any nonzero value exits that final wait.

```cpp
enum { kMainChainWaveCount = 37 };

void PreloadWaveImages(MainChainSoundRoot& root) {
    for (unsigned long index = 0;
         index != kMainChainWaveCount && g_resource_gate != 2;
         ++index) {
        unsigned char* image = LoadPackedResource(g_wave_names[index], 0, 0);
        root.raw_wave_images[index] = image;
        if (image == 0) {
            AppendSoundFileLoadError(g_wave_names[index]);
            return;
        }
    }

    while (g_resource_gate == 0)
        Sleep(1);
}
```

The shown function preserves normal target timing and ownership.  A Win32
thread adapter may return a fixed `DWORD` after calling it, because the target
entry itself has a plain `ret` despite being passed to `CreateThread`.

## Consumer Caller Loop

The surrounding initializer reaches `0x0043cf60` only after other sound-root
setup.  It compares the gate to two before each item.  It invokes the mixed
ABI consumer for indices `0..36` in ascending order.  A nonzero consumer
result appends the generic sound-file error with that index's filename and
returns `-1`; cancellation at the outer check also returns `-1`.  After all
37 consumer calls return zero, it appends the fixed DirectSound-success
message and returns zero.

The consumer itself returns zero on cancellation, so cancellation observed
inside `0x0043d390` does not cause a generic per-file error.  The next outer
gate test then produces the initializer's `-1` return.

```cpp
int ConsumeAllPreloadedWaveImages(MainChainSoundRoot& root) {
    for (unsigned long index = 0; index != kMainChainWaveCount; ++index) {
        if (g_resource_gate == 2)
            return -1;
        if (ConsumeWaveImage(root, index, g_wave_names[index]) != 0) {
            AppendSoundFileLoadError(g_wave_names[index]);
            return -1;
        }
    }
    AppendDirectSoundReadyMessage();
    return 0;
}
```

## Per-Slot Consumer State Machine

`0x0043d390` first reads `root +0x610`.  If it is null, it returns zero
without releasing an existing `sound_buffers[index]` slot and without polling
the raw-image slot.  This field is therefore an established prerequisite for
the conversion path, but its full object type is not recovered.

When it is non-null, the operation is ordered exactly as follows:

1. If `sound_buffers[index]` is non-null, call its COM `Release` at vtable
   offset `+0x08`, then clear the slot unconditionally.  The returned
   reference count is ignored.
2. Poll `raw_wave_images[index]`.  On a null read, sleep 10 milliseconds,
   test `g_resource_gate == 2`, return zero on cancellation, otherwise poll
   again.  The cancellation test precedes the next raw-slot read.
3. Read the non-null raw pointer to a local and immediately clear the shared
   raw slot.  This is the sole ownership-transfer point.
4. Validate and parse the file image, create the DirectSound buffer, lock and
   copy PCM bytes, unlock, then release the local raw image.
5. Return zero.  Every post-transfer failure also releases the local raw
   image and returns `-1`.

The raw slot remains untouched on the cancellation path because transfer has
not happened.  Root teardown at `0x0043d120` owns that still-published image.
No event, mutex, or interlocked operation guards this protocol in the target.

## WAV Validation and Chunk Parsing

The consumer verifies four bytes at raw-image offset zero against `RIFF`, and
four bytes at offset eight against `WAVE`, through the target byte comparison
helper.  It then constructs this normal-path view:

```text
riff_size     = *(u32*)(image + 0x04)
first_chunk   = image + 0x0c
scan_argument = riff_size - 0x0c       // literal unsigned subtraction
fmt_payload   = FindChunk(scan_argument, first_chunk, "fmt ")
data_payload  = FindChunk(scan_argument, first_chunk, "data")
```

On a normal file, the first 18 bytes of `fmt_payload` are copied into a local
`WAVEFORMATEX`-sized object.  The data byte count comes from the dword directly
before `data_payload` (`*(u32*)(data_payload - 4)`).  The function does not
use the archive loader's optional output size to bound any of these reads.

`0x0043d250` has an important malformed-input limitation.  Its initial
`EAX` size only gates entry to the first iteration.  After a nonmatching
chunk it advances by `8 + chunk_size` and branches based on whether that
increment is zero, rather than retesting a decremented remaining byte count.
Thus normal RIFF chunk ordering is assumed; a controlled C++ parser should
retain a real remaining-size bound, but must not call that hardening an exact
match to the native malformed-file behavior.

The native function reports `-1` and releases the transferred image for a
bad RIFF tag, bad WAVE tag, or a missing `fmt ` or `data` chunk.  It does not
validate that either payload actually fits the allocation, nor that the fmt
chunk is at least 18 bytes before copying it.

## DirectSound Construction and Copy

After both chunks are found, `0x0043d390` calls the vtable `+0x0c` method of
`root->sound_device`, the x86 `IDirectSound::CreateSoundBuffer` slot.  It
initializes a 36-byte `DSBUFFERDESC` equivalent with these proven values:

```cpp
DSBUFFERDESC description;
ZeroMemory(&description, sizeof(description));
description.dwSize = 0x24;
description.dwFlags = 0x80c8;
description.dwBufferBytes = *(unsigned long*)(data_payload - 4);
description.lpwfxFormat = &copied_wave_format;
// description.guid3DAlgorithm remains all-zero.
```

It invokes:

```cpp
HRESULT create_result = root.sound_device->CreateSoundBuffer(
    &description, &root.sound_buffers[index], 0);
```

Only a negative HRESULT is a failure.  On that path the raw image is freed,
but the target does not release or clear a non-null output buffer that a
nonconforming implementation might have published.  The normal C++ boundary
may clean such an output only if it intentionally chooses safer behavior over
the exact target failure contract.

For a nonnegative result, the newly published buffer is locked with:

```cpp
buffer->Lock(0, description.dwBufferBytes,
             &region1, &region1_bytes,
             &region2, &region2_bytes,
             0);
```

Again, only a negative HRESULT is failure.  On success, the target copies
`region1_bytes` from `data_payload`, then, when `region2_bytes != 0`, copies
that many bytes from `data_payload + region1_bytes` to `region2`.  It does
not compare the combined lock regions with the WAV data length.  Finally it
calls:

```cpp
buffer->Unlock(region1, region1_bytes, region2, region2_bytes);
```

The `Unlock` HRESULT is discarded.  It releases the transferred raw image
after both the successful and failed `Lock` paths.  A failed `Lock` therefore
leaves the successfully created buffer in its slot while returning `-1`.

## C++ Semantic Boundary

The following source-level shape preserves the normal ownership protocol
without pretending that it is the original register ABI:

```cpp
int ConsumeWaveImage(MainChainSoundRoot& root,
                     unsigned long index,
                     const char* resource_name) {
    if (root.field_0610 == 0)
        return 0;

    ReleaseAndClear(root.sound_buffers[index]);

    while (root.raw_wave_images[index] == 0) {
        Sleep(10);
        if (g_resource_gate == 2)
            return 0;
    }

    unsigned char* const image = root.raw_wave_images[index];
    root.raw_wave_images[index] = 0;

    // Parse the complete RIFF/WAVE file image, construct the DS buffer,
    // copy the data chunk through Lock/Unlock, and release image once.
    return BuildSoundBufferFromTransferredWaveImage(root, index,
                                                     resource_name, image);
}
```

`BuildSoundBufferFromTransferredWaveImage` must consume exactly one raw-image
ownership on every path after transfer.  It should keep malformed-input
validation in a separate policy decision: the normal resource loop only needs
the normal RIFF/WAVE behavior above, while strict length checks change known
native edge behavior.

## Ownership and Failure Summary

| Stage | Owner of raw image | Native result and persistent slot state |
| --- | --- | --- |
| Loader returns non-null | producer until consumer transfer | raw slot is non-null |
| Consumer cancellation while slot null | root teardown | raw slot remains null at that instant; a later producer publication is still root-owned |
| Consumer transfer | consumer local | raw slot is cleared before parsing |
| Parse failure | consumer releases image | sound slot was already released/cleared |
| CreateSoundBuffer failure | consumer releases image | target does not clean a possible non-null output slot |
| Lock failure | consumer releases image | created sound slot remains published |
| Successful unlock | consumer releases image | initialized sound slot remains published |
| Root teardown before transfer | root teardown | frees and clears every non-null raw slot |

The producer can still be inside `0x0044b360` when cancellation is requested,
so a teardown implementation must join or otherwise serialize the producer
before reclaiming raw slots.  That coordination is outside either requested
entry, but the existing shutdown path orders cancellation, worker control,
and raw-slot teardown separately.

## Verification Basis

* `resources/th10.exe`, disassembly `0x0043d080-0x0043d0ea`: fixed producer
  loop, resource publication, error path, and final one-millisecond wait.
* `0x0043cf60-0x0043d03f`: ordered 37-slot consumer caller loop and its
  distinct cancellation/per-item failure behavior.
* `0x0043d390-0x0043d639`: prerequisite field, COM release, polling,
  ownership transfer, RIFF/WAVE checks, chunk lookup, DS buffer construction,
  lock/copy/unlock, and raw-image release.
* `0x0043d250-0x0043d294`: native chunk-finder ABI and scan behavior.
* `0x0043d120-0x0043d249`: root teardown of untransferred raw slots.
* `0x0044b360` and `0x00452422`: caller-owned packed-resource allocation and
  matching game-heap release route, recovered in the archive loader evidence.
