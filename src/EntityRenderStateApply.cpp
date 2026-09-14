// Semantic reconstruction of TH10 0x004423e0.
//
// Native caller: 0x00444760 (sprite/effect render preparation; the render
// owner arrives in EAX from the stack argument and the entity in EBX), and
// the sibling 0x004425a0 applies only the blend + sampler halves (EDI =
// entity, no color block). The equivalent mode-8 VM path is reconstructed in
// AsciiMode8Renderer.cpp (UpdateMode8SharedState); the differences there are
// the D3D entry used for the sampler state (vtable +0x10c / slot 67,
// SetTextureStageState, versus +0x114 / slot 69, SetSamplerState, here) and
// the color-source flag (both use +0x35c bit 15).

#include "EntityRenderStateApply.hpp"

#include "MainChainRender.hpp"
#include "Th10Platform.hpp"

namespace th10 {

namespace {

typedef i32 (TH10_STDCALL *D3DSetRenderStateFn)(D3D9Device *, u32, u32);
typedef i32 (TH10_STDCALL *D3DSetSamplerStateFn)(D3D9Device *, u32, u32, u32);

extern D3D9Device *g_MainChainD3D9Device; // TH10 DAT_00491c30

u32 ReadU32At(const void *address)
{
    return *reinterpret_cast<const u32 *>(address);
}

// The modulation multipliers at owner+0x732458..+0x73245b are 0..256 fixed
// point applied per byte with a >>7 shift and an explicit 0xff clamp. The
// native reads the multipliers as bytes and the color channels as bytes of
// the selected color dword.
u32 ModulateColorBytes(u32 color, const u8 *owner)
{
    u32 output = 0;
    for (u32 index = 0; index != 4U; ++index) {
        u32 channel = ((color >> (index * 8U)) & 0xffU) *
            owner[0x732458U + index] >> 7;
        if (channel > 0xffU)
            channel = 0xffU;
        output |= channel << (index * 8U);
    }
    return output;
}

void SetDeviceRenderState(u32 state, u32 value)
{
    (void)reinterpret_cast<D3DSetRenderStateFn>(g_MainChainD3D9Device->vtable[
        0xe4U / 4U])(g_MainChainD3D9Device, state, value);
}

void SetDeviceSamplerState(u32 sampler, u32 type, u32 value)
{
    (void)reinterpret_cast<D3DSetSamplerStateFn>(g_MainChainD3D9Device->vtable[
        0x114U / 4U])(g_MainChainD3D9Device, sampler, type, value);
}

} // namespace

// TH10 0x004423e0. EAX = owner, EBX = entity.
void ApplyEntityRenderStateEaxEbxAbi(void *owner /* EAX */,
                                     const void *entity /* EBX */)
{
    u8 *const owner_bytes = static_cast<u8 *>(owner);
    const u8 *const entity_bytes = static_cast<const u8 *>(entity);

    // Blend mode: +0x35c bits 4..5 against the owner blend cache at
    // +0x3ada68. On a mismatch the pending vertices are flushed first, then
    // the cache is refreshed and D3DRS_DESTBLEND (20) is reprogrammed:
    // mode 0 -> 6 (D3DBLEND_INVSRCALPHA), modes 1 and 2 -> 2
    // (D3DBLEND_ONE), mode 3 leaves the device state untouched while the
    // cache still advances.
    const u8 blend = static_cast<u8>((ReadU32At(entity_bytes + 0x35cU) >> 4)
        & 3U);
    if (owner_bytes[0x3ada68U] != blend) {
        FlushRenderOwnerPendingVertices(reinterpret_cast<RenderOwnerPartial *>(
            owner));
        owner_bytes[0x3ada68U] = blend;
        if (blend != 3U) {
            SetDeviceRenderState(0x14U, blend == 0U ? 6U : 2U);
        }
    }

    // Texture-factor color: bit 15 of +0x35c picks between the secondary
    // (+0x300) and primary (+0x2fc) color dwords. When the owner's global
    // modulation gate at +0x73245c is nonzero, each byte is scaled by the
    // matching multiplier; otherwise the raw color is published.
    u32 color = (ReadU32At(entity_bytes + 0x35cU) & 0x8000U) != 0U
        ? ReadU32At(entity_bytes + 0x300U)
        : ReadU32At(entity_bytes + 0x2fcU);
    if (ReadU32At(owner_bytes + 0x73245cU) != 0U)
        color = ModulateColorBytes(color, owner_bytes);

    if (ReadU32At(owner_bytes + 0x3ada60U) != color) {
        FlushRenderOwnerPendingVertices(reinterpret_cast<RenderOwnerPartial *>(
            owner));
        *reinterpret_cast<u32 *>(owner_bytes + 0x3ada60U) = color;
        SetDeviceRenderState(0x3cU, color); // D3DRS_TEXTUREFACTOR (60)
    }

    // Sampler selection: bit 31 of +0x35c against the owner cache at
    // +0x3ada6e. 0 -> linear (2), 1 -> point (1), applied to both
    // D3DSAMP_MAGFILTER (5) and D3DSAMP_MINFILTER (6) of sampler 0 through
    // the vtable +0x114 entry.
    const u8 sampler = static_cast<u8>(ReadU32At(entity_bytes + 0x35cU) >> 31);
    if (owner_bytes[0x3ada6eU] != sampler) {
        FlushRenderOwnerPendingVertices(reinterpret_cast<RenderOwnerPartial *>(
            owner));
        owner_bytes[0x3ada6eU] = sampler;
        const u32 filter = sampler == 0U ? 2U : 1U;
        SetDeviceSamplerState(0U, 5U, filter);
        SetDeviceSamplerState(0U, 6U, filter);
    }

    // Owner state version bump; the native increments unconditionally.
    ++*reinterpret_cast<u32 *>(owner_bytes + 0x54U);
}

} // namespace th10
