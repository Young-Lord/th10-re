# ManagerWork `+0x128` Cleanup-State Reachability

## Scope

This note resolves the reachability of the `ManagerWork + 0x128` branch in
`0x00447700` from `resources/th10.exe`. It supplements
`manager-work-service.md`; it does not change the behavioral names used there.

## Result

The branch is **present and control-flow reachable**, but is **unreachable
under every statically recovered ManagerWork initialization and mutation
path**. The only proven constructor for an object stored in the 33 owner slots
zeroes all `0x130` bytes, including `+0x128`; it subsequently writes `+0x108`,
`+0x10c`, `+0x110`, `+0x114`, `+0x118`, `+0x11c`, `+0x120`, and `+0x124`, but
never writes `+0x128`. No recovered ManagerWork helper writes that offset.

It is not a normal deletion path. If an unknown writer nevertheless makes the
field nonzero, the branch frees the object, clears its owner slot, then
unconditionally writes through the now-null slot pointer. On normal Win32
process mappings this is a deterministic access violation at address
`0x00000128`. The supported classification is:

* normal runtime state: dead by established initialization invariant;
* malformed, corrupted, or unknown external state: intentional-or-accidental
  **fault path**, not a completed cleanup operation;
* compiler artifact: not supported as an explanation. The instructions are
  ordinary reachable code and preserve a real post-clear store; they are not
  padding, an unreachable table tail, or a disassembly boundary mistake.

Whether the fault was deliberately retained as a corruption tripwire or was
an original source-level use-after-null bug cannot be distinguished from this
binary alone. It must not be modeled as an ordinary `delete` state in C++.

## Exact Branch

`0x00447700` scans owner slots `0..32`, with `ESI` pointing at the current
slot and `EBX` holding its index. The relevant native instructions are:

```text
00447710  mov edi,[esi]                 ; work = owner->slots[index]
00447712  test edi,edi
00447714  je   00447757
00447716  mov eax,[edi+00000128h]
0044771c  test eax,eax
0044771e  je   0044774d                ; normal active/idle handling

00447720  test ebx,ebx
00447722  jl   0044773f
00447724  cmp ebx,21h
00447727  jae  0044773f
00447729  call 00447810                ; EDI still holds work
0044772e  mov eax,[esi]
00447730  push eax
00447731  call 004524a1                ; outer allocation release
00447736  add esp,4
00447739  mov dword ptr [esi],0         ; slot = 0
0044773f  mov ecx,[esi]
00447741  mov dword ptr [ecx+128h],0    ; writes [0x128] when index is valid
```

The index guard cannot save this path during the scan: `EBX` is initialized
to zero at `0x00447706`, incremented once per slot at `0x00447757`, and the
loop condition at `0x0044775b` keeps it below `0x21`. Thus a nonzero
`work+0x128` on any scanned non-null slot takes the calls, clears that slot,
and reaches the null-base store.

The normal branch at `0x0044774d` does not write `+0x128`; it only reads
`+0x124` to decide whether to call `0x004473c0`.

## Object Creation And Initialization

The sole recovered allocator/installer is `0x004470c0`, called through
`0x00447280` when a requested slot is empty. Its direct callers cover all
known creation requests for this 33-slot owner pool.

`0x004470c0` allocates exactly `0x130` bytes at `0x0044712f`, then clears
`0x4c` dwords at `0x0044714c..0x00447155`:

```text
0044712f  push 130h
00447134  call 00452493                ; allocate 0x130 bytes
...
0044714c  mov ecx,4ch
00447151  xor eax,eax
00447153  mov edi,esi
00447155  rep stosd                    ; zero [0, 0x130)
```

It publishes the pointer to `owner + 0x3ad06c + index * 4` before further
initialization (`0x00447162..0x0044716f`). The subsequent writes are to
`+0x000`, `+0x108`, `+0x10c`, `+0x110`, `+0x114`, `+0x118`, `+0x11c`, and
`+0x120`; no instruction in `0x004470c0..0x00447270` addresses `+0x128`.

`0x00447280` immediately sets only `work+0x124 = 1`
(`0x004472a9..0x004472b0`) and waits for that activity field to return to
zero. It likewise never writes `+0x128`.

The immediate service wrapper at `0x00447080` has the same property: it sets
`+0x124 = 1`, repeatedly calls `0x004473c0` until that field becomes zero,
and contains no `+0x128` write.

## Recovered Xref Set

For the actual ManagerWork type, the relevant direct entries are:

| Entry | `+0x128` action | Reachability consequence |
| --- | --- | --- |
| `0x004470c0` | whole allocation zero-fill | establishes zero invariant |
| `0x00447280` | none | sets only `+0x124` after construction |
| `0x00447080` | none | sets/services only `+0x124` |
| `0x004473c0` | none | advances, clears, or increments only `+0x124` |
| `0x00447700` | read, then null-base store | sole cleanup-state consumer/writer |
| `0x00447790` | read only | idle only when `+0x124` and `+0x128` are zero |
| `0x004477d0` | none | indexed destruction without testing `+0x128` |
| `0x00447810` | none | releases contents; leaves both state fields unchanged |

The whole-executable raw displacement search also finds many unrelated object
types at offset `0x128`. None has data flow from the `0x004470c0` allocation
installed in `owner+0x3ad06c`. There is no recovered store of the form
`mov [ManagerWork + 0x128], nonzero` outside the impossible post-slot-clear
store in `0x00447700`.

All direct destruction sites for these slots use the safe sequence
`0x00447810(work); 0x004524a1(work); slot = 0` without accessing `+0x128`
afterward. This includes the indexed helper `0x004477d0`, lifecycle cleanup
at `0x0041fc46`, and owner destruction at `0x00402593`. Their existence
demonstrates the intended complete deletion sequence and makes the
`0x00447700` post-clear store anomalous rather than required cleanup.

## C++ Reconstruction Consequence

Keep the field as an unnamed, invariant-zero `u32` rather than a normal
"pending delete" flag. A semantic reconstruction should express normal
operation using `+0x124` and use explicit destruction for owner teardown. It
should not reproduce the `+0x128` branch as safe cleanup.

For exact-behavior work, retain an isolated fault/trap branch with a comment
that it requires an invariant violation or an unproven external writer. For
readable C++ reconstruction, an assertion or explicit unsupported-state
handler is more truthful than assigning deletion semantics to this field.

## Evidence Basis

* `resources/th10.exe`, `0x00447080-0x00447270` and
  `0x00447280-0x004472d0`: creation, zero-fill, and immediate service.
* `resources/th10.exe`, `0x004473c0-0x00447901`: service, cleanup-state
  branch, idle predicate, indexed deletion, and content cleanup.
* Direct xref inspection of `0x004470c0`, `0x00447280`, `0x00447700`,
  `0x004477d0`, and `0x00447810`, plus a whole-executable displacement search
  cross-checked against allocation provenance.
* Independent destruction sites at `0x00402593`, `0x0041fc46`, and
  `0x0040b861` confirm the non-faulting destruction sequence.
