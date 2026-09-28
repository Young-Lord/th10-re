# BGM support misc (0x0043cc30, 0x0043d250)

Module: `src/BgmSupportMisc.cpp`

- `0x0043cc30 RequestBgmWorkerStopEaxAbi`: sets context+0x5224 to 2 — the
  stop request sibling of the registered `StopBgmWorkerControls`
  (0x0043cc40).
- `0x0043d250 FindBgmChunkByTagEcxAbi`: walks the BGM data file's
  `{char tag[4]; u32 size; u8 data[size]}` records until the 4-byte tag
  matches (strncmp), returning the payload pointer and storing the size.
  Native quirk: the size out is written before the tag comparison, so a
  final miss leaves the last record's size behind. The walk requires the
  remaining byte count to reach exactly zero.
