# TH10 0x00413a20 — Scene-Owner Stage Script Open

Module: `src/StageScriptOpen.cpp/.hpp`.

Native stdcall (ret 4), argument = the 0x9ecc-byte scene owner published at
`DAT_0047770c` (called from `0x00413980` — which loads `front.anm` into
`+0x9ec8` first — and from `0x00417870` `SetupGameSceneFromTitle`, where
the return value is ignored). Reconstructed as
`i32 OpenSceneScriptResource(void *scene_owner)`.

This corrects the earlier `ReleaseAsciiHudOwnerRecords` guess recorded in
`src/TitleSceneSetup.cpp`: the body never releases anything; it opens the
mode record's stage script resource.

## Behavior

1. `RequestManagerWork(owner = dword_491c10, slot = 0x1c, name =
   *(dword_477848 + 0x20))` — the 0x00447280 semantic body in
   `src/ManagerWork.cpp` with ECX = slot 28, EDX = the manager. The result
   lands in `owner+0x9e80`; zero jumps to the shared failure tail
   (`0x44b810` error text with ECX = `0x474f70`) and returns **-1**.
2. Handle selection:
   - When `dword_491be8` (the pre-opened demo/replay script handle) is
     non-zero it is consumed once: stored to `owner+0x9e9c` and the global
     cleared.
   - Otherwise the stage name comes from
     `dword_477848->names[g_SceneStageIndexA]` (`+0x18 + 4*index`), the
     shared path scratch `DAT_00497c38` is emptied and the name appended
     (the native clears the first byte then runs the inline
     strcat-equivalent), and `0x0044b360` opens it (native EAX = path,
     stack `(0, 0)`; modeled as `LoadMainChainFile(path, 0, 0)`). A zero
     result takes the failure tail.
3. Timer arming on success: the block at `+0x9e60` is lazily initialized
   (guard bit 0 at `+0x9e70`, `-999999` prev sentinel, `&flt_476f78` rate)
   and then unconditionally reset to `{-1, 0, 0}`; `+0x9ec0` and `+0x9ec4`
   are set to `-1`; the scene sub-timer `dword_474c44` is copied to
   `+0x9e78`. Returns **0**.

The path scratch is modeled as a TU-local buffer, matching the pattern the
timeline loaders (`TimelineStreamLoader.cpp` etc.) use for the same
`DAT_00497c38` singleton.
