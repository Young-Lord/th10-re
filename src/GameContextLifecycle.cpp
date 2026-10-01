// TH10 game-context (DAT_004776ec) lifecycle: the 0x48-byte object that
// owns the scheduler records driving the popup update (0x405840 -> the
// EDI-ABI popup updater 0x405750) and the always-ready draw stub
// (0x405850).

#include "Th10Types.hpp"
#include "Th10Platform.hpp"
#include "CallbackScheduler.hpp"
#include "GameContext.hpp"
#include "ManagerReleaseWrappers.hpp"

namespace th10 {

extern CallbackScheduler *g_CallbackScheduler; // TH10 ds:0x491be4
extern void *g_StageNode;                      // TH10 DAT_004776ec holder

// TH10 0x0040405... (native thunk 0x405840): ECX adapter for the popup
// updater. Declared here as the callback targets used below; the thunk
// bodies themselves stay ASM boundaries.
i32 TH10_FASTCALL GameContextPopupUpdateAdapterThunk(void *context);
i32 TH10_FASTCALL GameContextDrawReadyThunk(void *context);

namespace {

const u32 kCalcPriority = 0x11;
const u32 kDrawPriority = 0x22;

} // namespace

// TH10 0x004055c0. Native EBX = the game-context object. Allocates two
// scheduler chain elements (0x449ed0), enables both (flags |= 2), points
// each element's argument at the context, registers the popup-update
// callback on the calculation chain at priority 0x11 and the draw stub on
// the draw chain at priority 0x22, and publishes the elements at
// context+8 / context+0xc. Returns 0.
i32 RegisterGameContextSchedulerRecordsEbxAbi(void *context) {
    GameContext &ctx = *static_cast<GameContext *>(context);
    ChainElem *calc = CallbackSchedulerApi::Create(
        reinterpret_cast<ChainCallback>(GameContextPopupUpdateAdapterThunk));
    calc->flags |= ChainElemFlag_Enabled;
    calc->arg = context;
    CallbackSchedulerApi::AddToCalculationChain(g_CallbackScheduler, calc,
                                                kCalcPriority);
    ctx.calc_record = calc;

    ChainElem *draw = CallbackSchedulerApi::Create(
        reinterpret_cast<ChainCallback>(GameContextDrawReadyThunk));
    draw->flags |= ChainElemFlag_Enabled;
    draw->arg = context;
    CallbackSchedulerApi::AddToDrawChain(g_CallbackScheduler, draw,
                                         kDrawPriority);
    ctx.draw_record = draw;
    return 0;
}

// TH10 0x004056b0. operator new(0x48) for the game-context object; the
// native wipes 0x12 dwords (dead store against the trailing flag set),
// sets bit 1 of the first dword and publishes DAT_004776ec, then calls
// the registration above. On a registration failure (nonzero result) or a
// null allocation the object is destroyed in place (0x405620) and freed;
// the null-allocation path still runs the registration (quirk preserved).
void *CreateGameContextObject() {
    void *context = ::operator new(0x48U);
    if (context != 0) {
        u32 *wipe = static_cast<u32 *>(context);
        for (int i = 0; i < 0x12; ++i) {
            wipe[i] = 0;
        }
        // Bit 1 of the +0x00 flags dword (dead store against the wipe).
        static_cast<GameContext *>(context)->flags |= 2u;
    }
    g_StageNode = context; // publish DAT_004776ec
    const i32 result = RegisterGameContextSchedulerRecordsEbxAbi(context);
    if (result != 0) {
        if (context != 0) {
            DestroyGameContextInPlace(context);
            ::operator delete(context);
        }
        return 0;
    }
    return context;
}

} // namespace th10
