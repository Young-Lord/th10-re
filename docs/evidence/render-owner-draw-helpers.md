# Render-owner draw helpers (0x004415c0-0x004458b0)

Module: `src/RenderOwnerDrawHelpers.cpp` / `.hpp`

- `0x004415c0`: per-row player VM initialization through
  `InitializePlayerMainVmEsiStackAbi` (0x00404f30) with a 0x3ac stride;
  the native advances the forwarded VM-work cursor by ONE BYTE per row —
  quirk preserved — and copies word +0x384 to +0x388.
- `0x00441f50`/`0x00442050`: camera tween arming; both prime the eased
  interpolation sub-record (flag |1, -999999, easing table 0x00476f78,
  then -1), the same shape as the 0x434a80 record.
- `0x00442130/0x00442150/0x004421a0/0x004421c0/0x004422f0`: small vector
  stores; `0x004423c0` the (a*b)>>7 brightness clamp.
- `0x004425a0 ApplyOwnerRenderModesEaxEdiAbi`: from entity+0x35c — blend
  bits 4-5 drive `SetRenderState(20, 6/2)` (vtable +0xe4) and the sign bit
  drives `SetSamplerState(0, 5/6, 2/1)` point/linear filtering (vtable
  +0x114); flushes pending vertices through 0x00442f50 before each change
  and bumps owner+0x54.
- `0x00442f30`: resets the vertex free-list head at owner+0x3adac8 and
  links the sentinels at 0x72d54c/0x72d550 to +0x3adacc.
- `0x00444b10`/`0x00444be0`: ribbon quad builders (stride 14 floats,
  rhw/z/x/y column order, descending from uv[10]/uv[11]); -1 below 3
  vertices. `0x0044458b0` duplicates the 0x00441ef0 cos/sin body.
- `0x004450e0`: state-3 check (`SetFVF` 0x144 via vtable +0x164), render
  modes, texture bind (+0x104), render state 0x0e, stage states (+0x10c)
  and the 28-vertex triangle-fan `DrawIndexedPrimitive` (vtable +0x14c).
  Native quirk: the StartIndex/PrimCount words are read from stale stack;
  modeled as 0.
- `0x00445880`: dispatches the strip draw against `DAT_00491c10` with
  record+0x358 and the fixed 33-word budget.
