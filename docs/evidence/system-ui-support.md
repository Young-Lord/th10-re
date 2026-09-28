# System/UI support (0x00439660-0x0043bc40)

Module: `src/SystemUiSupport.cpp`

- `0x00439660 GetMainChainFrameTimeUnlocked`: lock-free twin of the
  registered 0x00439540 frame clock — `QueryPerformanceCounter` over
  `DAT_00492508/492510` with the `DAT_00492540` epoch clamp, and the
  `timeGetTime` fallback.
- `0x0043a3a0 ReplayOverwriteDialogProc`: WM_INITDIALOG (272) checks
  dialog item 202; WM_COMMAND (273) IDs 201 (result 6) and 203 (result 7)
  fold the checkbox state into the 0x100 bit of `DAT_00491d78`.
- `0x0043a5c0` MIDI-out device-name query (`midiOutGetDevCapsA`, 0x34
  bytes, `szPname` copy); `0x0043a6f0`/`0x0043a700` are the 16/32-bit byte
  swaps used by the big-endian MIDI readers.
- `0x0043ba90 RefreshMainChainInputAxes`: clears bits 0x200/0x400 of
  `DAT_00491ff4`, re-runs `InitializeMainChainInput` and re-merges the
  axis bytes `DAT_00491c38/491c3c` into those bits.
- `0x0043bc40 SubmitReplayCameraFrameEsiAbi`: `Clear` (vtable +0xac,
  flags 3, color = the ESI word, z 1.0) and `Present` (vtable +0x44); on
  failure the presentation parameters `DAT_00491d0c` are re-set (vtable
  +0x40) and the pass repeats.
