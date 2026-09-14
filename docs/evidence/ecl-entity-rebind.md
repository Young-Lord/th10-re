# ECL entity rebind (0x0043e8b0) and id-slot rebind (0x004496d0)

Reconstruction in `src/EclScriptLibrary.cpp`
(`BindEntityToScriptSlotEaxEdxEcxAbi` and
`RebindEntitySlotEaxStackAbi`, replacing the former extern boundary
consumed by `RunEclScriptSetupStackAbi`).

## 0x004496d0 RebindEntitySlotEaxStackAbi

Native EAX = id slot pointer, stack = new id, `ret 4`. Reads the current
id from the slot, resolves its entity through 0x004491c0
(`FindEntityEdxStackAbi` over the render owner's two lists at
+0x72dad4/+0x72dadc), and — only on a hit — calls the bind helper with
EAX = entity, EDX = entity+0x308 (the owning list back-pointer) and
ECX = the new id. A failed lookup is silent.

## 0x0043e8b0 BindEntityToScriptSlotEaxEdxEcxAbi

1. `vm = owner_list->vm_table[new_id]` where the table pointer lives at
   owner+0x11c. A null entry returns immediately.
2. The owner+0x124 dword must be zero or the whole bind is skipped.
3. Entity word +0x38a = new id.
4. Flags dword +0x35c: when bit 0x200 is set, the +0x3c float is
   negated (fmul -1.0f at 0x470b60) and the dword is rewritten as
   `(flags | 0x8) ^ 0x200` before the low word gets overwritten below.
5. Word +0x35c = 7 (the low flag word is pinned to 7 regardless).
6. +0x2fc = -1; +0x60/+0x64 = 0; +0x5c = 0xfff0bdc1 (the negative-NaN
   timer sentinel); +0xb4/+0x100/+0x12c/+0x178/+0x1b4/+0x200/+0x22c = 0.
7. Word +0x386 = the owner list's first word; flags +0x35c &= ~0x600;
   +0x308 = owner list (stored twice by the native — kept).
8. +0x38c and +0x390 = the bound VM record.
9. Lazy arm of the timer triple: when bit 0 of +0x6c is clear, set
   +0x60 = 0, +0x5c = NaN sentinel, +0x64 = 0, +0x68 = &flt_476f78 (the
   global frame-time scale rate pointer), +0x6c |= 1 — then the
   unconditional reset overwrites +0x60/+0x64 with 0 and +0x5c with -1.
10. Flags &= ~1.
11. Run the freshly bound VM body once (0x0043ee30, exported here as
    `FinalizeTimelineRenderObjectSetup`), then increment the render
    owner's bind counter (+0x4c of DAT_00491c10).

The call site of the bind helper is the id-slot rebind above; the new id
is a VM-table index, so the entity currently carrying the old id adopts
the VM record (and script) that the list owner publishes at that index.
