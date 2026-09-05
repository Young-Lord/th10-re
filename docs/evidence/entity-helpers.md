# TH10 Entity Helpers, Playfield Cull, And Pool Allocation

Module: `src/EntityHelpers.cpp/.hpp`. All list walks target the two
manager lists at `manager+0x72dad4` / `+0x72dadc` (nodes
`{entity, next}`; entity id at entity+0). Child propagation runs over the
list at entity+0x14 while the container field at entity+0x18 is zero.

| Address | Semantic name | Contract |
|---|---|---|
| `0x4491c0` | `FindEntityEdxStackAbi` | EDX = manager; id 0 never resolves |
| `0x4492a0` | `ReleaseEntityById` | soft release: `+0x35c \|= 0x4000000` |
| `0x449210` | `StopEntityById` | hard stop: word `+0x304 = 1` |
| `0x449470` | `SetEntityStateWordEaxEsiAbi` | EAX = &idSlot, ESI = value; doubles as kill code and option sprite kind |
| `0x4492f0` | `SetEntityPositionDirectEsiAbi` | ESI = float3; `+0x340/344/348` verbatim |
| `0x449350` | `SetEntityPositionOffsetEsiAbi` | ESI = float3; `+0x340 = p0+224`, `+0x344 = p1+16`, `+0x348 = p2` |
| `0x409e50` | `ExpireEntityHandleEaxAbi` | EAX = &handle; state word 1 |
| `0x40c4d0` | `FireEntityHandleEaxAbi` | EAX = &handle; state word 3 |
| `0x428d70` | `IsOutsidePlayfieldBox` | ECX = {x, y}, stack = half extents; outside when `x+hx <= -192 \|\| x-hx >= 192 \|\| y+hy <= 0 \|\| y-hy >= 448`; NaN takes the outside branch |

## 0x00449950 AllocatePoolVmEsiAbi (verified against disassembly)

Pool records at `manager+0x68 + index*0x3ac`, used-flag bytes at
`manager+0x3ac068`, cursor dword at `manager+0x3ad068` (4096 slots). The
candidate slot is accepted when free (the function itself sets the used
flag); a busy candidate advances the cursor once (writing it back
immediately) and retries; a second busy candidate falls back to the heap
(`0x452493`, 0x3ac bytes) with a blank construct (zero 235 dwords, clear
bit 0 of the nine latch dwords `0x6c/0xb0/0xfc/0x128/0x174/0x1b0/0x1fc/
0x228/0x378`, `u16 +0x384 = 0xffff`) plus the reset `0x401de0` — which the
original calls even for a null allocation (preserved). The heap path does
not set the used flag. The cursor advances unconditionally at the end, so
the slot after the accepted one is the next candidate.

## 0x00449870 AssignPoolVmScriptEcxEaxAbi (corrected ABI)

Stack args (vm, script id), `ret 8`. Zero-list exactly
`{0x340, 0x344, 0x348, 0x334, 0x338, 0x33c, 0x34c, 0x350, 0x354}`;
`+0x35c |= 0x40000000`; `u16 +0x38a = script id`; `+0x3a0 = +0x3a1 =
0x10`; then the script-bind boundary `0x43e7e0` (kept as an extern —
unlike `0x43e710` it reads its script list from a context struct at
+0x238 with a stop flag at +0x248, and it also wipes the whole record
when the script is missing; the pass-through register value its callers
supply is absorbed by the boundary).

## 0x004489d0 LinkEntityAndAssignIdEaxEsiAbi (corrected ABI)

EBX = entity, EDX = manager, EAX = &outId (ECX is computed internally, not
an input). Node = entity+4 (`{entity, next, prev}`), initialized to
self/0/0. Links into list 1 only: empty list sets head then tail; a
non-empty tail with a non-null next (quirk) splices the node before that
next, then after the tail; otherwise plain append; tail always becomes the
node. The id counter at `manager+0x732454` increments and wraps to 1 (not
" id+2"); the id lands on entity+0 and *outId.

## 0x00448a50 LinkEntityFrontAndAssignIdEaxEsiAbi (front twin)

Cross-checked with IDA (same register ABI as 0x4489d0: EBX = entity,
EDX = manager, EAX = &outId). Identical node layout
(`entity+4 = {entity, next, prev}`) and the shared id counter at
`manager+0x732454` (wrap past zero to 1), but this twin **prepends** to
list A: when the head at `+0x72dad4` is non-empty the new node's next
becomes the old head and the old head's prev becomes the new node; when
empty the tail at `+0x72dad8` is set once. The head is always the new node.
Native keeps a dead branch (`v5 = new->next` right after it was zeroed)
that has no observable effect.

## Resource-keyed release walk (0x004493e0)

`ReleaseEntitiesUsingResourceEaxEdxAbi` (native EAX = manager, EDX =
resource pointer) walks the list-A and list-B node chains
(`manager+0x72dad4` / `+0x72dadc`, nodes `{entity, next}`) and applies
`entity+0x35c |= 0x4000000` to every entity whose `+0x308` resource
pointer equals EDX. Returns the last list-B node (or zero).

Note: the 0x00405ed0 wrapper passes its ECX (never established by any
call site — caller garbage) as the manager and its +0x3e0bb0 slot as the
resource; the wrapper remains a boundary until that indeterminacy is
pinned down.
