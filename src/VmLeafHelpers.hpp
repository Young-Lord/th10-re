#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x00404610. Native EAX = interpolator base, EDI = output vec3.
// Ticks the vec3 animation block (modes 7/8/0x11/default easing).
void TickVec3Interpolator(void *block, float out_vec3[3]);

// TH10 0x0044C350. Native EAX = curve mode, stack = {value, denominator}
// (retn 8), result returned as a double in ST0. Eases value/denominator:
// modes 1..3 ease-in (t^2..t^4), 4..6 ease-out (1-(1-t)^2..^4), 9..11
// ease-in-out quad/cubic/quart, 12..14 the mirrored in-out formulations,
// 15 returns 0.0, 16 returns 1.0, every other mode (0, 7, 8, > 16)
// returns t unchanged. NaN takes the "out" branch of the in/out pairs.
double EasingCurveSelectorEaxStackAbi(i32 mode, float value,
                                      float denominator);

// TH10 0x004050d0. Native EAX = interpolator base; re-arms the timer.
void ResetVec3InterpolatorTimer(void *block);

// TH10 0x00413200. Native EAX = new node (entity+0x10), ECX = list owner
// (entity+0x10); push-front into the doubly-linked child list.
void LinkChildListNode(void *node, void *list_owner);

// TH10 0x0041ab70. Sets up the two-component scale interpolation block at
// entity+0x180 against the current scale at entity+0x3c.
void SetupScaleInterpolation(void *entity, const float target[2],
                             i32 duration, i32 mode);

// TH10 0x00442220 / 0x00442050. RGB interpolation blocks at vm+0xbc
// (outputs 0x2fc..0x2fe) and vm+0x1bc (outputs 0x300..0x302).
void SetupRgbInterpolation(void *vm, u32 block_base, const u8 start[3],
                           const u8 target[3], i32 duration, i32 mode);

// TH10 0x00442300 / 0x00441f50. Alpha interpolation blocks at vm+0x108
// (output 0x2ff) and vm+0x208 (output 0x303).
void SetupAlphaInterpolation(void *vm, u32 block_base, i32 start,
                             i32 end, i32 duration, i32 mode);

// TH10 0x00428dd0. Rewinds the VM script timer by n frames (the opcode
// 0x4b time-shift mechanic).
void ShiftVmTimerBack(void *vm, i32 frames);

// TH10 0x004452f0. Native EDI = entity; frees and rebuilds the 32-segment
// radial ribbon vertex buffer at entity+0x358 (0x4b0 bytes) and installs
// the per-frame/render callbacks. Returns zero.
i32 RebuildRibbonRingBuffer(void *entity);

} // namespace th10
