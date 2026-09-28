// TH10 effect command scheduler (0x00449a20-0x0044a030 + 0x0043cb00).
//
// The scheduler root (TH10 DAT_00491be4) owns two sorted doubly-linked
// lists headed at root+0x14 and root+0x38. Each list anchor is a node-like
// {key, prev-link, next-link} triple: the anchor's +4 points at the first
// node's link slot (node+0x14 for list A / node+0x1c for list B), and the
// nodes carry their key at +0x00. Node records are 0x24 bytes:
//   +0x00 sort key, +0x04 flags (bit0 allocated, bit1 queued),
//   +0x08..+0x10 payload words, +0x14/+0x18 list-A prev/next link slots,
//   +0x1c/+0x20 list-B link slots.
// A node's constructor pointer (+0x0c) is invoked once by the queueing
// caller through word +0x20 and then cleared. The nesting counter
// DAT_0049231c guards the shared critical section DAT_00492274.
#include <stdlib.h>

#include "Th10Types.hpp"
#include "Th10Platform.hpp"

namespace th10 {

u32 *AllocateSchedulerNodeStackAbi(u32 timer_word);

namespace {

struct CriticalSectionBox {
    u8 storage[0x18];
};

extern CriticalSectionBox g_SchedulerLock; // TH10 CriticalSection 0x492274
extern u8 g_SchedulerNesting;              // TH10 byte_49231c
extern u32 *g_SchedulerRoot;               // TH10 DAT_00491be4

extern "C" void TH10_STDCALL EnterCriticalSection(void *critical_section);
extern "C" void TH10_STDCALL LeaveCriticalSection(void *critical_section);

extern void *AllocateArchiveVector(u32 bytes); // TH10 0x00452493
extern void FreeArchiveVector(void *pointer);  // TH10 0x004524a1

u32 ReadU32At(const void *base, u32 offset)
{
    return *reinterpret_cast<const u32 *>(
        static_cast<const u8 *>(base) + offset);
}

void WriteU32At(void *base, u32 offset, u32 value)
{
    *reinterpret_cast<u32 *>(static_cast<u8 *>(base) + offset) = value;
}

} // namespace

// TH10 0x00449a20. Native thiscall: returns the previous tick value.
u32 AdvanceSchedulerTickEcxAbi(u32 *counter)
{
    const u32 previous = *counter;
    *counter = previous + 1U;
    return previous;
}

// Shared initialization of the 0x24-byte node record.
static void InitSchedulerNodeWords(u32 *node)
{
    const u32 flags = node[1];
    node[2] = 0;
    node[3] = 0;
    node[4] = 0;
    node[0] = 0;
    node[1] = flags & 0xfffffffeU;
    node[5] = reinterpret_cast<u32>(node);
    node[6] = 0;
    node[7] = 0;
}

// TH10 0x00449a50. Native EAX = node.
void InitializeSchedulerNodeEaxAbi(u32 *node)
{
    InitSchedulerNodeWords(node);
}

// TH10 0x00449a70. Variant seeding ECX into +0x08.
void InitializeSchedulerNodeWithTimerEaxCcxAbi(u32 *node, u32 timer_word)
{
    InitSchedulerNodeWords(node);
    node[2] = timer_word;
}

// TH10 0x00449a90. Clears only the payload words +0x08..+0x10.
void ClearSchedulerNodeLinksEaxAbi(u32 *node)
{
    node[2] = 0;
    node[3] = 0;
    node[4] = 0;
}

// Sorted insertion shared by 0x00449ae0 (+0x14 list) and 0x00449b70
// (+0x38 list). Native EDI = key, ESI = node, stack = root (ret 4).
static void InsertSortedShared(u32 key, u32 *node, u32 root, u32 head_offset)
{
    typedef i32 (TH10_STDCALL *NodeCtorFn)(void *);
    i32 ctor_result = 0;
    NodeCtorFn ctor = reinterpret_cast<NodeCtorFn>(node[3]);
    if (ctor != 0) {
        ctor_result = ctor(reinterpret_cast<void *>(node[8]));
        node[3] = 0;
    }

    EnterCriticalSection(&g_SchedulerLock);
    ++g_SchedulerNesting;

    u32 link = root + head_offset;
    node[0] = key;
    if (ReadU32At(reinterpret_cast<const void *>(root),
                  head_offset + 4U) != 0U) {
        u32 candidate;
        do {
            candidate = ReadU32At(reinterpret_cast<const void *>(link), 4U);
            if (ReadU32At(reinterpret_cast<const void *>(candidate), 0U)
                    >= key)
                break;
            link = candidate;
        } while (ReadU32At(reinterpret_cast<const void *>(candidate), 4U)
                 != 0U);
    }
    const u32 next = ReadU32At(reinterpret_cast<const void *>(link), 4U);
    node[5] = link;
    if (next != 0U) {
        node[6] = next;
        WriteU32At(reinterpret_cast<void *>(next), 8U,
                   reinterpret_cast<u32>(node) + head_offset + 0x14U
                       - 0x14U);
    }
    WriteU32At(reinterpret_cast<void *>(link), 4U,
               reinterpret_cast<u32>(node) + head_offset);
    node[7] = link;

    LeaveCriticalSection(&g_SchedulerLock);
    --g_SchedulerNesting;
    (void)ctor_result;
}

// TH10 0x00449ae0. List-A insert (head root+0x14).
void InsertSchedulerNodeSortedListAEdiEsiAbi(u32 key, u32 *node, u32 root)
{
    InsertSortedShared(key, node, root, 0x14U);
}

// TH10 0x00449b70. List-B insert (head root+0x38).
void InsertSchedulerNodeSortedListBEdiEsiAbi(u32 key, u32 *node, u32 root)
{
    InsertSortedShared(key, node, root, 0x38U);
}

// TH10 0x0044a000. Native EAX = timer word, EDI = sort key, stack =
// payload (ret 4). Allocates a queued node and inserts into list A of
// DAT_00491be4.
u32 *QueueSchedulerCommandListAEaxEdiAbi(u32 timer_word, u32 sort_key,
                                         u32 payload)
{
    u32 *node = AllocateSchedulerNodeStackAbi(timer_word);
    node[8] = payload;
    node[1] |= 2U;
    InsertSchedulerNodeSortedListAEdiEsiAbi(sort_key, node,
        reinterpret_cast<u32>(g_SchedulerRoot));
    return node;
}

// TH10 0x0044a030. List-B twin of 0x0044a000.
u32 *QueueSchedulerCommandListBEaxEdiAbi(u32 timer_word, u32 sort_key,
                                         u32 payload)
{
    u32 *node = AllocateSchedulerNodeStackAbi(timer_word);
    node[8] = payload;
    node[1] |= 2U;
    InsertSchedulerNodeSortedListBEdiEsiAbi(sort_key, node,
        reinterpret_cast<u32>(g_SchedulerRoot));
    return node;
}

// TH10 0x00449ed0. Native stdcall: argument = timer word. Allocates the
// 0x24-byte node, initializes it, seeds +0x08 and sets the allocated bit.
u32 *AllocateSchedulerNodeStackAbi(u32 timer_word)
{
    u32 *node = static_cast<u32 *>(AllocateArchiveVector(0x24U));
    if (node != 0)
        InitSchedulerNodeWords(node);
    node[3] = 0;
    node[4] = 0;
    node[1] |= 1U;
    node[2] = timer_word;
    return node;
}

// TH10 0x00449f60. Native ECX = node, EDX = root. Searches list A then
// list B for the node's anchor, unlinks it and, when the allocated bit
// (bit 0 of +0x04) is clear, zeroes +0x08..+0x10 and frees the record
// (native quirk: the words are cleared in both branches).
void DetachSchedulerNodeEcxEdxAbi(void *node, u32 root)
{
    if (node == 0)
        return;

    u32 link = root + 0x14U;                 // list-A anchor
    while (*reinterpret_cast<void **>(link) != node) {
        link = ReadU32At(reinterpret_cast<const void *>(link), 4U);
        if (link == 0U) {
            // List A exhausted; restart on list B (root+0x38).
            link = root + 0x38U;
            while (*reinterpret_cast<void **>(link) != node) {
                link = ReadU32At(reinterpret_cast<const void *>(link), 4U);
                if (link == 0U)
                    return;
            }
            break;
        }
    }

    const u32 next = ReadU32At(reinterpret_cast<const void *>(link), 8U);
    if (next == 0U)
        return;
    const u32 prev = ReadU32At(reinterpret_cast<const void *>(link), 4U);
    if (prev != 0U)
        WriteU32At(reinterpret_cast<void *>(prev), 8U, next);
    WriteU32At(reinterpret_cast<void *>(next), 4U, prev);
    WriteU32At(reinterpret_cast<void *>(link), 4U, 0U);
    WriteU32At(reinterpret_cast<void *>(link), 8U, 0U);

    u8 *record = static_cast<u8 *>(node);
    *reinterpret_cast<u32 *>(record + 8U) = 0;
    if ((record[4] & 1U) == 0U) {
        *reinterpret_cast<u32 *>(record + 8U) = 0;
        *reinterpret_cast<u32 *>(record + 12U) = 0;
        *reinterpret_cast<u32 *>(record + 16U) = 0;
        FreeArchiveVector(node);
    }
}

// TH10 0x00449e50. Native EAX = root. Walks the list-A chain from
// root+0x18 and detaches every node through 0x00449f60.
void SweepSchedulerListEaxAbi(u32 root)
{
    u32 link = ReadU32At(reinterpret_cast<const void *>(root), 0x18U);
    while (link != 0U) {
        void *node = reinterpret_cast<void *>(
            ReadU32At(reinterpret_cast<const void *>(link), 0U));
        const u32 next = ReadU32At(reinterpret_cast<const void *>(link), 4U);
        if (node != 0)
            DetachSchedulerNodeEcxEdxAbi(node, root);
        link = next;
    }
}

// TH10 0x0043cb00. Native stdcall: argument = holder {+0x08, +0x0c}.
// Detaches both nodes through 0x00449f60 under the scheduler lock.
void ReleaseSchedulerNodePairStdcallAbi(u32 holder)
{
    void *first = reinterpret_cast<void *>(
        ReadU32At(reinterpret_cast<const void *>(holder), 8U));
    if (first != 0) {
        EnterCriticalSection(&g_SchedulerLock);
        ++g_SchedulerNesting;
        DetachSchedulerNodeEcxEdxAbi(first, holder);
        LeaveCriticalSection(&g_SchedulerLock);
        --g_SchedulerNesting;
    }
    void *second = reinterpret_cast<void *>(
        ReadU32At(reinterpret_cast<const void *>(holder), 12U));
    if (second != 0) {
        EnterCriticalSection(&g_SchedulerLock);
        ++g_SchedulerNesting;
        DetachSchedulerNodeEcxEdxAbi(second, holder);
        LeaveCriticalSection(&g_SchedulerLock);
        --g_SchedulerNesting;
    }
}

} // namespace th10
