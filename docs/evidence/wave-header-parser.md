# TH10 0x0043d390 — WAV Image Consumer / RIFF Header Parser

Semantic body: already reconstructed as `ConsumeMainChainWaveImage` in
`src/MainChainSoundLoading.cpp` (with helper `ParseWaveNormalInput` /
`FindWaveChunkNormalInput`); this document records the analysis that confirms
the match. One native caller loop (`0x0043cf60`) is reconstructed as
`ConsumeAllMainChainWaveImages` in the same file.

## Behavior (native EAX=index? ECX=root, EDX=index-register split)

The native entry receives the transition/sound root (DAT_00492590) and a wave
index; decompiler shows `(int a1, int *a2, char a3)` where `a2` is the root
and `a1` the slot index.

1. If `root->sound_build_gate` (`+0x610`, `a2[388]`) is zero, return 0.
2. If `root->sound_buffers[index]` (`a2[a1+2]`, `+0x208`) is non-null, call
   vtable `+0x08` (IUnknown::Release) on it and clear the slot.
3. Spin until `root->raw_wave_images[index]` (`a2[a1+5260]`, `+0x5230`) is
   non-null, sleeping 10 ms per iteration; abort with 0 when the resource
   gate `DAT_004977b4` equals 2 (shutdown).
4. Take the transferred image pointer, clear the slot; null yields -1.
5. RIFF validation: `strncmp(image, "RIFF", 4)` and
   `strncmp(image+8, "WAVE", 4)`; failure logs `"Wav "` via `0x0044b810`
   (`AppendSoundFileLoadError` family) and returns -1 after `free`.
6. Chunk scan from `image+12` via `0x0043d250`: locate the `fmt ` chunk and
   the `data` chunk. The native scan advances by `8 + chunk_size` without a
   remaining-length bound (see helper notes); a missing chunk logs `"Wav "`
   and returns -1.
7. Copies the 18-byte `WAVEFORMATEX` (16-byte body plus the u16 extra size at
   chunk+16) into a stack `WAVEFORMATEX`, builds a `DSBUFFERDESC` with
   `dwSize=0x24`... native sets `v21[0]=36` (0x24), `dwFlags=0x80c8` (32968,
   DSBCAPS_CTRLDEFAULT-ish combination), `dwBufferBytes=data chunk size`
   (`v18`), `lpwfxFormat=&format`, remaining fields zero.
8. Creates the buffer with vtable `+0x0c` of the DirectSound object held at
   `root[0]` (IDirectSound::CreateSoundBuffer), storing the result into
   `root->sound_buffers[index]`; negative HRESULT frees the image and returns
   -1.
9. Locks the buffer with vtable `+0x2c` (offset 0, data chunk size, flags 0)
   to obtain two write regions, copies the `data` chunk payload into region 1
   and, when the second region length is non-zero, the payload tail into
   region 2, then unlocks via vtable `+0x4c` and frees the image. Returns 0.

## Match against the existing reconstruction

`ConsumeMainChainWaveImage` + `BuildSoundBufferFromTransferredWaveImage`
reproduce steps 1-9 in order: gate check, release/clear, 10 ms spin with
`g_MainChainResourceGate == 2` abort, `strncmp`-equivalent little-endian tag
checks ("RIFF"/"WAVE" as u32 compares), unbounded-but-normal-input chunk scan
modeled by `FindWaveChunkNormalInput` (deliberately not hardened for hostile
data, per the in-file comment), 0x24-byte description with flags 0x80c8,
`CreateDirectSoundBuffer` (+0x0c), `LockDirectSoundBuffer` (+0x2c), both
region copies, `UnlockDirectSoundBuffer` (+0x4c), and image release through
the game heap free `0x00452422`. The native ECX/EDX/stack register split is
left to a thunk; the semantic form keeps `resource_name` unused explicitly
for that boundary.

CSV row appended: `0x0043d390,ConsumeMainChainWaveImage,MainChainSoundLoading`.
