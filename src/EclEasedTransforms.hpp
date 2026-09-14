#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x00441600. Native EAX = 0x4c-byte eased RGB color block, one stack
// argument (ret 4) = three output dwords (the callers re-read the low byte
// of each as the R/G/B channel). Block layout: start vec3 (int channels)
// @0, end @0xc, handle1 @0x18, handle2/vel @0x24, timer prev @0x30, timer
// @0x34, accumulator @0x38, rate pointer @0x3c, flags @0x40, duration
// @0x44, mode @0x48. Mode 7 adds the delta into the start triple, 0x11
// integrates the velocity dword, 8 rides a cubic Hermite with the int
// scaled by the weight and truncated per component, everything else eases
// through the 0x44c350 curve selector. On completion the block rearms
// (duration 0, accumulator = (float)old duration) and emits the endpoint.
void TickRgbColorInterpolationEaxStackAbi(void *block, u32 out_rgb[3]);

// TH10 0x00441950. Native ESI = 0x2c-byte eased scalar block {start @0,
// end @4, handle1 @8, handle2/vel @0xc, prev @0x10, timer @0x14,
// accumulator @0x18, rate pointer @0x1c, flags @0x20, duration @0x24,
// mode @0x28}; returns the eased value in EAX (the callers keep the low
// byte as the alpha channel). Same timer/mode family as 0x441600.
i32 TickAlphaInterpolationEsiAbi(void *block);

// TH10 0x00441ad0. Native ESI = 0x3c-byte eased vec2 scale block (float
// components: start @0, end @8, handle1 @0x10, handle2/vel @0x18, prev
// @0x20, timer @0x24, accumulator @0x28, rate pointer @0x2c, flags @0x30,
// duration @0x34, mode @0x38), EDI = two-float output; returns EDI in
// EAX. Drives the entity scale pair at +0x3c/+0x40.
void *TickScaleInterpolationEsiEdiAbi(void *block, float out_xy[2]);

// TH10 0x00445620. Native ECX (this) = entity owning the 0x4b0-byte radial
// ribbon vertex buffer at +0x358 (installed by 0x4452f0 with the render
// callback 0x445880). Per-frame update: re-centers vertex 0, scrolls the
// per-vertex U/V accumulators by the +0x4a4 jitter (the full 33-entry
// coordinate sweep only while the triggered accumulator stays negative),
// rewrites every vertex color, advances theta[i] += theta2[i] and rebuilds
// each vertex xy from FSINCOS of the packed color dword bits (native
// quirk: the raw +0x2fc color dword is the angle, theta is the radius),
// drifts the positions by the entity center and duplicates vertex 1 into
// the wrap-around slot 32. Returns zero.
void RibbonFrameUpdateCallback(void *entity);

// TH10 0x00445880 (boundary). Render half of the ribbon callback pair;
// defined by the entity ticker.
void RibbonRenderCallback();

} // namespace th10
