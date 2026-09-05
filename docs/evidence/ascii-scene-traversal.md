# ASCII Scene Traversal Evidence

`0x00403a30` is the scene/controller traversal above the ASCII render-mode
dispatcher. Its two stack arguments are a scene pointer and signed channel;
the render owner remains the separate global passed to `0x004451c0`.

The scene has a signed-terminated, 16-byte outer descriptor stream at `+0x18`.
Each descriptor selects a child pointer through the table at `+0x14`, carries
a world translation at `+0x04`, and is filtered by the child's signed byte at
`+0x02`. Accepted children have bit 1 set at `+0x03`, then use a
signed-terminated variable-length operation stream at `child+0x1c`. The next
record address is its signed 16-bit byte delta at `+0x02`; it is deliberately
not bounds-checked.

Every nonnegative operation computes its VM at `scene+0x17c + i16(+0x06) *
0x3ac`, switches the global mode-8 fog state after flushing when necessary,
dispatches it, and increments `scene+0x2a14` regardless of dispatcher result.
Only opcode zero applies position/size inputs. For modes 4 and above it writes
the descriptor-plus-operation translation to VM `+0x340..+0x348`; ordered zero
size inputs are skipped, but NaN size values retain the native division path.

`0x00403090` rejects a child when the squared distance from its translated
center to camera position exceeds the scene limit at `+0x1ee0`, including an
unordered comparison. It projects eight half-extent box corners, considers
only ordered depth values in the closed range `[0, 1]`, and tests their screen
bounds against the closed region `x=[32,416]`, `y=[16,464]`. The seeded bounds
and NaN update order are retained in the semantic C++ implementation.
