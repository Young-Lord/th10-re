# ASCII Overlay Callback Evidence

`0x0043c1a0`, `0x0043c2c0`, and `0x0043c410` are ECX-context render
callbacks registered indirectly by the overlay scheduler. Each builds packed
diffuse color exactly as `u32(context+0x20) | (u32(context+0x18) << 24)`;
the lower source is intentionally not masked before OR.

`0x0043c1a0` first flushes the global render-owner glyph batch, writes the
global viewport block at `0x00491cf4` to `(0, 0, 640, 480)`, calls the device
viewport setter, then draws `{0,0,640,480}` through the immediate colored
rectangle path. It returns one without any guard.

`0x0043c2c0` and `0x0043c410` independently draw the same fixed rectangle
`{32,16,416,464}` and return one. Their distinct native entries are retained:
the scheduler registers them for different overlay kinds even though their
current semantic bodies are equal.

`0x0043c500` uses the same fixed rectangle but combines alpha from `+0x18`
with `context+0x24 & 0x00ffffff`; unlike the other two inset entries, its RGB
source is explicitly masked before the OR.

`0x0043c230` is the type-2/type-5 fade-in update callback. It writes alpha
at `+0x18`, returns completion code 7 when suspended or when signed tick is at
least wrapped `duration+2`, and otherwise advances time only when the title
screen does not expose mask `0x5` at `+0x58`. Negative calculated alpha is
clamped to zero.

`0x0043c310` is the type-6/type-7 half-alpha updater. Its `+0x2c` fade-out
path returns 7 only after tick 8 and does not clamp alpha; the normal path
updates alpha while tick is no greater than duration. Both callbacks retain
the native rate behavior: only ordered `0.99 < rate < 1.01` increments tick,
while all other values including NaN convert accumulated time through the x87
toward-zero helper.

`0x0043c3c0` is kind 6's full-screen draw callback. It uses the same unmasked
packed color formula as the inset callbacks but does not perform the explicit
flush or viewport publication done by `0x0043c1a0`.

`0x0043bd40` is the full fade callback used by kinds 0 and 3. Unless the
global suspend flag is set, a nonzero signed duration at `+0x1c` produces
`truncate((1 - accumulated_time / duration) * 255)` at `+0x18`, with only a
negative-result clamp to zero. A signed current tick at `+0x34` at or beyond
duration returns completion code 7 before time advances; there is no title
screen gate and no upper alpha clamp.

`0x0043c460` is kind 5's repeating fade-out callback. While signed tick is
less than signed duration it writes `max(0, u8(context+0x27) -
truncate(u8(context+0x27) * accumulated_time / duration))` to `+0x18`, then
advances the shared timer. Once the interval has ended it clears alpha,
decrements `+0x20` with native 32-bit wraparound, and returns zero when the
new signed count is nonpositive. A positive count resets the timer block at
`+0x30` through native helper `0x405410`, then advances it once. Suspension
also returns zero, unlike the completion-code-7 callbacks.

`0x0043c550` is kind 1's no-draw interpolated random-offset updater. It first
advances the shared timer, returns 7 when suspended or when the new signed tick
is at least signed duration, and otherwise interpolates a magnitude from
signed `+0x20` toward signed `+0x24` using accumulated time / duration. It
uses two independent modulo-3 selections from the shared RNG for the global X
and Y offsets: zero, positive magnitude, or negative magnitude. The native
subtraction of the endpoint values wraps as a 32-bit value before its signed
integer-to-float conversion. Unlike kind 8's helper call, this entry packs its
second 16-bit RNG result into both halves of the modulo dividend.

`0x0043c710` is kind 8's no-draw random offset updater. After the suspend and
title mask `0x77` gates it advances time, applies signed rise/hold/fall phase
comparisons with wrapped boundaries, and writes two independent values from
`{0,+magnitude,-magnitude}` to the global render offsets. The rise denominator
uses signed conversion, while fall duration and total use unsigned conversion;
completion occurs after time advance and leaves prior offsets unchanged.
