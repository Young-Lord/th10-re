# Stage texture loading (0x00436d30-0x00449700)

Module: `src/TextureStageLoading.cpp`

- `0x00436d30`/`0x00446500`: identical pixel-hit accumulators — add the
  4-byte pixel triple into a 3-dword accumulator and bump the counter when
  the weight byte (pixel[3]) is non-zero.
- `0x00446b70 CreateStageTextureFromFileEcxAbi`: ECX = format index,
  ESI = texture record, stack = size word / path / color key. When the
  `DAT_00491d78` bit 0 low-fidelity gate is set, formats 21/0 downgrade to
  selector 5 and 20 to 3; `D3DXCreateTextureFromFileInMemoryEx` (pool 1,
  filter 3, mip filter -1), then the transparent-pixel dilation pass
  (0x004465b0, registered) and the bpp record from `DAT_0046f2a0`.
- `0x00446c70 CreateStageTextureFromImageEaxAbi`: `GetSurfaceLevel`
  (vtable +0x48) then either `D3DXLoadSurfaceFromMemory` with the embedded
  header (u32 at image+0x30 offsets a `{u16 x3, u16 format, u16 width,
  u16 height, pixels}` header) or `D3DXLoadSurfaceFromFileInMemory`;
  releases the surface, dilates, records the bpp.
- `0x00449700 ClearStageTextureSurfaceEaxAbi`: level-0 `GetDesc` /
  `LockRect` / memset(0) / unlock and releases.
