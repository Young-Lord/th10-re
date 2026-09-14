# ECL Eased Transforms (0x441600 / 0x441950 / 0x441ad0 / 0x40d830 / 0x445620)

Semantic reconstruction in `src/EclEasedTransforms.cpp/.hpp`. Verified against
fresh IDA decompiles (session `th10re`); field offsets, mode dispatch,
Hermite weights and completion epilogues of 0x441950 were checked
instruction-for-instruction.

## Shared interpolation-block tail

Every eased ticker shares a seven-dword tail (offsets differ per block):

| field  | meaning |
|--------|---------|
| prev   | previous timer value; `-999999` (0xfff0bdc1) poison when stopped |
| timer  | integer frame counter |
| accum  | float accumulator advanced by the rate |
| rate   | `float*` source; one-time initialized to `&flt_476F78` (=1.0) behind flag bit 0 |
| flags  | bit 0: rate/accumulator initialized |
| duration | armed frame count; 0 disarms |
| mode   | 7 = add delta, 8 = cubic Hermite, 0x11 = velocity, else easing curve |

Timer advance: inside the rate-unity window (0.99 < rate < 1.01) the timer
simply increments and the accumulator steps 1.0; otherwise accum += rate and
timer = FTOL(accum). Completion (timer >= duration): one-time rearm of the
rate pointer behind flag bit 0 (its zeroing is immediately overwritten),
then timer = duration, prev = duration - 1, accum = (float)duration (FILD),
duration = 0.

## 0x441600 TickRgbColorInterpolationEaxStackAbi (EAX block, stack ret 4)

Three-component RGB interpolation. Head: current rgb at +0..+b, end rgb at
+0xc..+0x17, handle1 at +0x18, handle2/velocity at +0x24. Mode 7 adds the
delta; mode 0x11 does pos += vel, vel += end per integer channel; mode 8
evaluates the Hermite with h01 on the endpoint, h00 on the start, h10 on
handle1, h11 on handle2, each product scaled per component through
0x441e50-style FTOL(component * weight); default modes go through
`EasingCurveSelectorEaxStackAbi` (0x44c350) and round start +
FTOL(factor*delta). Completion copies the end rgb (start for mode 7).

## 0x441950 TickAlphaInterpolationEsiAbi (ESI)

Single-channel alpha block (start +0, end +4, handle1 +8, velocity +0xc).
Same mode family; the mode-8 path multiplies with FIMUL and truncates the
sum once; the default path is FTOL(factor * delta + start). Completion
returns +4 (start for mode 7).

## 0x441ad0 TickScaleInterpolationEsiEdiAbi (ESI, EDI out)

Two-component float scale block. Same modes in float arithmetic; the
default path stores the scaled delta as a float before adding the start
back. Completion copies +8/+c (or +0/+4 for mode 7).

## 0x40d830 ConstructEclScriptObjectEsiStackAbi (userpurge, ret 4)

Constructor of the 0x2518-byte ECL script object. Installs vtable
0x46d0c0, runs the 0x40cc70-style sub-record reset at +0x103c (clears the
pending bits of seven per-tick dwords and zeroes the eight 0x210-byte
command slots with -1 at +0x204), then zeroes the 0x537-dword tail
(overwriting the -1 markers), seeds self-pointers (+0x101c/+0x116c next =
self, +0x2514 = self), 24.0f at +0x10ec.., -1 at +0x248c and +0x1018, the
three animation tails at +0x1158/+0x2458/+0x246c (one-time rate init plus
unconditional prev = -1 arm), 32.0f at +0x243c/+0x2440, and eight
{-1,-1,0} triples from +0x2494. Finally reads the HUD conditional state
(DAT_00477704) presentation slot at +0x54 into +0x102c and resolves the
ctor argument through 0x450470 into the +8 node (id at +0xc, +8 = 0).

## 0x445620 RibbonFrameUpdateCallback (vtable/indirect)

Ribbon entity frame callback. Reads the buffer pointer at +0x358 without a
null check; takes the jitter step from buffer+0x4a4; stores the entity
center (+0x334 + +0x340) at buffer+0..+8; runs the texture-coordinate
scroll (trigger += jitter; while trigger < 0 all 33 per-vertex coordinates
at 0x1c stride advance by 1.0; unordered/NaN skips); copies the packed
color dword from +0x2fc into each vertex and — native quirk — reinterprets
it as the polar angle while the per-vertex theta slots (buffer+0x3a0) plus
per-vertex radius deltas (buffer+0x424) form the radius; the rotation
counter starts at -pi and advances 2*pi/31 per vertex but is never read.
Slot 32 duplicates vertex 1's seven dwords.

## Globals

- `g_FrameTimeScale` — TH10 DAT_00476F78, default frame-rate pointer (1.0f).
- `g_AsciiHudConditionalState` — TH10 DAT_00477704, its +0x54 feeds the ECL
  object presentation slot.
- `ResolveScriptTableIndexEaxAbi` — TH10 0x450470 boundary (name -> table id).

## ABI notes

All tickers keep their native register ABIs (EAX/ESI/EDI + stack) as thin
semantic entry points; the internal helpers are plain C++.
