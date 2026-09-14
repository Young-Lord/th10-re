# Entity Render-State Apply: `0x004423e0`

Reconstruction: `src/EntityRenderStateApply.cpp/.hpp`
(`ApplyEntityRenderStateEaxEbxAbi`). Evidence gathered from the offline
disassembly of `resources/th10.exe` (440 bytes, no relocations).

## Identity

- Callers: `0x00444760` (sprite/effect render preparation; the render owner
  arrives in EAX from a stack slot at `0x444988`, the entity in EBX, and the
  call at `0x444994` follows the z-offset computation) and the deleting
  paths around it. `0x00444760` itself is a coverage-gap entry.
- Sibling: `0x004425a0` applies only the blend + sampler halves (EDI =
  entity, no color block); `src/AsciiMode8Renderer.cpp`'s
  `UpdateMode8SharedState` is the mode-8 VM twin of the same algorithm.

## Native ABI

`EAX = render owner` (the large `DAT_00491c10` object), `EBX = entity`
record. No stack arguments, plain `ret`. `ESI` holds the owner across the
body because `0x00442f50` (pending-vertex flush) consumes it implicitly.

## Body

1. **Blend mode** — `blend = (entity+0x35c >> 4) & 3` against the owner
   cache byte at `+0x3ada68`. On mismatch: flush pending vertices
   (`0x442f50`, ESI = owner) first, store the cache, then, through the
   device vtable entry `+0xe4` (slot 57, `SetRenderState`) on the device
   published at `DAT_00491c30`:
   - blend 0 → `(0x14, 6)` (D3DRS_DESTBLEND=20, D3DBLEND_INVSRCALPHA)
   - blend 1 → `(0x14, 2)` (D3DBLEND_ONE)
   - blend 2 → `(0x14, 2)` (the two native branches are byte-identical)
   - blend 3 → no device call, cache still advances.
2. **Texture-factor color** — bit 15 of `entity+0x35c` picks `+0x300`
   (set) or `+0x2fc` (clear). When the owner modulation gate
   `+0x73245c` is nonzero, each byte is scaled by the matching multiplier
   byte at `+0x732458..+0x73245b` with a `>>7` shift and a 0xff clamp.
   If the owner cache dword `+0x3ada60` differs: flush, store, and
   `SetRenderState(0x3c, color)` (D3DRS_TEXTUREFACTOR=60).
3. **Sampler** — bit 31 of `entity+0x35c` against the owner cache byte
   `+0x3ada6e`. On mismatch: flush, store, then through the device vtable
   entry `+0x114` (slot 69, `SetSamplerState`): `(0, 5, v)` and
   `(0, 6, v)` for D3DSAMP_MAGFILTER/MINFILTER with `v = 2` (linear) when
   the bit is clear and `v = 1` (point) when set.
4. **State bump** — `++owner+0x54` (dword), unconditionally, then `ret`.

Note the difference against the mode-8 twin: that path uses vtable slot 67
(`+0x10c`, `SetTextureStageState`) for the sampler pairs; both this
function and the `0x4425a0` sibling use slot 69. The color-source flag is
bit 15 in both (byte 1 of the flag dword is `test`ed against itself).

## Preserved quirks

- Cache misses always flush before the cache word is refreshed.
- Blend mode 3 updates the cache without touching the device.
- `test ah,ah` on the flag dword makes the color-source test a bit-15 test
  (not bit 31); the NaN channel behavior of the modulation is byte-exact.
