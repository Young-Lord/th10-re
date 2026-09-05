#include "TimelineRecordInterpreter.hpp"

#include <string.h>

#include "AsciiOverlayFactory.hpp"
#include "BgmRuntime.hpp"
#include "ManagerWork.hpp"
#include "MainChainRuntime.hpp"
#include "TimelineAudioActions.hpp"
#include "TimelineContinuation.hpp"
#include "TimelineGateState.hpp"
#include "TimelineRenderObjects.hpp"
#include "TimelineStreamLoader.hpp"
#include "ThreadControl.hpp"

namespace th10 {
namespace {

extern void *g_MainChainRenderOwner;
extern u32 g_MainChainPresentDiagnosticOptions; // DAT_00474e30
extern i32 g_TimelinePhase; // DAT_00474c74
extern i32 g_AsciiOverlayUpdateSuspended; // DAT_004918a4
extern TransitionRootPartial g_TransitionRoot;
extern const float g_AsciiOverlayInitialRate;
extern i32 ConvertFloatToI32TowardZeroX87(float);

u32 R32(const u8 *p) { return *reinterpret_cast<const u32 *>(p); }
i32 I32(const u8 *p) { return *reinterpret_cast<const i32 *>(p); }
void W32(u8 *p, u32 v) { *reinterpret_cast<u32 *>(p) = v; }
void WI32(u8 *p, i32 v) { *reinterpret_cast<i32 *>(p) = v; }
float RF(const u8 *p) { return *reinterpret_cast<const float *>(p); }
void WF(u8 *p, float v) { *reinterpret_cast<float *>(p) = v; }
i32 WrapAdd(i32 a, i32 b) { return static_cast<i32>(static_cast<u32>(a) + static_cast<u32>(b)); }

void ResetTimer(u8 *t, i32 tick) {
    if ((R32(t + 0x10) & 1U) == 0) {
        W32(t + 0x10, R32(t + 0x10) | 1U); WI32(t + 4, 0);
        WI32(t, static_cast<i32>(0xfff0bdc1U)); WF(t + 8, 0.0f);
        *reinterpret_cast<const float **>(t + 12) = &g_AsciiOverlayInitialRate;
    }
    WI32(t + 4, tick); WI32(t, WrapAdd(tick, -1)); WF(t + 8, static_cast<float>(tick));
}
void AdvanceTimer(u8 *t) {
    const i32 tick = I32(t + 4); const float rate = **reinterpret_cast<const float *const *>(t + 12);
    WI32(t, tick);
    if (rate > .99f && rate < 1.01f) { WI32(t + 4, WrapAdd(tick, 1)); WF(t + 8, RF(t + 8) + 1.0f); }
    else { const float next = RF(t + 8) + rate; WF(t + 8, next); WI32(t + 4, ConvertFloatToI32TowardZeroX87(next)); }
}
void AdvanceRecord(u8 *s) { u8 *r = *reinterpret_cast<u8 **>(s + 0x54); *reinterpret_cast<u8 **>(s + 0x54) = r + 4 + r[3]; }
bool TransitionGate(const u8 *s) {
    if ((g_MainChainPresentDiagnosticOptions & 0x1001U) || I32(s + 0x30) <= 0) return true;
    if ((g_TimelineGateState->gate_word_0020 & 1U) || !(g_MainChainPresentDiagnosticOptions & 0x100U)) return false;
    return I32(s + 0x30) % 6 == 0;
}
void ReleaseSlot(void **slot) {
    ReleaseTimelineHandle(g_MainChainRenderOwner,
                          reinterpret_cast<i32>(*slot));
    *slot = 0;
}
void MarkTimelineTextHandle(void **slot, u16 kind) {
    u8 *const object = static_cast<u8 *>(ResolveTimelineHandle(
        g_MainChainRenderOwner, reinterpret_cast<i32>(*slot)));
    if (object == 0)
        return;

    *reinterpret_cast<u16 *>(object + 0x304) = kind;
    if (*reinterpret_cast<const u32 *>(object + 0x18) != 0)
        return;

    for (u8 *node = *reinterpret_cast<u8 **>(object + 0x14); node != 0;
         node = *reinterpret_cast<u8 **>(node + 0x04)) {
        u8 *const child = *reinterpret_cast<u8 **>(node);
        *reinterpret_cast<u16 *>(child + 0x304) = kind;
    }
}
void MarkTimelineTextKindTwo(void **slot) { MarkTimelineTextHandle(slot, 2); }
void MarkTimelineTextKindThree(void **slot) { MarkTimelineTextHandle(slot, 3); }
void MakeObject(u8 *s, const u8 *r) {
    const i32 dst = I32(r + 4);
    void **out = reinterpret_cast<void **>(s + 0x90) + dst;
    ReleaseSlot(out);
    i32 *const created = SpawnSetupEffectVmListABack(I32(r + 12), 15);
    *out = reinterpret_cast<void *>(*created);
}
void HandleTextOpcode(u8 *s, const u8 *r) {
    void **const slots = reinterpret_cast<void **>(s + 0x40);
    i32 n = I32(s + 0x78);
    if (n == 0) {
        for (i32 i = 0; i != 5; ++i) {
            void *const object = ResolveTimelineHandle(
                g_MainChainRenderOwner, reinterpret_cast<i32>(slots[i]));
            if (object == 0)
                slots[i] = 0;
            SubmitTimelineText(object, g_MainChainRenderOwner, 0xffffff, " ");
            MarkTimelineTextKindThree(slots + i);
        }
        void *const object = ResolveTimelineHandle(
            g_MainChainRenderOwner, reinterpret_cast<i32>(slots[0]));
        if (object == 0)
            slots[0] = 0;
        SubmitTimelineText(object, g_MainChainRenderOwner, R32(s + 0x7c),
                           DecryptTimelineRecordText(r + 4));
        MarkTimelineTextKindTwo(slots);
    } else {
        void *const object = RefreshTimelineTextHandle(slots + n);
        SubmitTimelineText(object, g_MainChainRenderOwner, R32(s + 0x7c),
                           DecryptTimelineRecordText(r + 4));
        MarkTimelineTextKindTwo(slots + n);
    }
    n = WrapAdd(n, 1);
    WI32(s + 0x78, n >= 5 ? 0 : n);
}
void Transition(u8 *s, const u8 *r, bool clear) {
    if (I32(s + 0x30) <= 0) ResetTimer(s + 0x2c, I32(r + 4));
    SetTimelineAudioValue(-1.0f); if (!clear && I32(r + 4) < 0) ResetTimer(s + 0x2c, 999);
    if (!TransitionGate(s)) return;
    if (!(g_MainChainPresentDiagnosticOptions & 0x1001U) && I32(s + 0x30) > 0) ResetTimer(s + 0x2c, 0);
    else { EnqueueBgmSoundValue(&g_TransitionRoot, 0, 0); ResetTimer(s + 0x2c, 0); }
    if (clear) { WI32(s + 0x78, 0); g_AsciiOverlayUpdateSuspended = 0; }
}
} // namespace

i32 ExecuteTimelineRecords(void *memory) {
    u8 *const s = static_cast<u8 *>(memory);
    if (R32(s + 0x74) & 4U) return 0;
    if (!(g_TimelineGateState->gate_word_0020 & 1U) && !(R32(s + 0x74) & 2U) &&
        (g_MainChainPresentDiagnosticOptions & 0x100U) && (R32(s + 0x74) & 1U)) {
        ResetTimer(s + 0x18, static_cast<i32>(*reinterpret_cast<const u16 *>(*reinterpret_cast<u8 **>(s + 0x54))));
    }
    for (;;) {
        u8 *const r = *reinterpret_cast<u8 **>(s + 0x54);
        if (I32(s + 0x1c) < static_cast<i32>(*reinterpret_cast<const u16 *>(r))) break;
        switch (r[2]) {
        case 0: return -1;
        case 3: HandleTextOpcode(s, r); break;
        case 4: for (i32 i=0;i!=5;++i) MarkTimelineTextKindThree(reinterpret_cast<void **>(s+0x40)+i); break;
        case 5: Transition(s,r,false); if (!TransitionGate(s)) return 0; break;
        case 6: Transition(s,r,true); if (!TransitionGate(s)) return 0; break;
        case 7:
            StartTimelineContinuation(480.0f, 392.0f);
            ReleaseManagerWorkAtSlot(
                reinterpret_cast<ManagerWorkOwnerPartial *>(g_MainChainRenderOwner),
                WrapAdd(I32(r + 4), 0x1d));
            *reinterpret_cast<u8 **>(s + 0x70) = const_cast<u8 *>(r + 8);
            W32(s + 0x74, R32(s + 0x74) | 4U);
            RegisterTimelineContinuation(
                reinterpret_cast<ThreadControl *>(s + 0xd0), s);
            AdvanceRecord(s);
            return 0;
        case 8: MakeObject(s,r); break;
        case 9: W32(s+0x7c,R32(r+4)); break;
        case 10: SubmitTimelineAudioPath(0,reinterpret_cast<const char *>(r+4)); SelectTimelineAudioMode(0,memcmp(r+4,"bgm/th10_13.wav",16)==0?15:16); break;
        case 11: SetTimelineAudioValue(3.0f); W32(s+0x74,R32(s+0x74)&~1U); break;
        case 12: { for(i32 i=0;i!=5;++i) ReleaseSlot(reinterpret_cast<void **>(s+0x40)+i); u8 *n=LoadTimelineStream(g_TimelineGateState,reinterpret_cast<const char *>(r+4)); if(!n)return -1; memset(s,0,0xec); *reinterpret_cast<u8 **>(s+0x54)=n+R32(n+4); ResetTimer(s+4,0); ResetTimer(s+0x18,0); ResetTimer(s+0x2c,0); W32(s+0x74,2); W32(s+0x7c,0xffffff); break; }
        case 13: CreateAsciiOverlayContext(0,R32(r+4),0,0,0,49); break;
        case 14: CreateAsciiOverlayContext(5,R32(r+4),0,0,0,49); break;
        case 15: case 16: case 17: if(g_TimelinePhase==r[2]-14) { const i32 dst=I32(r+4); void **out=reinterpret_cast<void **>(s+0x90)+dst; SetTimelineObjectPhase(out); i32 *created=SpawnSetupEffectVmListABack(I32(r+12),15); *out=reinterpret_cast<void*>(*created); } break;
        default: break;
        }
        AdvanceRecord(s);
    }
    AdvanceTimer(s + 0x18); return 0;
}

i32 AdvanceTimelineRecordController(void *memory) {
    u8 *const state = static_cast<u8 *>(memory);
    if (ExecuteTimelineRecords(state) != 0)
        return 1;
    AdvanceTimer(state);
    return 0;
}
} // namespace th10
