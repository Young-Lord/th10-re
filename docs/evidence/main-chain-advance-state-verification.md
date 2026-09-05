# MainChainAdvanceState Verification

## Scope and calling convention

This document independently verifies `MainChainAdvanceState` at TH10
`0x004218d0` against `resources/th10.exe` (the checked input executable has
SHA-256 `2f14760b6fbbf57549541583283badb9a19a4222b90f0a146d5aa17f01dc9040`).
It records observed machine-code behavior. Names such as `TitleScreen` and
`GameManager` are behavioral names inferred from the direct callees; they are
not proof of a final source-level class hierarchy.

The context pointer arrives in `EAX` and is preserved in `EDI` at `0x4218d4`.
This is not a normal compiler-generated C++ member-call ABI. The directly
observed fields are:

| Context offset | Access | Observed role |
| --- | --- | --- |
| `+0x38c` | read, normal-tail write | prior/committed state |
| `+0x390` | read, several writes | requested state |
| `+0x394` | write | snapshot of prior state when a transition begins |
| `+0x39c` | writes in states 7 and 10--13 | transition flag |
| `+0x6c4` | address passed to `EnterCriticalSection` / `LeaveCriticalSection` | lock slot 5 |
| `+0x6f9` | increment/decrement | lock slot 5 depth byte |
| `+0x768` | write | result of `0x0041fd00` factory |
| `+0x780` | write | set to `0xff000000` for every nontrivial transition |

## Common control flow and locking

`previous_state` is compared with `requested_state` before any lock operation.
If equal, the function immediately returns `1`; it does not write any field,
enter a critical section, or alter the depth byte.

For unequal states, the function:

1. calls the imported function at IAT `0x004660b0` with `&context[0x6c4]`;
2. increments `context[0x6f9]` after that call returns;
3. copies `+0x38c` to `+0x394` and writes `0xff000000` to `+0x780`;
4. dispatches on the unsigned requested value, using the table at `0x00421b0c`
   only when it is at most `15`;
5. normally copies the (possibly rewritten) requested value to `+0x38c`, calls
   IAT `0x004660b4` with `&context[0x6c4]`, decrements `+0x6f9`, and returns
   `1`.

Thus the observable order is **enter then increment** and **leave then
decrement**. The increments and decrements are byte operations with no
underflow/overflow protection in this function.

The error tail uses `0x00421c50` with `ESI = 5`. That helper computes exactly
the same lock address (`context + 0x64c + 0x18 * 5 = context + 0x6c4`), calls
the same leave import, then decrements `context + 0x6f4 + 5`. It is lock-depth
balanced with the entry path.

## Requested-state dispatch

The following table is the exact first-level table at `0x00421b0c`. “Commit”
means the common normal tail described above.

| Requested value | Target | Observed effect before outcome |
| --- | --- | --- |
| `0` | `0x42192b` | Rewrites requested state to `1`; calls `0x0041fd00`; stores its result at `+0x768`. Null result rewrites request to `3` and takes error path A; non-null commits state `1`. |
| `1` | `0x42198b` | Commit. |
| `2` | `0x42198b` | Commit. |
| `3` | `0x42194e` | Calls `0x004203f0`; takes error path B. |
| `4` | `0x421967` | Uses the predecessor-state selector described below, then commits. |
| `5` | `0x42198b` | Commit. |
| `6` | `0x42198b` | Commit. |
| `7` | `0x4219f8` | If predecessor is `4`, loads global `0x0047784c` and calls `0x0042cdb0`; then writes `+0x39c = 1`, pushes `0`, calls `0x004180e0`, and commits. |
| `8` | `0x42198b` | Commit. |
| `9` | `0x42198b` | Commit. |
| `10` | `0x421a80` | Loads global `0x00477810`, calls `0x00418150`, writes `+0x39c = 1` and requested state `7`; updates globals `0x00474c7c` and `0x00477848` from `0x00474c80`; pushes `0`, calls `0x004180e0`, then commits state `7`. |
| `11` | `0x421a4e` | Loads `0x00477810`, reads its `+0x5c` into `EBX`, writes `+0x39c = 0`; calls `0x00418150` only when predecessor is `7`; rewrites request to `7`; pushes the saved `+0x5c` value, calls `0x004180e0`, then commits state `7`. |
| `12` | `0x421a1e` | If predecessor is `4`, loads `0x0047784c` and calls `0x0042cdb0`; pushes `1`, rewrites request to `7`, writes `+0x39c = 1`, calls `0x004180e0`, then commits state `7`. |
| `13` | `0x421ac7` | Loads `0x00477810`, calls `0x00418150`; pushes `0`, writes `+0x39c = 1` and request `7`, calls `0x004180e0`, then commits state `7`. |
| `14` | `0x421af2` | Calls `0x00418150` with global `0x00477810` only when predecessor is `7`; calls `0x0040b940`; commits state `14`. |
| `15` | `0x4219bb` | Handles only predecessor states `2`, `7`, and `14` as described below; all others commit state `15`. |
| `>15` (unsigned) | `0x42198b` | No jump-table access or special action; commits the out-of-range requested value. |

The `0x004180e0` calls receive their one observed stack argument as `0`, the
saved title object field `+0x5c`, or `1`. The function's register-based ABI and
the exact semantic meaning of these values are not established by this
function alone.

## Requested state 4 secondary selector

At `0x421967`, the prior state from `EAX` is decremented and range-checked
against `13`. Consequently only prior values `1..14` index the byte selector
table at `0x00421b5c`; prior `0`, prior `15`, and every out-of-range value go
directly to the normal commit. The selector bytes and target table at
`0x00421b4c` give the following complete behavior:

| Prior value | Action for requested state `4` |
| --- | --- |
| `0` | Commit; no creation/destruction call. |
| `1`, `2` | Call `0x0042cd50`, then commit. |
| `3`, `4`, `5`, `6` | Commit; no creation/destruction call. |
| `7` | Load global `0x00477810`; call `0x00418150`; call `0x0042cd50`; commit. |
| `8`, `9`, `10`, `11`, `12`, `13` | Commit; no creation/destruction call. |
| `14` | Load global `0x00477700`; call `0x0040b9d0`; call `0x0042cd50`; commit. |
| `15` or outside `0..15` | Commit; no creation/destruction call. |

The calls at `0x00418150`, `0x0042cd50`, and `0x0040b9d0` use `ESI` as set by
the preceding load or inherited control flow. Their final C++ prototypes must
therefore not be inferred from these instruction sequences alone.

## Requested state 15 predecessor handling

`0x4219bb` performs chained subtraction tests on the prior state value:

| Prior value | Action |
| --- | --- |
| `2` | Rewrite requested state to `4`; write `3` to global `0x00491c00`; call `0x0042cd50`; commit state `4`. |
| `7` | Load `0x00477810`; call `0x00418150`; then perform the same rewrite, global write, `0x0042cd50` call, and commit state `4`. |
| `14` | Load `0x00477700`; call `0x0040b9d0`; then perform the same rewrite, global write, `0x0042cd50` call, and commit state `4`. |
| Any other value | No special call or rewrite; commit state `15`. |

## Error returns

There are exactly two explicit return-`4` paths after the lock has been
entered. Neither bypasses unlock/depth decrement.

| Path | Trigger | Operations after the trigger | Result |
| --- | --- | --- | --- |
| A, `0x421944` | Requested state `0` and `0x0041fd00` returns null | Set requested state to `3`; call `0x004203f0`; leave slot 5; decrement depth. | `4` |
| B, `0x42194e` | Requested state `3` | Call `0x004203f0`; leave slot 5; decrement depth. | `4` |

No result from `0x00418150`, `0x004180e0`, `0x0042cdb0`, `0x0042cd50`,
`0x0040b9d0`, or `0x0040b940` is tested by this function. Therefore their
failure behavior cannot produce a distinct return path here without an
unobserved nonlocal transfer.

## Reconstruction implications

The current semantic C++ reconstruction should preserve the following facts:

- Equal states return `1` before acquiring lock slot 5.
- Every unequal-state path sets `+0x394` and `+0x780` before dispatch, even an
  out-of-range requested state.
- State `0` success commits `1`, not `0`; its factory-null path is one of the
  two return-`4` paths.
- State `4` creates at most for predecessor values `1`, `2`, `7`, and `14`.
- State `11` destroys the title object only when predecessor is `7`.
- State `15` maps only predecessors `2`, `7`, and `14` into committed state
  `4`; all other predecessors commit `15` unchanged.

This verification does not establish names for the unknown numeric states,
the full layouts of global objects, or the source-level calling conventions of
the direct callees.
