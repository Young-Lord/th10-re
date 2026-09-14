# Screenshot writer, HUD owner lifecycle and menu helpers

Reconstructions in `src/ScreenshotWriter.cpp`, `src/AsciiHudOwnerLifecycle.cpp`,
`src/OptionTrailUpdate.cpp`, `src/MenuItemObject.cpp`. All verified against
fresh IDA decompiles/disassembly (session `th10re`).

## 0x00420670 CaptureMainChainSnapshot (userpurge: EAX path, ESI context)

Spins on context+0x50c with `Sleep(10)` until the shared scratch block is
idle, then obtains the back-buffer wrapper via device vtable slot 0x48/4
(device at context+0x8). Builds a BITMAPFILEHEADER inside the context at
+0x510: 'BM', size 54 (dword +0x512), data offset 54 (dword +0x51a), and
copies the output path (with terminator) to context+0x528.

Render-mode gate at +0xec:
- 22 (capture): malloc(0x2c) BITMAPINFOHEADER to +0x520 (biSize 40, 640x480,
  planes 1, 24bpp, all zeroed first), malloc(0xE1000) pixel buffer to
  +0x524; failure of either logs `"snapShotScreen : "`. On success the file
  size field +0x512 grows by 0xE1000, the surface is locked through vtable
  slot 0x34/4 (descriptor out-param is {pitch, bits}), 480 rows of 640
  pixels are copied bottom-up from 32-bit to 24-bit, unlocked via 0x38/4,
  and the writer thread is spawned with `_beginthread(0x420540, 0, 0)`
  (handle into DAT_00492134).
- 23: logs `"16bit "` only.
- else: logs `"error ? .\\src\\game\\mother.cpp\r\n"` and returns 1.

The surface wrapper is always released through vtable slot 2; the snapshot
path returns 0. `PushTextLogMessage` models the thiscall into the 0x474F70
Text manager (0x44B810 boundary).

## 0x00420540 SnapshotFileWriterThread

Calls the 0x44B620 boundary (opens the file into DAT_00474038 under the
0x4922A4 critical section, bumping the 0x49231E depth), then writes the
14-byte file header (0x492138), the 0x28-byte info header (0x492148) and
the 0xE1000-byte pixel block (0x49214C). Every short write closes the
handle, leaves the critical section and decrements the depth; the final
close does the same. Both buffers are freed (0x492148/0x49214C cleared)
and the thread handle 0x492134 cleared. `goto finish` structure preserved.

## 0x00413810 ConstructAsciiHudOwnerRecords

EH-scoped constructor of the 0x9ED0-byte HUD owner record:
1. Six `eh vector constructor iterator` calls (0x45252d) over 0x3AC-byte
   VM-record arrays at +0x10/+0x24c8/+0x4980/+0x6a8c/+0x793c/+0x8094 with
   counts 10/10/9/4/2/7, ctor 0x402050, dtor 0x401ff0
   (DestroyTitleScreenVmRecordInPlace).
2. The nine ready flags of the +0x9a48 block are cleared; the first one is
   loaded from +0x9ab4 and stored back through +0x9a48+0x6c (same address).
3. The +0x9a48 block is wiped (0x3ac bytes, `rep stosd` 0xEB dwords) and
   the -1 word is written at +0x9dcc — overwriting the flag stores.
4. `+0x9e70 &= ~1`, then the whole 0x9ED0 record is wiped — erasing the
   arrays and block above (native order preserved; the observable result
   is a zeroed record with word 2 at +0, published into DAT_004770C).

## 0x00414370 ReleaseAsciiHudOwnerResources (stdcall)

- `(mode_flags_0x474CD0 & 9) != 0` (replay/demo): releases every entity
  using the tracked resource id via ReleaseEntitiesUsingResourceEaxEdxAbi
  (owner DAT_00491C40). Otherwise: slot owner+0x3acb9c released through
  ReleaseLargeRenderOwnerSlotEdiAbi and freed.
- The +0x9e80 resource id is cleared; the +0x9eb8 glyph buffer goes
  through the 0x4136C0 boundary then free; the +0x9ebc buffer is freed
  and DAT_00491C18 mirrors it (replay/demo keeps the pointer, normal mode
  frees and clears).
- Parent at +8 gets `+4 &= ~2`.
- Entity ids +0x9e54/+0x9e58/+0x9e64 soft-released via ReleaseEntityById
  (0x4492a0) and cleared; dwords +0x9e70..+0x9e9c zeroed.
- Eight tracked-entity slots at +0x9e34: each id is searched in
  owner+0x72d674 then owner+0x72d67c (nodes {entity, next}, compared key
  is the entity's +0 id). A found entity gets `+0x35c |= 0x4000000`, and
  while entity+0x18 == 0 every +0x14 child gets the same flag. Slots are
  cleared; +0x9e90 is zeroed last. (Distinct list heads from the
  0x72dad4/0x72dadc pair used by ReleaseEntityById — hence the inline
  walk.)

## 0x00427960 UpdateOptionTrailHistoryEsiAbi (ESI)

Player option root DAT_00477864: slot table at +0x43cc (64 bytes per
option, index = record+0x88), trail history at +0x4370 (8-byte entries,
8 per option), live flag at +0x4474, current position deltas at +0x3cc/
+0x3d0. Reads the slot pair into record +0x34/+0x38. Leading mode
(flag != 0) publishes record +0x44/+0x48 plus the root deltas into the
slot head and zero-fills the seven following trail entries (the native
routes both stores through `_ftol2` of the x87 register file, which holds
0.0 on every reachable path); trailing mode recomputes +0x44/+0x48 as
table - root delta. Both tails re-add the root delta into +0x34/+0x38 and
store the flag into record +0x84.

## 0x004386B0 MenuItemStringAssign (thiscall)

MSVC std::string growth for the menu item records (data union at +4,
size +0x14, capacity +0x18): `capacity = new|0xF`; when that is not
0xFFFFFFFF and `capacity/3 < old_capacity>>1` (with the native overflow
guard against `old + old/2`), the capacity grows to `old + old>>1`.
`operator new(capacity+1)`, then exactly `copy_size` bytes are memcpy'd
from the previous buffer (SSO bytes when capacity < 0x10) — unchecked
overread when copy_size exceeds the old length, preserved. Old heap
buffers are freed, size/capacity/data are stored and the terminator is
written at `data[copy_size]`.
