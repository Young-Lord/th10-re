# Title-Screen State Destructor (TH10 0x00402440)

Implemented as `DestroyTitleScreenStateBufferInPlace` in
`src/TitleGameManagerLifecycle.cpp` (previously an extern boundary; the
0x417c80 teardown path calls the semantic body directly now).

## ABI

- Native stdcall, one stack argument (`ret 4`): the 0x2a78-byte title-screen
  state object whose pointer lives in `DAT_004776e4` (primary) /
  `DAT_004776e8` (secondary).

## Body, in native order

1. Scheduler records at `+0x08`, `+0x0c`, `+0x2a40`: each non-null record is
   removed via `CallbackSchedulerApi::Remove` (0x449f60) bracketed by the
   scheduler critical section (`DAT_00492274`) and the activity-depth byte
   (`DAT_0049231c`). The record slots themselves are intentionally not
   cleared.
2. Scratch buffers at `+0x10`, `+0x2a44`, `+0x17c`: each non-null pointer is
   released with the CRT `free` (0x452422) and cleared. While testing
   `+0x17c`, the native clears `+0x2a44` a second time; preserved.
3. Render-owner large slot: when mode flag `dword_474ca0` bit 0 is clear,
   index `((object+0x2a30) & 1) + 4` is bounds-checked against `0x21` (and
   the `+4` addition is sign-checked), and the pointer at
   `g_MainChainRenderOwner + index*4 + 0x3ad06c` is released through
   `ReleaseLargeRenderOwnerSlot` (0x447810) plus the main-chain delete
   (0x4524a1), then cleared.
4. The primary/secondary state globals (`DAT_004776e8` checked first, then
   `DAT_004776e4`) are cleared when they still point at this object.
5. Two eh vector destructor iterator calls, inlined semantically: 3 records
   of 0x3ac bytes at `+0x1f08` and 8 records at `+0x180`, each destroyed by
   the scalar record dtor `0x00401ff0` (boundary
   `DestroyTitleScreenVmRecordInPlace`, native `__thiscall` ECX = record).

## Status

Baselines pass (`scripts/compile-main-chain-cpp.sh`, g++ -m32 -std=c++98
syntax check, `git diff --check`). CSV row appended.
