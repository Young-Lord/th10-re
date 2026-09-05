# ASCII HUD Renderer Evidence

`0x00415800` is a stack-argument HUD batch callback that always returns one.
It dispatches existing VM render modes through the global render owner: ten
interleaved pairs at owner `+0x10/+0x24c8`, followed by runs of 9, 4, and 7 at
`+0x4980`, `+0x6a8c`, and `+0x8094`, each with VM stride `0x3ac`. Conditional
state gates add owner `+0x9a48` when the external state word `+0x2480` has
bits 0 and 4 clear, and two VMs at `+0x793c` when the owner's signed `+0x9ec0`
is nonnegative and `+0x9eb8` is zero.

The callback also emits horizontal progress rectangles. The initial two use
the signed global bar value and owner byte `+0x8393`. A finite ordered
`owner+0x9e84 > 0` emits two more, then considers four 8-byte entries at
`+0x9e94`; ordered zero is skipped, while NaN follows the native draw path.
The native ordered-min branch selects an entry only when it is ordered less
than the owner progress value.

`0x0043bda0` is the shared immediate colored rectangle path. It receives four
float coordinates `{left, top, right, bottom}` and a packed diffuse color,
flushes pending glyph vertices, makes four `XYZRHW | DIFFUSE` vertices, and
submits a two-primitive `D3DPT_TRIANGLESTRIP` with stride `0x14`. It sets
stage-0 color/alpha to select diffuse, writes destination blend state `6`,
and then restores only the glyph texture-modulation stage state. It directly
invalidates owner texture/blend/FVF-related caches at `+0x3ada64` and
`+0x3ada68..+0x3ada70`; it intentionally does not restore FVF or reset the
sampler cache at `+0x3ada6e`.
