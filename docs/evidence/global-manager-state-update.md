# Main Chain Update Evidence

`0x0041ff80` receives a context pointer in `ECX`. `0x00420470` registers it
through the calculation-chain insertion path with argument `0x00491c28`; Ghidra
therefore names the function `MainChainUpdate` and that global
`g_MainChainContext`. This establishes callback role and global context, not a
source-level class name.

The callback conditionally writes three to `0x00491fb8` based on fields
`+0x3cc` and `+0x63c`, invokes
`0x00421e00`, `0x0044a5f0`, and `0x00447700`, then returns either four, a
value derived from field `+0x648`, or the result of `0x004218d0`.

The context type and relationship to any known game class remain unproven.
`src/MainChainContext.hpp` captures only the directly observed field offsets;
it intentionally does not assert the total object size or a source-level class
name.

`GlobalManagerStateUpdate.obj` was compared on 2026-08-28. Its 113-byte
`.text` section is byte-identical to the reconstruction and objdiff reports
`100.0%`.
