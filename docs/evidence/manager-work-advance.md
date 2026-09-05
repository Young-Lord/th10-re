# ManagerWork Chain Advance Evidence

## Scope

This note records the directly observable behavior of the ManagerWork advance entry at `0x004473c0` and its selected-stage processor at `0x00447470` in `resources/th10.exe`. It recovers the staged relative chain, the two prefix counters, and the result/control behavior of the processing helper.

`ManagerWork`, `ChainNode`, `stage`, `record`, and `pointer output` are behavioral names. They do not establish original class names, file-format names, or the concrete identity of the virtual object at `work+0x120`.

## `0x004473c0`: staged prefix walk

### ABI

The external entry saves `EBX`, `EBP`, `ESI`, and `EDI`, ends in `ret 8`, and uses its two stack arguments as follows:

```text
input:  stack[0] = ManagerWorkOwner *owner
        stack[1] = ManagerWork *work
output: EAX = work on a non-negative selected-stage result or natural end
        EAX = 0 on a negative selected-stage result
stack:  callee removes 8 bytes
```

`owner` is forwarded to `0x00447470`, but no instruction in the recovered `0x00447470-0x004476f8` range reads that first stack argument. Its purpose is not established by this pair alone. `work` is the non-error return value.

### Cursor behavior

`work+0x124` is not a direct node pointer and is not simply a remaining-item count. The outer service calls this function only while it is nonzero. This entry starts at `work+0x108`, initializes all three counters to zero, and tests a zero-based chain index against `work->active_cursor - 1` *before* processing the current node.

On the matching node, it invokes `0x00447470`, then processes that node's linkage normally. A non-negative helper result returns after that selected node, setting `active_cursor` to its old value plus one unless that node was terminal. Normal values are one-based stage numbers:

| `active_cursor` on entry | Node selected | Prefix totals supplied |
| ---: | --- | --- |
| `1` | head / index `0` | zero, zero |
| `2` | index `1` | node 0 `+0x00`, node 0 `+0x04` |
| `n` | index `n - 1` | sums of all preceding nodes' `+0x00` and `+0x04` |

The walk restarts at head on every call. It retains no current-node pointer in ManagerWork. Earlier nodes contribute to the prefix sums but do not invoke the processor during that call.

### Outer result and control behavior

For each visited node, `0x004473c0` adds node `+0x00` to one signed 32-bit accumulator, node `+0x04` to another, and increments a node-count local. It then advances by the signed relative displacement at `node+0x38`:

```cpp
next = reinterpret_cast<ChainNode *>(
    reinterpret_cast<u8 *>(node) + node->next_relative_0038);
```

A zero displacement is the terminator; there is no initial null guard for `work+0x108`. The original is 32-bit `add` arithmetic, so C++ should resolve relative pointers only after input validation rather than reproduce undefined pointer arithmetic.

| Event | `work+0x124` after return | Return |
| --- | ---: | --- |
| Selected helper returns `< 0` | `0` | `0` |
| Any visited node has `next_relative_0038 == 0` | `0` | `work` |
| Selected helper returns `>= 0`, successor exists | old cursor `+ 1` | `work` |
| Cursor selects no node before the terminator | `0` | `work` |

An out-of-range positive cursor therefore reaches the final node without calling the inner helper and is silently completed. The outer service treats a non-null result as its successful/continue result.


## Chain node layout recovered here

Only the following offsets are directly established. All relative values are resolved from the address of the containing node, not from the work object.

```cpp
struct ChainNodePartial {
    i32 generated_record_count_0000;
    i32 pointer_output_count_0004;
    u8 unknown_0008[4];
    i32 horizontal_normalizer_000c;
    i32 vertical_normalizer_0010;
    i32 adapter_source_relative_0014;
    i32 adapter_argument_0018;
    i32 marker_source_relative_001c;
    u8 unknown_0020[8];
    i32 required_kind_0028;       // must equal 4
    i32 virtual_method_argument_002c;
    i32 alternate_source_relative_0030;
    u8 use_alternate_adapter_0034;
    u8 unknown_0035[3];
    i32 next_relative_0038;       // zero terminates the chain
    i32 generated_item_relative_0040[];
};
```

`generated_record_count_0000` and `pointer_output_count_0004` have separate roles: each is accumulated by the outer walk, and each controls an inner output loop. The binary uses signed `jle`/`jl` tests for these loops. Negative and zero counts do not enter their loops, although a negative value still participates in the outer 32-bit prefix addition.

After `generated_record_count_0000` 32-bit relative item offsets beginning at `+0x40`, the second array begins at:

```text
node + 0x44 + 4 * generated_record_count_0000
```

It has `pointer_output_count_0004` elements at stride eight. Only the first dword of each element is consumed here as a node-relative pointer; its second dword remains uncharacterized.

The first array's referenced objects are read at offsets `+0x04`, `+0x08`, `+0x0c`, and `+0x10` as floats. The second array's referenced objects are not dereferenced within this range after relocation into the output pointer list. This establishes a compact relative-addressed node payload, but not a safe file-backed C++ struct without range validation.

## `0x00447470`: selected-stage processor

### Mixed ABI

The entry reserves `0x68` bytes, preserves `EBP` and `EDI` on all exits, and ends in `ret 0x14`. It accepts five stack words and one caller-supplied register value:

```text
input:  EAX      = prefix node count / zero-based stage index
        stack[0] = ManagerWorkOwner *owner
        stack[1] = ManagerWork *work
        stack[2] = prefix generated-record total
        stack[3] = prefix pointer-output total
        stack[4] = ChainNode *node
output: EAX = 1 on the observed success path; -1 on validation/setup failure
stack:  callee removes five words
```

The caller passes the two outer sums as stack words 2 and 3. The processor uses the first as the base index for the `+0x118` output array and the second as the base index for `+0x11c`. It does not use stack word 0 in the recovered body. `EAX` is the number of nodes preceding this stage and is shifted left four at `0x004474ad`, selecting its 16-byte `work+0x120` record.

This is not a normal `__stdcall` or `__thiscall` source boundary. A C++ implementation should give it an ordinary typed semantic signature and leave an optional small EAX/stack thunk only at the binary boundary.

### Validation and setup paths

The first two conditions are strict and return `-1` after calling the shared diagnostic path at `0x0044b8e0`:

| Validation | Diagnostic text address |
| --- | --- |
| `node == 0` | `0x00470274` |
| `node->required_kind_0028 != 4` | `0x00470254` |

It then selects one of three setup paths based on `use_alternate_adapter_0034` and, when that byte is zero, the byte at `node + node->marker_source_relative_001c`:

| Condition | Setup call | Failure test | Result |
| --- | --- | --- | --- |
| byte is `0x40` (`'@'`) | `0x00447050` | none | continues unconditionally |
| byte is not `0x40` | `0x00446eb0` | nonzero return | logs `0x004701b0`, returns `-1` |
| alternate byte is nonzero | `0x00446f40` | nonzero return | logs `0x0047016c`, returns `-1` |

The exact objects created/configured by these adapters remain external dependencies. Their direct register and stack conventions should not be declared as ordinary C++ member calls without wrappers. The `@` path writes through the selected `work+0x120 + 16 * prefix_node_count` record and calls `0x00447050` with custom `ECX`, `EDX`, `ESI`, and `EAX` inputs; it does not check that helper's return value.


### Virtual-object and output behavior

After successful setup, the processor obtains the selected 16-byte work record at:

```text
work->records_0120 + 16 * prefix_node_count
```

It calls three virtual slots on the record's first pointer, in order:

1. vtable `+0x1c`, with `node->virtual_method_argument_002c` as one stack argument.
2. vtable `+0x24`, with the object pointer as one stack argument.
3. vtable `+0x44`, with `(object, 0, &local_17_dword_block)` as stack arguments.

No null or vtable guard precedes these calls. The local block is initialized with `work+0x00` in its first dword and receives the third virtual call's output. The interface identity and the virtual methods' source names are not proven.

For every first-array entry, it combines that virtual output with the node-relative item data, performs x87 integer-to-float divisions using the two node normalizers and the two cumulative totals, and invokes `0x00447940`. That helper copies exactly `0x44` bytes into:

```text
work->output_records_0118 + 0x44 * (prefix_generated_record_total + i)
```

It then derives additional fields in the copied `0x44`-byte record, including four float divisions and two differences normalized by values at `+0x38` and `+0x3c` of the virtual-output block. Neither zero denominator is guarded. A semantic port must preserve expected x87 behavior where exact numeric matching matters and must not silently substitute guarded division semantics.

For every second-array entry, it stores the resolved first dword to:

```text
work->output_pointer_list_011c + 4 * (prefix_pointer_output_total + i)
```

The first output loop advances the `+0x118` record index from the prefix of node `+0x00`; the second independently advances the `+0x11c` pointer index from the prefix of node `+0x04`. This proves the two outer accumulators are output bases, not merely statistics.

## Safe C++ implementation plan

Implement this as two ordinary semantic functions and preserve the original mixed ABI only in isolated wrappers:

```cpp
enum StageProcessResult {
    StageProcess_Ok = 1,
    StageProcess_Failed = -1,
};

StageProcessResult ProcessSelectedChainStage(
    ManagerWorkOwnerPartial *owner,
    ManagerWorkPartial *work,
    i32 record_base,
    i32 pointer_base,
    const ChainNodeView &node);

ManagerWorkPartial *AdvanceManagerWork(
    ManagerWorkOwnerPartial *owner, ManagerWorkPartial *work);
```

`ChainNodeView` should parse node-relative offsets through explicit checked helpers, for example `ResolveRelative(node_base, displacement, required_size)`. It should expose two counted views rather than a flexible-array C++ object: the `i32` relative-item sequence at `+0x40` and the following stride-eight pointer-entry sequence. This keeps malformed resource data from causing C++ undefined behavior while retaining the valid-input layout.

`AdvanceManagerWork` should restart at head, compute both prefix sums in explicit `i32` arithmetic, invoke the stage processor only when `stage_index == active_cursor - 1`, then apply the exact terminal policy above. It should not write a persistent node cursor, skip prefix nodes, or treat a positive out-of-range cursor as an error: none matches the binary.

`ProcessSelectedChainStage` should separate setup-adapter calls from virtual object calls and output materialization. Model the three adapters and the virtual object as typed opaque boundaries until their concrete interfaces are separately recovered. In particular, do not present `0x00447470` as a regular C++ method: its `EAX` input and five-word cleanup convention require a thunk if a binary-compatible address is later required.

## Evidence basis

* `resources/th10.exe`, `0x004473c0-0x004476f8`.
* Direct caller `0x00447080-0x004470b1`, which sets `work+0x124 = 1` and repeatedly invokes `0x004473c0` while that field remains nonzero.
* Direct adapter and output helpers at `0x00446eb0-0x00447078` and `0x00447940-0x004479ce`, inspected only for the stated contracts.
