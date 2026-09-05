# Global Manager Micro-Helper Evidence

`0x0041ff30` stores `ECX` to `[EAX + 0x390]` and returns. `0x0041ff40`
loads `[EAX + 0x150]`, shifts right by four, masks with one, and returns the
result. Both addresses are separated by `int3` padding and have no direct code
xrefs, so their type, class ownership, and potential virtual dispatch role are
not asserted.

The functions were exported through explicit exclusive-end address ranges and
compared on 2026-08-28. The grouped 20-byte `.text` section is byte-identical
to the reconstruction, and objdiff reports `100.0%`.
