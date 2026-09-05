#include "MidiTimer.hpp"

namespace th10 {

namespace {

extern void *g_MidiTimerBaseVtable; // TH10 DAT_0046f810
extern void TimeKillEvent(u32 timer_id);
extern void TimeEndPeriod(u32 period);
extern void FreeMidiTimer(void *timer); // TH10 0x004524a1

} // namespace

void StopMidiTimer(MidiTimerPartial *timer)
{
    if (timer->timer_id != 0)
        TimeKillEvent(timer->timer_id);
    TimeEndPeriod(timer->period);
    timer->timer_id = 0;
}

void DestroyMidiTimerInPlace(MidiTimerPartial *timer)
{
    timer->vtable = g_MidiTimerBaseVtable;
    StopMidiTimer(timer);
    TimeEndPeriod(timer->period);
}

MidiTimerPartial *DestroyMidiTimer(MidiTimerPartial *timer, u32 flags)
{
    DestroyMidiTimerInPlace(timer);
    if ((flags & 1U) != 0)
        FreeMidiTimer(timer);
    return timer;
}

} // namespace th10
