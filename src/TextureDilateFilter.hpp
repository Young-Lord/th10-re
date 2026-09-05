#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x004465b0. Native register ABI: EAX = pointer to a texture holder
// whose first dword is the IDirect3DTexture9-like COM object. Retrieves
// surface level 0 (holder vtbl +0x48), reads the surface description
// (vtbl +0x30), locks it (vtbl +0x34), runs the transparent-pixel dilate
// filter over the three supported formats, unlocks (vtbl +0x38) and
// returns the surface Release (vtbl +0x08) result. COM calls remain
// vtable boundaries.
i32 DilateTextureTransparentPixelsEaxAbi(void *texture_holder);

} // namespace th10
