# ASCII Projected Renderer Evidence

## `0x00443b60`: perspective billboard staging quad

This EAX-only helper builds an identity world matrix from the three accumulated
VM position axes, projects origin and the active render-state reference point,
and rejects only projected depths below zero or above one. It scales glyph
width/height by half the projected-point distance, applies VM `+0x2c` screen
rotation and `+0x35c` anchor fields, then writes only X/Y/Z of the four staging
vertices. Anchor value three intentionally retains uninitialized local values.

## `0x00444240`: mode-7 matrix/projection setup

This EAX-VM plus stack-owner helper conditionally rebuilds VM `+0x27c` from
`+0x23c` when flags permit, applying scale and nonzero X/Y/Z rotations. It
creates the final world matrix with X/Y additive and Z replacement translation,
projects an anchor-selected 256-unit local quad into staging vertices, and
copies the final matrix to owner `+0x3ad0f0`. All D3DX return values are
ignored. The mode-7 fogging caller uses this published matrix for its per-vertex
distance calculation.

## Screen-rotated and mode-4 wrappers

`0x004436c0` and `0x00443910` construct full-height screen-space rotated
quads and submit normally. Both use the same anchors and accumulated position
fields; mode 1 falls back to the half-height scaled path for zero *or NaN*
angle, while mode 3 falls back only for ordered zero. `0x00443f80` is the
mode-4 wrapper: it propagates perspective geometry rejection from `0x443b60`,
then submits with flag zero.

## Modes 5 and 6

Mode 5 (`0x00444580`) always uses the mode-7 projection builder, submits with
uniform color, then restores only staging `rhw` to 1.0. Mode 6 (`0x00443fb0`)
uses the billboard projection helper, chooses/modulates VM color, applies the
unclamped distance-fog arithmetic, rejects when the resulting fade is at least
one, then submits with flag 2 so the per-vertex fog color is retained.

Mode 7 (`0x004445c0`) transforms four owner-provided fog sample positions by
the world matrix produced by the projection helper, derives each vertex color
individually with toward-zero channel arithmetic or fixed fog RGB, submits
with flag 2, and restores only `rhw` afterwards. It intentionally bypasses
owner color modulation.
