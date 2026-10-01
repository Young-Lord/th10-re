// TH10 large render-owner clear-color setters (0x4052e0 / 0x405340).
//
// The big render owner published at 0x491c10 stores the current D3D clear
// color as a dword at +0x732458 and a "custom color active" flag at
// +0x73245c.

#include "Th10Types.hpp"
#include "LargeRenderOwnerLayout.hpp"

namespace th10 {

// TH10 0x004052e0. Native EAX = render owner. Resets the clear color to
// the packed 0x80808080 (gray) default and clears the custom flag.
void RenderOwnerResetClearColorEaxAbi(void *owner) {
    LargeRenderOwnerLayout &owner_state =
        *static_cast<LargeRenderOwnerLayout *>(owner);
    owner_state.custom_color_gate = 0;
    owner_state.clear_color = 0x80808080U;
}

// TH10 0x00405340. Native EAX = render owner, ECX = packed color. Stores
// the custom clear color and sets the custom flag to 1.
void RenderOwnerSetClearColorEcxEaxAbi(void *owner, u32 color) {
    LargeRenderOwnerLayout &owner_state =
        *static_cast<LargeRenderOwnerLayout *>(owner);
    owner_state.custom_color_gate = 1;
    owner_state.clear_color = color;
}

} // namespace th10
