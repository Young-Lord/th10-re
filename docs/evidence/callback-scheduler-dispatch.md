# Callback Scheduler Dispatcher Evidence

## Scope

This document records the two scheduler dispatchers in `resources/th10.exe`:
`0x00449c00` (calculation queue) and `0x00449d40` (draw queue).  It is a
source boundary for a readable C++ implementation, not evidence that either
entry has a conventional member-function ABI.

The directly inspected companion removal routine is `0x00449f60`.  Scheduler
and record/link layouts use the terminology established in
`callback-scheduler.md`: the calculation sentinel link is at scheduler `+0x14`
and its first active link is stored at `+0x18`; the corresponding draw offsets
are `+0x38` and `+0x3c`.

## Dispatcher ABI and lock discipline

Both dispatchers have this public entry ABI:

~~~text
input:  [ESP+4] = CallbackScheduler *
output: EAX = processed count or a callback-directed terminal value
stack:  ret 4
~~~

They acquire the global critical section at `0x00492274` before walking a
link.  For each link, they save `link->next` before releasing that lock, then
invoke the primary callback with `ECX = record->owner_context` and no explicit
stack argument.  The dispatcher reacquires the lock before interpreting the
callback result.  Therefore a primary callback runs outside the scheduler
lock, while the following operations run under it:

- enabled-flag retest for primary result `2`;
- unlink/free for primary result `0`;
- calculation-only `record+0x10` follow-up for result `7`.

The continuation uses the pre-callback saved next-link pointer.  It remains
valid for ordinary self-removal of the current record, but the code has no
revalidation mechanism if arbitrary callback code removes or frees that next
record.

Each normal traversal increments the local count exactly once, including a
record whose primary callback is null or disabled.  A retry (`2`) does not
increment until it stops retrying and traversal resumes.

## Calculation dispatch: 0x00449c00

The calculation dispatcher begins from `scheduler+0x18`.  It calls a non-null
primary callback at record `+0x08` only if flags bit `0x2` is set.  Its jump
table is at `0x00449d14`.

| Primary callback EAX | Exact action | Final / next EAX |
| ---: | --- | --- |
| `0` | Call `0x00449f60(record, scheduler)` while locked, then advance to the saved next link. | count + 1 |
| `1` | Advance to the saved next link. | count + 1 |
| `2` | Retest bit `0x2`; if set, unlock and invoke the same primary again. If clear, advance to saved next. | count + 1 after retry ends |
| `3` | End dispatch immediately. | `1` |
| `4` | End dispatch immediately. | `0` |
| `5` | End dispatch immediately. | `-1` |
| `6` | Set count to zero and restart at `scheduler+0x18`. It does not continue from the saved next link. | count from the restarted scan |
| `7` | If record `+0x10` is non-null, call it with `ECX = owner_context` while locked; then advance. | count + 1 |
| other unsigned value | Advance to the saved next link. | count + 1 |

A null or disabled primary does not select a jump-table result: it simply
advances and increments.  Result `2` which terminates because the callback
cleared bit `0x2` likewise does not call the calculation follow-up.  The
follow-up's return value is ignored.

Because result `6` reloads the first link, records encountered before that
result can be counted again.  Consequently the final return is not a unique
record count when callbacks request restarts.  Repeated result `6` can also
make this dispatcher nonterminating.

## Draw dispatch: 0x00449d40

The draw dispatcher begins from `scheduler+0x3c`.  It has the same primary
callback ABI and flag gate but does not read record `+0x0c` or `+0x10`.  Its
jump table is at `0x00449e34`.

| Primary callback EAX | Exact action | Final / next EAX |
| ---: | --- | --- |
| `0` | Call `0x00449f60(record, scheduler)` while locked, then advance to the saved next link. | count + 1 |
| `1` | Advance to the saved next link. | count + 1 |
| `2` | Retest bit `0x2`; if set, unlock and invoke the same primary again. If clear, advance to saved next. | count + 1 after retry ends |
| `3` | End dispatch immediately. | `1` |
| `4` | End dispatch immediately. | `0` |
| `5` | End dispatch immediately. | `-1` |
| other unsigned value | Advance to the saved next link. | count + 1 |

There is no draw equivalent of calculation result `6`; unsigned results above
`5` are normal continuation.  As with calculation, null or disabled records
still count as traversed.

## Removal behavior used by result 0

`0x00449f60` is a mixed-register helper, not ordinary `__thiscall`:

~~~text
input:  ECX = CallbackRecord *record
        EDX = CallbackScheduler *scheduler
output: no caller-observed EAX contract
stack:  plain ret
~~~

It linearly searches the calculation chain first and the draw chain second,
comparing each link's owner field with `ECX`.  A found record is removed only
when `record+0x08` is non-null.  The helper repairs neighboring link pointers,
clears this record's link next/previous fields, and clears `record+0x08`.

If flags bit `0x1` is set, it additionally clears `+0x08`, `+0x0c`, and `+0x10`
and passes the record to `0x004524a1` for release.  It neither invokes a
registration hook nor a calculation follow-up.  A record absent from both
queues, or a found record whose primary pointer is already null, is unchanged
and not freed.  Thus a second removal after a successful first removal is
inert, and non-owning records are unlinked without being deallocated.

## C++ reconstruction boundary

Represent the algorithms as separate `DispatchCalculation()` and
`DispatchDraw()` methods under one scheduler lock, but keep callback execution
outside that lock.  A semantic callback result enum should retain the original
numeric values, especially calculation-only `6 = restart from head`; treating
it as a normal `continue` changes both ordering and final counts.  Implement
removal as an internal mixed-ABI wrapper or a normal C++ helper only after
separating its ABI thunk from the list/ownership semantics above.

## Verification

- `resources/th10.exe`: disassembly `0x00449c00-0x00449e4f`.
- `resources/th10.exe`: jump tables `0x00449d14` and `0x00449e34`.
- `resources/th10.exe`: removal routine `0x00449f60-0x00449fd1`.
- `docs/evidence/callback-scheduler.md` for shared record and sentinel layout.

## Reconstruction status (2026-09-14)

Both dispatchers are implemented in src/CallbackScheduler.cpp as
`CallbackSchedulerApi::DispatchCalculation` (0x00449c00) and
`CallbackSchedulerApi::DispatchDraw` (0x00449d40), and 0x00449f60 as the
shared `RemoveLocked` helper. The C++ bodies were re-verified against
the jump tables at 0x449d14 (calculation, 8 entries: 0 = remove,
1 = continue, 2 = retest loop, 3/4/5 = return 1/0/-1, 6 = restart from
scheduler+0x18, 7 = follow-up) and 0x449e34 (draw, 6 entries).
