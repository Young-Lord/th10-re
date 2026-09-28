# MSVCP60 std::string internals (0x00436770-0x004385b0)

Module: `src/StdStringCrt.cpp`

Object layout: SSO buffer / heap pointer at +0x04, size at +0x14,
capacity at +0x18 (15 marks the small buffer). Allocation goes through
`MenuItemStringAssign` (0x004386b0, registered); error paths call
`_String_base::_Xran` (0x462428) / `_Xlen` (0x4624e5), modeled as the
`StdStringBaseXran/Xlen` boundaries.

- `0x004381e0` empty ctor state; `0x00438260` assign(substr) preserving
  the native self-assign order (erase tail, then head);
- `0x00438420` range assign with the self-overlap check that delegates
  the overlapping tail to 0x00438260;
- `0x00438380` heap-to-SSO tidy; `0x00438510` erase; `0x00438590`
  truncate; `0x004385b0` reserve/shrink;
- `0x00436770`/`0x004367a0` are the byte-copy helpers used by the same
  cluster.
