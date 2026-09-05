# ASCII Glyph Submission Evidence

## Scope

This note records the semantic C++ reconstruction of the shared glyph submit
path used by the ASCII manager. The native entries have register ABIs; typed
C++ bodies intentionally keep those conventions at thunk boundaries.

## `0x00442670`: submit staging quad

Input is VM in `EAX`, render owner in `ECX`, and one stack flag. It adds owner
`+0x5c/+0x60` to all four staging-quad X/Y pairs at `0x4978c0`. Flag bit 0
then applies x87 nearest-even rounding minus 0.5 and restores the rectangle
pairing. It loads UVs from glyph `vm+0x394` fields `+0x20/+0x24/+0x28/+0x2c`
plus VM `+0x54/+0x58`.

The helper rejects a quad outside the active viewport at `DAT_00491fac+0xcc`
through `+0xd8`. Visible quads update/bind glyph texture `+0x04`, force owner
`+0x3ada6a` to one, optionally populate all vertex colors from VM `+0x2fc` or
`+0x300`, and apply owner channel modulation when `+0x73245c` is nonzero.
It then applies the flag-selected render/sampler state and appends triangles.

## `0x004425a0`: glyph state transition

The state helper flushes first whenever either cache changes. VM flags
`+0x35c` bits 4..5 select owner `+0x3ada68` and D3D render state 20 values 6
or 2 for values zero and one. Bit 31 selects owner `+0x3ada6e` and sampler
states 5/6 values 2 or 1. It increments owner `+0x54` on every invocation.

## `0x00442fe0`: append six triangle-list vertices

With `EAX=owner` and `EDX=staging quad`, this branchless helper copies seven
dwords per vertex into the destination pointer at owner `+0x72dacc` in source
order `0,1,2,1,2,3`. It advances that pointer by `0xa8` and increments pending
primitive count `+0x3adac8`; no capacity check exists.

## `0x00442f50`: flush

The existing C++ body correctly treats primitive type 4 as
`D3DPT_TRIANGLELIST`, not triangle strip. It ignores all D3D HRESULTs, resets
the current destination pointer from `+0x72dacc`, clears pending count, and
increments owner `+0x58` after every nonempty draw.
