# Global Manager Chain Element Release Evidence

`0x0041ff60` copies `EAX` to `ESI`, advances it by `0x62c`, writes
`0x004703e4` to the pointed-to first field, and calls `0x0044c150`. It then
restores `ESI` and returns. The helper has one direct caller at `0x004659f5`.

`0x0044c150` operates on `ESI` and clears fields including `+0x4` and `+0x18`;
this supports describing the action as release/reinitialization, but does not
establish the enclosing object or the precise pointee type.

`GlobalManagerChainElementRelease.obj` was compared on 2026-08-28. Its
22-byte `.text` section is byte-identical to the reconstruction and objdiff
reports `100.0%`.
