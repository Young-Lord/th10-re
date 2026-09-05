# Main Chain Registration Evidence

`0x00420470` initializes globals at `0x00491fb4`, `0x00491fb8`, and
`0x00491fc0`. It then creates a calculation-chain callback for `0x0041ff80`
and draw-chain callbacks for `0x00420000`, `0x004200c0`, and `0x004200d0`,
using `0x00491c28` as each callback argument.

The observed priorities are one for the calculation callback and one, 40, and
50 for the three draw callbacks. The registration API's detailed return
convention is not yet typed; the function returns its first insertion result,
or zero after all draw registrations.

`MainChainRegistration.obj` was compared on 2026-08-28. Its 204-byte `.text`
section is byte-identical to the reconstruction and objdiff reports `100.0%`.
