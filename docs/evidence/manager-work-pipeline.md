# Manager work pipeline (0x004472e0-0x00448360)

Module: `src/ManagerWorkPipeline.cpp`

Work contexts live at owner+0x3ad06c+4i (33 slots); each holds its slot
table at +0x120 (16-byte `{data, size, ...}` entries) and the image
material table at +0x118 (0x44-byte descriptors).

- `0x004472e0`: validates the record kind (must be 4), skips records whose
  name byte is 0x40, loads the leaf and stores data/size into the slot
  table; errors print through the 0x0044b8e0 console printer.
- `0x004473c0`: walks the record chain (stride at +0x38 of each record,
  offsets accumulate words 0/1) running the registered
  `ProcessSelectedManagerWorkStage` on the boundary record and advancing
  the +0x124 cursor.
- `0x00447700`: sweeps the 33 contexts: works marked +0x128 are released
  (`ReleaseManagerWorkContents` + free), marked ones are advanced.
- `0x00447940`: rep-movs the 0x44-byte descriptor and derives the
  normalized floats (+0x20/+0x24/+0x28/+0x2c ratios, +0x30/+0x34 extents
  over source words 14/15).
- `0x00447ec0`: releases the cached surface pair (0x00447f70, registered)
  and records data/size at +0x3ad640/+0x3ad6c0.
- `0x00448360`: `GetSurfaceLevel` on both slot textures and
  `D3DXLoadSurfaceFromSurface` (the decompiler's stack artifact for the
  trailing D3DX arguments is normalized to the documented 7-param ABI).
