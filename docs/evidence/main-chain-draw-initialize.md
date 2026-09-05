# Main Chain Draw Initialize Evidence

`0x00420470` installs `0x00420000` into the draw chain with argument
`0x00491c28`, identifying its `ECX` input as `g_MainChainContext`. The callback
uses fields `+0x8`, `+0x26c`, `+0x384`, and `+0x388`, resets several fields
under `0x00491c10`, calls `0x004215a0`, then performs two virtual calls through
the objects at context `+0x8` and global `0x00491c30`.

The pointed-to types, virtual methods, and the large global-base fields remain
unidentified. `MainChainContext.hpp` records only the observed context offsets.

`MainChainDrawInitialize.obj` was compared on 2026-08-28. Its 177-byte
`.text` section is byte-identical to the reconstruction and objdiff reports
`100.0%`.
