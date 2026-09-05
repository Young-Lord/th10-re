# Main Chain Draw Callback Evidence

`0x00420470` registers `0x004200c0` and `0x004200d0` through the draw-chain
path with priorities 40 and 50 respectively. The first returns one directly.
The second loads `0x00491c10` into `ESI`, calls `0x00442f50`, clears
`0x00491e64` and `0x00491e68`, and returns one.

The callee uses the implicit `ESI` setup, so the reconstruction preserves that
register contract. The identities of the two cleared globals remain unknown.

The two functions were exported with explicit end ranges and compared on
2026-08-28. The grouped 45-byte `.text` section is byte-identical to the
reconstruction and objdiff reports `100.0%`.
