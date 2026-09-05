# Global Lifecycle Manager Evidence

## Scope

This covers TH10 `resources/th10.exe` at `0x0041fd00`, `0x0041f850`,
`0x0041fac0`, `0x0041fb50`, `0x0041feb0`, and `0x0041fef0`.
`GlobalLifecycleManager` is a conservative reconstruction name. It is a
separate heap object from the main-chain context at `0x00491c28`;
`MainChainAdvanceState` stores its factory result at context `+0x768`.

## ABI and ownership

| Address | Observed ABI | Behavior |
| --- | --- | --- |
| `0x0041fd00` | no explicit arguments; `EAX` result | Factory allocates `0x3f0`, constructs, initializes, and returns the object or null. |
| `0x0041f850` | object pointer in `ESI`; `EAX` result | Register-based constructor; not a normal `thiscall` entry. |
| `0x0041fac0` | object pointer in `EBX`; `EAX` status | Register-based initializer; its current body returns zero. |
| `0x0041fb50` | object pointer at `[ESP+4]`; `ret 4` | In-place destructor/teardown; does not free the manager allocation. |
| `0x0041feb0` | callback owner in `ECX`; returns `1` | Calculation-chain callback. |
| `0x0041fef0` | callback owner in `ECX`; `EAX` result | Draw-chain callback wrapper. |

`0x0041fd00` calls allocator `0x00452493` with `0x3f0`. On initializer failure
it calls `0x0041fb50(pointer)`, then separately calls `0x004524a1(pointer)`.
Main-chain teardown at `0x00420423` repeats that destructor-then-free sequence.
A C++ wrapper must therefore not model `0x0041fb50` as a deleting destructor.

## Constructor and fields

`0x0041f850` temporarily writes `0x004703e4` at `+0x10`, clears
`+0x14..+0x20`, then uses `rep stosd` to zero all `0x3f0` bytes. It finally
sets bit 1 at `+0x00` and publishes the pointer through `0x00477820`.
The pre-fill stores, including a temporary `0xffff` at `+0x394`, are overwritten
by the full clear and are not final field values.

Directly supported fields are: `+0x00` lifecycle flags (bit 1 observed),
callback pointers at `+0x08` and `+0x0c`, owned heap pointer `+0x388`, and
startup/frame state at `+0x3dc`, `+0x3e0`, `+0x3e4`, `+0x3e8`, and `+0x3ec`.

## Initialization and callbacks

`0x00449ed0` allocates a `0x24`-byte callback node, places its target at node
`+0x8`, and initialization puts the manager pointer at node `+0x20`.

| Manager offset | Target | Registration helper | Priority |
| --- | --- | --- | --- |
| `+0x08` | `0x0041feb0` | `0x00449ae0` | `EDI = 3` |
| `+0x0c` | `0x0041fef0` | `0x00449b70` | `EDI = 2` |

The dispatcher at `0x00449c70` loads node `+0x20` into `ECX` before indirectly
calling node `+0x8`. Thus both labels receive `GlobalLifecycleManager *` in
`ECX`, although Ghidra has not created function boundaries for them.

Initialization calls `0x0044c150` with `manager + 0x10`, then invokes
`_beginthreadex` (`0x00453683`) with start address `0x0041f990`, argument
`manager`, and thread-ID output `manager + 0x18`. It stores the returned handle
at `+0x14`, writes one at `+0x20`, and writes the start address at `+0x28`.
The exact type/size of the region from `+0x10` is not proved; it must remain
opaque, rather than being claimed as a Win32 standard structure.

`0x0041feb0` tests manager `+0x00` bit 1. When set, it marks three records
reachable from global `0x004776e0` at `+0xc`, `+0x10`, and `+0x89a8`; clears
`0x00491ff4` bit `0x1000`; writes `4` to `0x00491fb8`; and clears the flag.
It always returns `1`. A one-shot global-chain-disable callback is a safe label;
the target record and state meanings are not yet typed.

`0x0041fef0` is exactly `push ecx; call 0x0041fdd0; ret`. Its inner handler
returns `1`, advances `+0x3e4` from 1 to 2 using `+0x3e0`, conditionally
advances `+0x3e8` from 1 to 2, increments `+0x3ec` each invocation, and writes
an auxiliary result at `+0x3dc`. It is safely a frame/draw-stage callback.

## Teardown

`0x0041fb50` calls `0x0044c150` with `manager + 0x10`, unregisters/releases
nodes `+0x08` and `+0x0c` through `0x00449f60`, and uses callback-framework
lock `0x00492274` during removal. It clears `0x00477820`, performs several
global resource cleanups, frees non-null `manager + 0x388`, clears that field,
restores `0x004703e4` at `+0x10`, and calls `0x0044c150` again.

The cleanup proves `+0x388` is owned heap storage, but not its pointee type.
The touched globals indicate lifecycle coordination, not exclusive ownership
of every global resource.

## Recommended C++ boundary

Use an ordinary `GlobalLifecycleManager` layout of size `0x3f0`, retaining
`+0x10..+0x387` as opaque startup/thread storage, and expose explicit
`CreateGlobalLifecycleManager()` / `DestroyGlobalLifecycleManager(manager)`
wrappers. Preserve the special constructor (`ESI`) and initializer (`EBX`)
conventions in thin bridges, or reimplement them semantically in ordinary C++.
The two callback entries can be modeled as `__thiscall` methods because their
direct convention is `ECX = owner`.
