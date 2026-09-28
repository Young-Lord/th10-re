# ECL Object Lifecycle (0x40b250..0x40cd20)

Module: `src/EclObjectLifecycle.cpp`. Covers the ECL script-object entity
bindings and the 0x1098-byte script viewer objects (vtables 0x46d0d8 /
0x46d0f0) with their deferred-name lists.

- `0x0040b250` `StoreScriptObjectInsnBlockEcxEcxAbi` — __thiscall ECX =
  script object, EDX = 0x2c-byte source block: stores the instruction block
  into the object.
- `0x0040b3a0` `ReleaseScriptObjectEntityBindingsEaxAbi` — native EAX =
  script object. Stops the embedded thread and releases the entity
  bindings (the +0x4000000 resource-release flag governs the teardown
  order; runs the release twice for the pair of bound entities).
- `0x0040c5d0` `ClearViewerPauseFlagsEaxAbi` — EAX = viewer: clear the
  +0x1000/+0x1004 flag pair.
- `0x0040c6e0` `FreeViewerDeferredListEaxAbi` — EAX = viewer: free the
  deferred-name linked list (CRT free per node, 0x4524a1 boundary).
- `0x0040c710` `InitViewerHeaderEaxAbi` — EAX = viewer: publish the
  0x46d0d8 vtable and the header defaults.
- `0x0040c730` `InitViewerListsEaxAbi` — EAX = viewer: clear the +0x1028
  busy bit and initialize the list heads.
- `0x0040c7b0` `DestroyViewerEcxEcxAbi` — __thiscall ECX = viewer, stack =
  free flag (ret 4): the viewer destructor; releases the deferred list and
  the viewer allocation when flagged.
- `0x0040c800` `ClearViewerTickFlagsEaxAbi` — EAX = viewer: clear the
  +0x1008/+0x100c pair.
- `0x0040cf40` `InitInstructionTargetRecordEdxAbi` — native EDX = a
  0x1f8-byte record (ECL instruction target): wipes 0x7e dwords and stores
  the 8.0f default (0x41000000) at +0x2c.
- `0x0040cf90` `FindScriptObjectByUniqueIdEcxEaxAbi` — native EAX = owner,
  ECX = unique id: walk the linked list at +0x18 (next at node+0x8) and
  return the node whose +0x54 equals the id, or null.
- `0x0040cd20` `LoadScriptFileAndRegisterNamesEcxEcxAbi` — native ECX =
  registry, stack = script path (ret 4). Copies the path into the shared
  scratch buffer at 0x497c38 (zero first byte, then a byte scan + rep movs
  append), loads it through the shared file loader (0x44b360, null
  size/null tail), hands the loaded block to the name registrar (0x450220)
  and returns 0 when the registrar reported success (>= 0) or -1
  otherwise. The same entry is declared as the `InitScriptViewerPathEcxStackAbi`
  boundary in `src/ConditionalStateSubrecords.cpp` for the viewer path.

Related: 0x40cc70 (`ResetEclSubObjectFlags`) is documented in
`docs/evidence/ecl-eased-transforms.md`.
