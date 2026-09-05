# ManagerWork Selected-Stage Processor

## Scope

This note defines a C++ reconstruction boundary for the selected-stage
processor at `0x00447470` in `resources/th10.exe`.  It combines the three
stage setup call sites with the post-setup virtual calls and the `0x44`-byte
output block.  The purpose is to make
`ProcessSelectedManagerWorkStage` implementable without embedding D3DX calls
or pretending that any observed mixed-register entry is an ordinary C++
member function.

`node`, `record`, `texture`, `stage`, and `output block` are behavioral names.
They do not identify original source types or resource-format fields.

## Processor contract

The native entry has an `EAX` input plus five callee-cleaned stack words:

```text
EAX      = zero-based node index / WorkRecord index
stack[0] = owner (not read by this body)
stack[1] = ManagerWork *work
stack[2] = generated-output base index
stack[3] = pointer-output base index
stack[4] = ChainNode *node
return   = 1 on success, -1 on validation or checked setup failure
cleanup  = ret 0x14
```

The semantic C++ entry should instead receive the already decoded values:

```cpp
enum StageProcessResult {
    StageProcess_Ok = 1,
    StageProcess_Failed = -1,
};

StageProcessResult ProcessSelectedManagerWorkStage(
    ManagerWorkPartial &work,
    i32 record_index,
    i32 generated_output_base,
    i32 pointer_output_base,
    const ChainNodeView &node,
    const ManagerWorkStageDependencies &dependencies);
```

The original owner stack word is deliberately absent: no instruction in
`0x00447470-0x004476f8` reads it.  A future address-compatible wrapper, if
needed, is the only place that should recreate the `EAX`/five-stack-word ABI.

Before choosing a setup path, the native code returns `-1` through its
diagnostic helper when `node == NULL` or `node[+0x28] != 4`.  The semantic
entry should retain these two failure cases.  It selects this record without
a range check:

```cpp
WorkRecordPartial &record = work.records[record_index]; // stride 0x10
```

## Node fields consumed by the three setup call sites

The processor selects exactly one adapter.  The field uses below are direct
call-site facts, independent of D3DX internals.

| Condition | Adapter role | Node fields passed by `0x00447470` | Native failure policy |
| --- | --- | --- | --- |
| `node[+0x34] == 0` and `*(u8 *)(node + node[+0x1c]) != '@'` | encoded-memory setup | `+0x14` format index, `+0x10`, `+0x18`, `+0x0c`; also `work` | nonzero result logs and returns `-1` |
| `node[+0x34] != 0` | raw-pixel setup | `+0x14` format index, `+0x10`, `+0x0c`, node-relative `+0x30` | nonzero result logs and returns `-1` |
| `node[+0x34] == 0` and `*(u8 *)(node + node[+0x1c]) == '@'` | empty setup | `+0x14` format index, `+0x10`, `+0x0c` | result ignored; processor continues |

The adapter receives a pointer to the selected `WorkRecordPartial` on every
path.  At the processor boundary its known layout is:

```cpp
struct WorkRecordPartial {
    void *virtual_object;        // +0x00: immediately called through vtable
    void *owned_allocation;      // +0x04: encoded path input / later cleanup
    i32 encoded_size_or_unknown; // +0x08: encoded path input
    i32 format_pitch_factor;     // +0x0c: selected table value on all paths
};
```

`+0x04` and `+0x08` are not initialized by this processor.  They must remain
adapter-owned input/state, rather than being synthesized from node fields in
the processor.  The processor also makes no attempt to validate the format
index, the node-relative marker displacement, or the selected record index.

An implementation can remove D3DX from this source file with this narrow
dependency:

```cpp
struct StageSetupAdapter {
    virtual i32 Setup(WorkRecordPartial &record,
                      const ChainNodeView &node) = 0;
};

struct ManagerWorkStageDependencies {
    StageSetupAdapter &encoded_setup;
    StageSetupAdapter &raw_setup;
    StageSetupAdapter &empty_setup;
    void (*report_failure)(const char *message);
};
```

The `empty_setup` implementation must return a nominal success result even
when its texture creation fails, because the original ignored that HRESULT.
Conversely, `encoded_setup` and `raw_setup` must return failure exactly at
their texture-creation boundary.  Their post-create details, including D3DX,
can stay entirely behind these implementations.

## Post-setup virtual-object sequence

Regardless of which adapter was selected, the processor immediately performs
these three calls through `record.virtual_object`, with no null check, and
ignores every return value:

```text
vtable +0x1c: (virtual_object, node[+0x2c])
vtable +0x24: (virtual_object)
vtable +0x44: (virtual_object, 0, &local_output)
```

The executable pushes the object pointer explicitly and each target cleans its
own arguments.  This is evidence for a binary adapter, not proof of a source
`__thiscall` declaration.  A semantic dependency can preserve only the proven
ordering and data exchange:

```cpp
struct StageVirtualObjectAdapter {
    virtual void CallSlot1c(void *object, i32 node_argument) = 0;
    virtual void CallSlot24(void *object) = 0;
    virtual void FillOutput(void *object, i32 zero, void *output_44_bytes) = 0;
};
```

Do not infer the original method names, interface identity, or ownership from
these three slots.  In particular, `FillOutput` must be allowed to write the
whole `0x44`-byte output record.

## Output block materialization

The third call writes a stack-local `0x44`-byte block.  The processor then
sets these two dwords before processing item payloads:

| Local output offset | Processor write |
| ---: | --- |
| `+0x20` | `work[+0x00]` |
| `+0x24` | `record.virtual_object` |

For each `i` in `0 .. node[+0x00)`, it resolves the signed relative dword at
`node + 0x40 + 4*i` to an item.  It consumes only these four item floats:

```text
item +0x04, +0x08, +0x0c, +0x10
```

Let `u32_as_float(x)` denote the native `FILD` conversion with the observed
negative-input correction (`if ((i32)x < 0) add 4294967296.0f`).  The native
FPU sequence constructs the following local-output fields:

```cpp
const float x_scale = u32_as_float(local_output.field_0018) /
                      static_cast<float>(node.field_000c);
const float y_scale = u32_as_float(local_output.field_001c) /
                      static_cast<float>(node.field_0010);

local_output.field_0028 = x_scale * item.field_0004;
local_output.field_002c = y_scale * item.field_0008;
local_output.field_0030 = x_scale * (item.field_000c + item.field_0004);
local_output.field_0034 = y_scale * (item.field_0010 + item.field_0008);
local_output.field_0038 = local_output.field_001c;
local_output.field_003c = local_output.field_0018;
```

There are no zero-denominator checks for node `+0x0c`, node `+0x10`, or for
the two later denominators consumed by the output copier.  A C++
implementation seeking numerical compatibility must avoid introducing
clamping or fallback values.  It should also use an explicit unsigned-32-bit
to float conversion helper instead of relying on signed conversion when the
virtual block's `+0x18` or `+0x1c` has its high bit set.

The native output helper copies the resulting whole block to:

```text
work[+0x118] + 0x44 * (generated_output_base + i)
```

and then derives six destination fields from the copied data:

```cpp
out.field_0020 = out.field_0008 / out.field_001c;
out.field_0024 = out.field_000c / out.field_0018;
out.field_0028 = out.field_0010 / out.field_001c;
out.field_002c = out.field_0014 / out.field_0018;
out.field_0030 = (out.field_0014 - out.field_000c) / local_output.field_003c;
out.field_0034 = (out.field_0010 - out.field_0008) / local_output.field_0038;
```

Thus offsets `+0x18/+0x1c` are virtual-call output inputs to the item scale,
while `+0x38/+0x3c` are explicitly carried into the output-copy normalization
step.  The processor does not otherwise interpret fields written by the
virtual call.

After the generated-record loop, it independently copies the first dword of
each stride-eight entry at:

```text
node + 0x44 + 4 * node[+0x00] + 8*i
```

after adding the node base, into:

```text
work[+0x11c] + 4 * (pointer_output_base + i)
```

No fields of these pointer-output targets are dereferenced here.

## C++ implementation shape

The processor body needs no D3DX header or direct graphics call:

```cpp
StageProcessResult ProcessSelectedManagerWorkStage(
    ManagerWorkPartial &work, i32 record_index, i32 generated_base,
    i32 pointer_base, const ChainNodeView &node,
    const ManagerWorkStageDependencies &deps)
{
    if (node.required_kind() != 4)
        return StageProcess_Failed;

    WorkRecordPartial &record = work.records[record_index];
    if (node.use_alternate_setup()) {
        if (deps.raw_setup.Setup(record, node) != 0)
            return StageProcess_Failed;
    } else if (node.marker_byte() == '@') {
        deps.empty_setup.Setup(record, node); // native return is ignored
    } else {
        if (deps.encoded_setup.Setup(record, node) != 0)
            return StageProcess_Failed;
    }

    // Invoke the three virtual-object operations through a separate adapter,
    // seed +0x20/+0x24, then materialize and copy each 0x44-byte output.
    return StageProcess_Ok;
}
```

Keep checked failure reporting outside the adapters and retain the original two
diagnostic paths.  The node view should perform bounds-checked relative-address
resolution when parsing external data; that validation is a host-side safety
boundary and must not be mistaken for a native processor branch.

## Evidence basis

* `resources/th10.exe`, `0x00447470-0x004476f8`.
* Stage setup entries `0x00446eb0-0x00447078`.
* Output copier `0x00447940-0x004479ce`.
* Existing local evidence: `manager-work-advance.md` and
  `manager-work-stage-adapters.md`.
