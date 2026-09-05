#include "MainChainContext.hpp"
#include "MainChainRender.hpp"
#include "MainChainRuntime.hpp"

namespace th10 {

namespace {

extern RenderOwnerPartial *g_RenderOwner; // TH10 DAT_00491c10
extern i32 g_MainChainDrawFinalizeField0; // TH10 DAT_00491e64
extern i32 g_MainChainDrawFinalizeField1; // TH10 DAT_00491e68

} // namespace

// TH10 0x004200d0. Context is supplied by the scheduler but deliberately
// unused. The native flush helper consumes the render owner in ESI; its
// ordinary C++ semantic body preserves the D3D transaction and ignored HRESULTs.
i32 TH10_FASTCALL MainChainContext::DrawFinalize(MainChainContext *)
{
    FlushRenderOwnerPendingVertices(g_RenderOwner);
    g_MainChainDrawFinalizeField0 = 0;
    g_MainChainDrawFinalizeField1 = 0;
    return MainChainAdvance_Continue;
}

} // namespace th10
