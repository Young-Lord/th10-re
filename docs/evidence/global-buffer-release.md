# Global Buffer Release Evidence

`0x0041f930` conditionally releases two pointer fields at offsets `0x3ad084`
and `0x3ad088` from the global pointer stored at `0x00491c10`. For each
non-null field, it calls `0x00447810`, frees the field, and stores zero back
into the field. It returns zero.

The `0x00447810` calls use implicit register state, notably `EDI`, so the
reconstruction preserves the observed register setup and call ordering. The
function is named `ReleaseGlobalBuffers` only as a behavioral label; neither
the global-base type nor the two buffer types are established.

`GlobalBufferRelease.obj` was exported with `ExportTh10Delinker.java` and
compared on 2026-08-28. Both `.text` sections are 91 bytes and compare
byte-for-byte equal; objdiff reports `100.0%`.
