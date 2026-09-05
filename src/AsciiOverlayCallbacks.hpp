#pragma once

#include "Th10Types.hpp"

namespace th10 {

// These semantic bodies receive their context in ECX in the original code.
// Native callback/vtable ABI remains a thunk boundary.
i32 DrawAsciiFullScreenOverlayWithViewport(void *context); // 0x0043c1a0
i32 DrawAsciiInsetOverlayA(void *context); // 0x0043c2c0
i32 DrawAsciiInsetOverlayB(void *context); // 0x0043c410
i32 DrawAsciiInsetOverlayMaskedRgb(void *context); // 0x0043c500
i32 UpdateAsciiOverlayFadeIn(void *context); // 0x0043c230
i32 UpdateAsciiOverlayHalfAlpha(void *context); // 0x0043c310
i32 DrawAsciiOverlayKindSix(void *context); // 0x0043c3c0
i32 UpdateAsciiOverlayFullFade(void *context); // 0x0043bd40
i32 UpdateAsciiOverlayKindFive(void *context); // 0x0043c460
i32 UpdateAsciiOverlayKindOne(void *context); // 0x0043c550
i32 UpdateAsciiOverlayKindEight(void *context); // 0x0043c710

} // namespace th10
