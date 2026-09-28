// TH10 large render-owner clear-color setters (0x4052e0 / 0x405340).
//
// The big render owner published at 0x491c10 stores the current D3D clear
// color as a dword at +0x732458 and a "custom color active" flag at
// +0x73245c.

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x004052e0. Native EAX = render owner. Resets the clear color to
// the packed 0x80808080 (gray) default and clears the custom flag.
void RenderOwnerResetClearColorEaxAbi(void *owner) {
    u8 *bytes = static_cast<u8 *>(owner);
    *reinterpret_cast<u32 *>(bytes + 0x73245c) = 0;
    *reinterpret_cast<u32 *>(bytes + 0x732458) = 0x80808080U;
}

// TH10 0x00405340. Native EAX = render owner, ECX = packed color. Stores
// the custom clear color and sets the custom flag to 1.
void RenderOwnerSetClearColorEcxEaxAbi(void *owner, u32 color) {
    u8 *bytes = static_cast<u8 *>(owner);
    *reinterpret_cast<u32 *>(bytes + 0x73245c) = 1;
    *reinterpret_cast<u32 *>(bytes + 0x732458) = color;
}

} // namespace th10
