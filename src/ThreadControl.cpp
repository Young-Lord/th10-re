#include "ThreadControl.hpp"

namespace th10 {

namespace {

const u32 kWaitTimeout = 0x102;

extern u32 WaitForThreadControlHandle(void *handle, u32 milliseconds);
extern void SleepMilliseconds(u32 milliseconds);
extern void CloseThreadControlHandle(void *handle);

} // namespace

void StopThreadControl(ThreadControl *control)
{
    if (control->thread_handle == 0)
        return;

    control->stop_requested = 1;
    control->field_0010 = 0;
    u32 wait_result = WaitForThreadControlHandle(control->thread_handle, 200);
    while (wait_result == kWaitTimeout) {
        SleepMilliseconds(1);
        wait_result = WaitForThreadControlHandle(control->thread_handle, 200);
    }

    CloseThreadControlHandle(control->thread_handle);
    control->thread_handle = 0;
    control->thread_entry = 0;
}

} // namespace th10
