# Callback Scheduler Evidence

## Scope and confidence

This document covers the TH10 callback scheduler in resources/th10.exe. The
directly inspected entries are 0x00449ed0, 0x00449ae0, 0x00449b70,
0x00449c00, 0x00449d40, and 0x00449f60. Names below are descriptive only;
the executable has no class names or RTTI for this code.

Node size, each listed offset, register input, return convention, list
algorithm, and result-code table are direct instruction-level evidence.
"calculation" and "draw" derive from call sites at 0x0043942f before frame
calculation and 0x004394d8 during rendering, not from binary names.

## Layout

0x00449ed0 asks allocator 0x00452493 for 0x24 bytes. The scheduler constructor
at 0x00449aa0 follows a 0x48-byte allocation and initializes two adjacent
0x24-byte sentinels. The following is conservative but directly supported:

~~~cpp
struct CallbackLink {
    CallbackRecord *owner;     // +0x00 relative to link; sentinel points to itself
    CallbackLink *next;        // +0x04
    CallbackLink *previous;    // +0x08
};

struct CallbackRecord {        // sizeof = 0x24
    int priority;              // +0x00, ordering key while linked
    unsigned int flags;        // +0x04, bit 0 ownership; bit 1 enabled
    int (__thiscall *callback)(void *owner);           // +0x08
    int (__thiscall *registration_hook)(void *owner);  // +0x0c
    int (__thiscall *calc_followup)(void *owner);      // +0x10
    CallbackLink link;         // +0x14
    void *owner_context;       // +0x20
};

struct CallbackScheduler {    // sizeof = 0x48
    CallbackRecord calculation_sentinel; // +0x00, first active node at +0x18
    CallbackRecord draw_sentinel;        // +0x24, first active node at +0x3c
};
~~~

The three callable declarations express the observed direct-call ABI, not
formal C++ type recovery. Every invocation loads ECX = record+0x20 and
indirectly calls with no stack arguments. A target may ignore ECX, as
0x004200c0 does. Nothing here proves the type or ownership of owner_context.

For an active record, CallbackLink::owner is its containing record. For a
sentinel it is the sentinel itself. The constructor makes both sentinel links
{ self, null, null } and clears their remaining fields. These functions use the
global critical section at 0x00492274, via imported EnterCriticalSection at
0x004660b0 and LeaveCriticalSection at 0x004660b4; no scheduler-local lock is
evidenced.

| Flag bit | Directly observed effect | Confidence |
| --- | --- | --- |
| 0x1 | Set by 0x00449ed0; after successful unlink, 0x00449f60 frees the record through 0x004524a1. | confirmed allocation ownership |
| 0x2 | Dispatchers call +0x08 only while set. Registration callers explicitly set or clear it. | confirmed execution gate; semantic label is descriptive |

0x00449ed0 clears only bit 0x1 before setting it. It does not initialize bit
0x2; callers establish the desired state.

## Allocation: 0x00449ed0

~~~text
input:  [ESP+4] = callback target for record +0x08
output: EAX = allocated record
stack:  callee pops one argument (ret 4)
~~~

The function allocates 0x24 bytes; zeroes +0x00, +0x08, +0x0c, +0x10, +0x18,
and +0x1c; clears then sets ownership bit 0x1; sets the link at +0x14 to
{ self, null, null }; and writes the supplied target to +0x08. Fields +0x0c,
+0x10, and +0x20 remain caller-configurable.

The allocator-null branch sets EAX = 0, but the common tail immediately writes
the supplied target to [EAX+0x08]. Thus this binary has no usable
allocation-failure return contract: a null allocation dereferences address
eight before it can return. It must not be represented as a nullable factory
without an explicit behavioral change.

## Registration: 0x00449ae0 and 0x00449b70

Both helpers share this nonstandard ABI:

~~~text
input:  ESI = CallbackRecord *record
        EDI = signed 32-bit priority
        [ESP+4] = CallbackScheduler *scheduler
output: EAX = registration_hook result, or 0 when record +0x0c was null
stack:  callee pops scheduler (ret 4)
~~~

They do not obtain the record or priority from the stack, and do not preserve
EDI; callers load both registers immediately before the call. 0x00449ae0
inserts through the first sentinel link at scheduler+0x14; 0x00449b70 uses the
second at scheduler+0x38.

Before acquiring the lock, a helper calls non-null record+0x0c with
ECX = record+0x20, saves its EAX result, and clears record+0x0c. It then locks
and inserts regardless of that result, finally returning it. Consequently a
nonzero hook result is caller-visible but does not establish an insertion
failure or rollback.

Insertion walks while the current priority is strictly less than EDI, then
inserts before the first priority greater than or equal to EDI. Ordering is
ascending by signed priority. Equal-priority records are newest-first, not FIFO.
There is no allocation, duplicate, or capacity failure path in these helpers.

## Execution: 0x00449c00 and 0x00449d40

Both dispatcher entries use this stack ABI:

~~~text
input:  [ESP+4] = CallbackScheduler *scheduler
output: EAX = processed count, or callback-directed terminal value
stack:  callee pops scheduler (ret 4)
~~~

They lock while locating a node, save its next link, unlock, invoke the
callback, and relock before processing its result. User callback code therefore
does not execute under the scheduler lock and can use APIs that acquire it. The
saved next link also supports removal of the current record by its callback.

0x00449c00 begins at scheduler+0x18. A non-null primary callback at +0x08
executes only with flag 0x2 set, receiving ECX = record+0x20.

| Primary result | Effect | Dispatcher EAX |
| ---: | --- | --- |
| 0 | Remove current record through 0x00449f60, then continue. | count plus one |
| 1 | Continue. | count plus one |
| 2 | Reinvoke current primary while bit 0x2 remains set; otherwise continue. | count plus one after loop |
| 3 | Stop. | 1 |
| 4 | Stop. | 0 |
| 5 | Stop. | -1 |
| 6 | Reset accumulated count, then continue at next record. | later count |
| 7 | Invoke non-null +0x10, then continue. | count plus one |
| other | Continue. | count plus one |

For calculation dispatch, a null or disabled primary callback increments the
count and does not invoke +0x10. Result 2 which ends because the gate was
cleared likewise does not invoke +0x10. The semantic purpose of this
calculation-only, result-7 follow-up remains uncertain.

0x00449d40 begins at scheduler+0x3c. It uses the same primary ABI and flag gate
but never reads +0x0c or +0x10 during dispatch.

| Primary result | Effect | Dispatcher EAX |
| ---: | --- | --- |
| 0 | Remove current record, then continue. | count plus one |
| 1 | Continue. | count plus one |
| 2 | Reinvoke current primary while bit 0x2 remains set; otherwise continue. | count plus one after loop |
| 3 | Stop. | 1 |
| 4 | Stop. | 0 |
| 5 | Stop. | -1 |
| other | Continue. | count plus one |

These terminal values are callback protocol values, not allocator or insertion
errors.

## Removal and ownership: 0x00449f60

~~~text
input:  ECX = CallbackRecord *record
        EDX = CallbackScheduler *scheduler
output: no defined EAX result used by direct callers
stack:  plain ret; no stack arguments
~~~

This is not ordinary __thiscall: EDX is a second mandatory input. It searches
the calculation chain first and the draw chain second for a link whose owner is
ECX. If found and record+0x08 is non-null, it unlinks the record, clears link
next and previous, and clears record+0x08.

When flag 0x1 is set, it additionally clears +0x08, +0x0c, and +0x10, then
passes the record to 0x004524a1 for deallocation. It never calls either
secondary callback. A record not found in either queue, or a found record with
null +0x08, is neither unlinked nor freed. The first successful owning removal
clears +0x08, making later attempts inert.

Ownership is deliberately split: records created by 0x00449ed0 carry bit 0x1
and are freed by removal; a manually supplied record can clear that bit and be
unlinked without being freed. These functions do not establish ownership of
owner_context or callback target objects.

## Verification sources

- resources/th10.exe, objdump -D -Mintel, ranges 0x00449aa0-0x0044a028 and
  0x00438ba3-0x00438bc3.
- Import table entries 0x004660b0 = EnterCriticalSection and
  0x004660b4 = LeaveCriticalSection.
- Dispatcher call sites 0x0043942f and 0x004394d8; representative
  registration/removal sites including 0x00420470, 0x0041fb50, and
  0x00401184-0x00401337.
