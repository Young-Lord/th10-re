# Deferred Render Batches (`0x00448270`, `0x00448450`)

Both entries are mixed-register native boundaries, but their C++ semantic
inputs are explicit fields in the large render owner.

`0x00448270` resolves a texture through
`owner + 0x3ad06c + group * 4`, then that object's `+0x120` slot array with a
16-byte stride. A null texture prevents the vertex flush. After the flush it
requires `GetBackBuffer(0, 0, 0, &back_buffer) == 0`, re-resolves the texture,
then requires `texture->GetSurfaceLevel(0, &surface) == 0`. It constructs
source and destination RECTs from `{x, y, width, height}` groups at owner
`+0x08` and `+0x18`, calls D3DX with filter `2`, ignores that HRESULT, and
releases the temporary level-zero surface and the back buffer.

`0x00448450` treats owner `+0x2c` and `+0x3c` as source/destination
`{x, y, width, height}` groups. After a successful back-buffer acquisition it
writes the destination width and height to `+0x3ad6e0 + slot * 0x1c` and
`+0x3ad6e4 + slot * 0x1c`. It creates a lockable render target using
`DAT_00491d14`, falls back to a pool-3 offscreen surface only on nonzero
HRESULT, then creates a pool-3 shadow surface. The first D3DX copy uses filter
`0xffffffff` and must return exactly zero before the second full-surface copy;
the latter result is ignored. All later failures preserve descriptor and any
already-published surfaces while releasing the acquired back buffer.

The cached descriptor has a 0x1c-byte stride. The currently proven fields are
only width and height at offsets zero and four. `0x00447fd0` and `0x00448120`
consume those fields to lazily rebuild a missing primary from the system-memory
shadow, first attempting a lockable render target and then pool 3.
`0x00447fd0` maps four stack scalars to `RECT.left`, `RECT.top`, `POINT.x`,
and `POINT.y`, writing descriptor width/height directly into `RECT.right/bottom`.
`0x00448120` uses its six coordinate scalars as a normal
`{destination x/y, source x/y/width/height}` update rectangle.
