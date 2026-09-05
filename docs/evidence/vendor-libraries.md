# TH10 Vendor Libraries And Toolchain Identification

Module: analysis only — no rebuilt code. Applies to `resources/th10.exe`
(sha256 `2f14760b6fbbf57549541583283badb9a19a4222b90f0a146d5aa17f01dc9040`).

## Method

- Binary copied to `resources/th10_ident.exe` and opened with idalib
  (`resources/th10_ident.exe.i64`, auto-analysis + FLIRT applied,
  Hex-Rays ready).
- PE header / IAT inspected with `pefile` + `objdump -x`; vendor-related
  strings extracted from the memory-mapped image.
- FLIRT name hits counted via `entity_query(kind=names)` over
  CRT/C++/library name patterns.

## Function layout (total 2,045 functions)

| Region | Address range | Count | Notes |
|---|---|---|---|
| Game code (ZUN) | 0x401000–0x452400 | 1,460 | recompilation target; FLIRT recognizes nothing here (1,274 still `sub_`) |
| Statically linked vendor code | 0x452400–0x471000 | 585 | 487 FLIRT-named (CRT/C++ runtime), 98 unnamed |

First function past the boundary: `TextOutA` thunk at 0x452404; CRT
proper starts near `_free` 0x452422 / `__nh_malloc` 0x4526da.

Boundary caveats (verified by spot checks; split remains valid):

- The tail of the "game" region, 0x45221e–0x452400, is the IAT import
  thunk block (~81 named API thunks: `CreateFileA`, `timeSetEvent`,
  window/GDI wrappers, ...). Linker-emitted plumbing, not ZUN logic.
- A few vendor bits are statically placed *inside* the game region:
  `j__memcpy` thunk 0x4367d0, `__RTC_NumErrors` 0x4464f4 (with its RTC
  check machinery around 0x4464xx), and compiler-emitted `std::string`
  inline helpers `unknown_libname_1/2/3` (0x4352c0, 0x4383e0,
  0x438680; IDA marks them "Microsoft VisualC 2-14/net runtime").
  These are template instantiations tied to ZUN's objects, so the
  rebuild must reproduce them within the game objects.
- The 98 unnamed functions in the CRT region were spot-checked and are
  CRT internals (locale/EH/heap variants interleaved with FLIRT-named
  CRT code), not misfiled game code.

## Dynamically linked libraries (IAT, 9 DLLs)

- `d3d9.dll` — Direct3D 9 (single import `Direct3DCreate9`).
- `d3dx9_31.dll` — 16 D3DX helpers (`D3DXMatrixMultiply`,
  `D3DXMatrixPerspectiveFovLH`, `D3DXLoadSurfaceFromMemory`,
  `D3DXCreateTextureFromFileInMemoryEx`, `D3DXVec3*`, ...). The `_31`
  suffix maps to **DirectX SDK October 2006**; matches the local
  `references/…redxsdk-oct2006media` SDK copy.
- `DINPUT8.dll` (DirectInput 8), `DSOUND.dll`, `WINMM.dll` (MIDI out,
  joystick, timeGetTime), `USER32.dll`, `GDI32.dll`, `KERNEL32.dll`,
  `ole32.dll`.
- No import of `msvcr*/msvcp*` — the CRT is statically linked.

## Statically linked vendor code (0x452400–0x471000)

- **MSVC 7.10 CRT** (Visual C++ .NET 2003, static multithreaded
  LIBCMT): PE linker version `7.10`; FLIRT hits include `__nh_malloc`,
  `___sbh_free_block` (SBH heap), `__freefls`/`__freeptd` (MT CRT
  teardown), `___get_qualified_locale`, stdio (`_sprintf`, `_vsprintf`,
  `__freebuf`), mem (`_memcpy`, `_memset`).
- **C++ exception support**: `__CxxFrameHandler` family,
  `CatchIt`/`FindHandler`/`CallCatchBlock`, `std::exception`,
  `std::logic_error/length_error/out_of_range`, `std::string`
  (`_String_base::_Xran/_Xlen`) — VC7.1 STL layout.
- **/GS**: `___security_init_cookie` 0x45f1f3,
  `@__security_check_cookie@4` 0x458ea5, `___security_cookie`
  0x473660.
- **/RTC residue**: `__RTC_NumErrors` 0x4464f4 and validators
  (`_ValidateRead/Write/Execute`).

## Absent

- No Bink/Smacker, zlib/libpng, vorbis, or any other middleware.
- LZSS decompression (`src\pack\LzUtil.cpp` assert path), MIDI playback
  (`src\core\midilib.cpp`), and all audio/window glue are ZUN's own
  `zunlib`. Embedded assert strings reveal source layout:
  `src\core`, `src\game`, `src\pack`, `src\script`; build dir
  `c:\cygwin\home\zun\prog\th10\`.

## Toolchain requirements for a matching build

VS 2003 (VC 7.1), `/GS`, static multithreaded CRT, DirectX SDK
October 2006 (d3dx9_31.lib import set above), Win32/x86, no CRT DLL
imports.

## Context from reference decomps

- TH06 (GensokyoClub): 1,508 mapped functions, ~508 `th06::` game
  functions, rest statically linked D3DX/CRT (`library`).
- TH07 (some100): 1,587 functions total = 694 game `function` + 893
  `library` + 1 type; badge 100% implemented / 99.78% accuracy
  (th07.exe), custom.exe 100%/100%.
