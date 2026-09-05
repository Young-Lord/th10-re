#include "MainChainRender.hpp"
#include "Th10Platform.hpp"

namespace th10 {

namespace {

struct D3DStatePair {
    u32 state;
    u32 value;
};

typedef i32 (TH10_STDCALL *D3DSetRenderStateFn)(D3D9Device *, u32, u32);
typedef i32 (TH10_STDCALL *D3DSetTextureStageStateFn)(D3D9Device *, u32,
                                                       u32, u32);
typedef i32 (TH10_STDCALL *D3DSetSamplerStateFn)(D3D9Device *, u32, u32,
                                                  u32);

extern D3D9Device *g_MainChainD3D9Device; // TH10 DAT_00491c30
extern MainChainRenderOwnerFrameState *g_MainChainRenderOwner; // 0x491c10

const D3DStatePair kRenderStates[] = {
    { 7, 1 }, { 0x89, 0 }, { 0x16, 1 }, { 0x1b, 1 }, { 9, 2 },
    { 0x13, 5 }, { 0x14, 6 }, { 0x17, 8 }, { 0xf, 1 }, { 0x18, 7 },
    { 0x19, 1 }, { 0x1c, 1 }, { 0x26, 0x3f800000U }, { 0x23, 0 },
    { 0x8c, 3 }, { 0x22, 0xffa0a0a0U }, { 0x24, 0x447a0000U },
    { 0x25, 0x459c4000U }, { 0xa1, 0 }
};

const D3DStatePair kTextureStageStates[] = {
    { 4, 4 }, { 5, 2 }, { 6, 3 }, { 1, 4 },
    { 2, 2 }, { 3, 3 }, { 0x18, 2 }, { 0xb, 0 }
};

const D3DStatePair kSamplerStates[] = {
    { 7, 0 }, { 5, 2 }, { 6, 2 },
    { 3, 3 }, { 1, 1 }, { 2, 1 }
};

} // namespace

void RestoreMainChainD3DRenderStates()
{
    for (u32 index = 0; index != sizeof(kRenderStates) / sizeof(kRenderStates[0]);
         ++index) {
        (void)reinterpret_cast<D3DSetRenderStateFn>(
            g_MainChainD3D9Device->vtable[57])(
                g_MainChainD3D9Device, kRenderStates[index].state,
                kRenderStates[index].value);
    }
    for (u32 index = 0;
         index != sizeof(kTextureStageStates) / sizeof(kTextureStageStates[0]);
         ++index) {
        (void)reinterpret_cast<D3DSetTextureStageStateFn>(
            g_MainChainD3D9Device->vtable[67])(
                g_MainChainD3D9Device, 0, kTextureStageStates[index].state,
                kTextureStageStates[index].value);
    }
    for (u32 index = 0;
         index != sizeof(kSamplerStates) / sizeof(kSamplerStates[0]); ++index) {
        (void)reinterpret_cast<D3DSetSamplerStateFn>(
            g_MainChainD3D9Device->vtable[69])(
                g_MainChainD3D9Device, 0, kSamplerStates[index].state,
                kSamplerStates[index].value);
    }

    if (g_MainChainRenderOwner != 0) {
        g_MainChainRenderOwner->field_3ada68 = 3;
        g_MainChainRenderOwner->field_3ada69 = 0xff;
        g_MainChainRenderOwner->field_3ada6a = 0xff;
        g_MainChainRenderOwner->field_3ada64 = 0;
        g_MainChainRenderOwner->field_3ada6c = 0xff;
    }
}

} // namespace th10
