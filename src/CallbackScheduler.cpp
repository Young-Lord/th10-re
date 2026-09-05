#include "CallbackScheduler.hpp"
#include "ThreadControl.hpp"

namespace th10 {

namespace {

extern ChainElem *AllocateChainElem(u32 bytes); // TH10 0x00452493
extern void FreeChainElem(void *element); // TH10 0x004524a1
extern Win32CriticalSection g_CallbackSchedulerLock; // TH10 DAT_00492274
extern u8 g_CallbackSchedulerActivityDepth; // TH10 DAT_0049231c
extern ThreadControl g_MainChainSecondaryControl; // TH10 DAT_00492254

extern "C" void TH10_STDCALL EnterCriticalSection(void *critical_section);
extern "C" void TH10_STDCALL LeaveCriticalSection(void *critical_section);

ChainLink *FindInsertionLink(ChainLink *sentinel, i32 priority)
{
    ChainLink *link = sentinel;
    while (link->next != 0 && link->next->owner->priority < priority)
        link = link->next;
    return link;
}

void InsertAfter(ChainLink *before, ChainLink *link)
{
    link->previous = before;
    link->next = before->next;
    if (before->next != 0)
        before->next->previous = link;
    before->next = link;
}

bool RemoveFromChain(ChainLink *sentinel, ChainElem *element);
void RemoveLocked(CallbackScheduler *scheduler, ChainElem *element);

void DrainSchedulerChain(CallbackScheduler *scheduler, ChainElem *sentinel)
{
    for (ChainLink *link = sentinel->link.next; link != 0; ) {
        ChainElem *const element = link->owner;
        ChainLink *const next = link->next;
        EnterCriticalSection(&g_CallbackSchedulerLock);
        ++g_CallbackSchedulerActivityDepth;
        RemoveLocked(scheduler, element);
        LeaveCriticalSection(&g_CallbackSchedulerLock);
        --g_CallbackSchedulerActivityDepth;
        link = next;
    }
}

void RemoveLocked(CallbackScheduler *scheduler, ChainElem *element)
{
    if (!RemoveFromChain(&scheduler->calculation_sentinel.link, element) &&
        !RemoveFromChain(&scheduler->draw_sentinel.link, element)) {
        return;
    }

    if ((element->flags & ChainElemFlag_OwnedByScheduler) != 0) {
        element->callback = 0;
        element->registration_hook = 0;
        element->calculation_followup = 0;
        FreeChainElem(element);
    }
}

i32 Register(ChainLink *sentinel, ChainElem *element, i32 priority)
{
    i32 hook_result = 0;
    if (element->registration_hook != 0) {
        hook_result = element->registration_hook(element->arg);
        element->registration_hook = 0;
    }

    EnterCriticalSection(&g_CallbackSchedulerLock);
    element->priority = priority;
    InsertAfter(FindInsertionLink(sentinel, priority), &element->link);
    LeaveCriticalSection(&g_CallbackSchedulerLock);
    return hook_result;
}

bool RemoveFromChain(ChainLink *sentinel, ChainElem *element)
{
    for (ChainLink *link = sentinel->next; link != 0; link = link->next) {
        if (link->owner != element)
            continue;

        if (element->callback == 0)
            return false;

        if (link->previous != 0)
            link->previous->next = link->next;
        if (link->next != 0)
            link->next->previous = link->previous;

        link->next = 0;
        link->previous = 0;
        element->callback = 0;
        return true;
    }
    return false;
}

} // namespace

CallbackScheduler *ConstructCallbackScheduler(void *storage)
{
    u32 *const words = static_cast<u32 *>(storage);
    words[2] = 0;
    words[3] = 0;
    words[4] = 0;
    words[0] = 0;
    words[1] &= ~1U;
    words[5] = reinterpret_cast<u32>(storage);
    words[6] = 0;
    words[7] = 0;

    words[10] &= ~1U;
    words[11] = 0;
    words[12] = 0;
    words[13] = 0;
    words[9] = 0;
    words[14] = reinterpret_cast<u32>(words + 9);
    words[15] = 0;
    words[16] = 0;
    return static_cast<CallbackScheduler *>(storage);
}

void DestroyCallbackSchedulerInPlace(CallbackScheduler *scheduler)
{
    StopThreadControl(&g_MainChainSecondaryControl);
    DrainSchedulerChain(scheduler, &scheduler->calculation_sentinel);
    DrainSchedulerChain(scheduler, &scheduler->draw_sentinel);
    scheduler->draw_sentinel.callback = 0;
    scheduler->draw_sentinel.registration_hook = 0;
    scheduler->draw_sentinel.calculation_followup = 0;
    scheduler->calculation_sentinel.callback = 0;
    scheduler->calculation_sentinel.registration_hook = 0;
    scheduler->calculation_sentinel.calculation_followup = 0;
}

// TH10 0x00449ed0, expressed as a normal C++ allocation boundary. The native
// allocator's null result is immediately dereferenced by the original, so no
// nullable factory contract is introduced here.
ChainElem *CallbackSchedulerApi::Create(ChainCallback callback)
{
    ChainElem *element = AllocateChainElem(sizeof(ChainElem));
    element->priority = 0;
    element->flags = ChainElemFlag_OwnedByScheduler;
    element->callback = callback;
    element->registration_hook = 0;
    element->calculation_followup = 0;
    element->link.owner = element;
    element->link.next = 0;
    element->link.previous = 0;
    return element;
}

// TH10 0x00449ae0.
i32 CallbackSchedulerApi::AddToCalculationChain(CallbackScheduler *scheduler,
                                                 ChainElem *element,
                                                 i32 priority)
{
    return Register(&scheduler->calculation_sentinel.link, element, priority);
}

// TH10 0x00449b70.
i32 CallbackSchedulerApi::AddToDrawChain(CallbackScheduler *scheduler,
                                          ChainElem *element,
                                          i32 priority)
{
    return Register(&scheduler->draw_sentinel.link, element, priority);
}

// TH10 0x00449f60. The caller owns synchronization exactly as in the original
// teardown path; owning records are freed after a successful unlink.
void CallbackSchedulerApi::Remove(CallbackScheduler *scheduler,
                                  ChainElem *element)
{
    RemoveLocked(scheduler, element);
}

void CallbackSchedulerApi::RemoveSynchronized(CallbackScheduler *scheduler,
                                              ChainElem *element)
{
    EnterCriticalSection(&g_CallbackSchedulerLock);
    ++g_CallbackSchedulerActivityDepth;
    RemoveLocked(scheduler, element);
    LeaveCriticalSection(&g_CallbackSchedulerLock);
    --g_CallbackSchedulerActivityDepth;
}

i32 CallbackSchedulerApi::DispatchCalculation(CallbackScheduler *scheduler)
{
    i32 count = 0;
    EnterCriticalSection(&g_CallbackSchedulerLock);
    ChainLink *link = scheduler->calculation_sentinel.link.next;

    while (link != 0) {
        ChainElem *element = link->owner;
        ChainLink *next = link->next;
        i32 result = 1;

        if (element->callback != 0 &&
            (element->flags & ChainElemFlag_Enabled) != 0) {
            LeaveCriticalSection(&g_CallbackSchedulerLock);
            result = element->callback(element->arg);
            EnterCriticalSection(&g_CallbackSchedulerLock);

            while (result == 2 &&
                   (element->flags & ChainElemFlag_Enabled) != 0) {
                LeaveCriticalSection(&g_CallbackSchedulerLock);
                result = element->callback(element->arg);
                EnterCriticalSection(&g_CallbackSchedulerLock);
            }

            if (result == 3) {
                LeaveCriticalSection(&g_CallbackSchedulerLock);
                return 1;
            }
            if (result == 4) {
                LeaveCriticalSection(&g_CallbackSchedulerLock);
                return 0;
            }
            if (result == 5) {
                LeaveCriticalSection(&g_CallbackSchedulerLock);
                return -1;
            }
            if (result == 6) {
                count = 0;
                link = scheduler->calculation_sentinel.link.next;
                continue;
            }
            if (result == 0)
                RemoveLocked(scheduler, element);
            else if (result == 7 && element->calculation_followup != 0)
                element->calculation_followup(element->arg);
        }

        count++;
        link = next;
    }
    LeaveCriticalSection(&g_CallbackSchedulerLock);
    return count;
}

i32 CallbackSchedulerApi::DispatchDraw(CallbackScheduler *scheduler)
{
    i32 count = 0;
    EnterCriticalSection(&g_CallbackSchedulerLock);
    ChainLink *link = scheduler->draw_sentinel.link.next;

    while (link != 0) {
        ChainElem *element = link->owner;
        ChainLink *next = link->next;
        i32 result = 1;

        if (element->callback != 0 &&
            (element->flags & ChainElemFlag_Enabled) != 0) {
            LeaveCriticalSection(&g_CallbackSchedulerLock);
            result = element->callback(element->arg);
            EnterCriticalSection(&g_CallbackSchedulerLock);

            while (result == 2 &&
                   (element->flags & ChainElemFlag_Enabled) != 0) {
                LeaveCriticalSection(&g_CallbackSchedulerLock);
                result = element->callback(element->arg);
                EnterCriticalSection(&g_CallbackSchedulerLock);
            }

            if (result == 3) {
                LeaveCriticalSection(&g_CallbackSchedulerLock);
                return 1;
            }
            if (result == 4) {
                LeaveCriticalSection(&g_CallbackSchedulerLock);
                return 0;
            }
            if (result == 5) {
                LeaveCriticalSection(&g_CallbackSchedulerLock);
                return -1;
            }
            if (result == 0)
                RemoveLocked(scheduler, element);
        }

        count++;
        link = next;
    }
    LeaveCriticalSection(&g_CallbackSchedulerLock);
    return count;
}

} // namespace th10
