#pragma once

#include <stddef.h>

#include "Th10Types.hpp"

namespace th10 {

struct MidiTimerPartial {
    void *vtable;
    u32 timer_id;
    u32 period;
};

typedef char AssertMidiTimerPeriodOffset[
    offsetof(MidiTimerPartial, period) == 0x8 ? 1 : -1];

// Semantic body of the stop fragment embedded in TH10 0x00420270.
void StopMidiTimer(MidiTimerPartial *timer);
// TH10 0x004203c0 semantic body. Native ESI/plain-ret ABI needs a thunk.
void DestroyMidiTimerInPlace(MidiTimerPartial *timer);
// TH10 0x004203a0 semantic body. Native EAX plus deleting-flag/ret-4 ABI
// remains a thunk boundary.
MidiTimerPartial *DestroyMidiTimer(MidiTimerPartial *timer, u32 flags);

} // namespace th10
