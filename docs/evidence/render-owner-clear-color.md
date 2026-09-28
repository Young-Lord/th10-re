# Render Owner Clear Color (0x4052e0 / 0x405340)

Module: `src/RenderOwnerClearColor.cpp`. The large render owner published
at 0x491c10 stores the current D3D clear color as a dword at +0x732458 and
a "custom color active" flag at +0x73245c.

- `0x004052e0` `RenderOwnerResetClearColorEaxAbi` — native EAX = render
  owner. Clears the custom flag first and then resets the clear color to
  the packed 0x80808080 (gray) default.
- `0x00405340` `RenderOwnerSetClearColorEcxEaxAbi` — native EAX = render
  owner, ECX = packed color. Stores the custom clear color and sets the
  custom flag to 1.
