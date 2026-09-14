# Game-Manager Slot Background VMs — 0x00421180 / 0x00421070 / 0x00421300

Implemented in `src/GameManagerGateVms.cpp/.hpp`. The 0x491c28 game-manager
slot owns three background effect VMs: their entity ids live in
DAT_00477824 / DAT_00477828 / DAT_0047782c, the one-time spawn latch at
`slot+0x6fc` (DAT_00492324) and the ANM manager-work used for the script
binds at `slot+0x3c8`.

`0x00421070` and `0x00421300` were previously kept as the
`LeaveGameManagerGate` / `EnterGameManagerGate` boundaries in
`src/TitleSceneSetup.cpp` (call sites 0x00417c2b and the 0x00417b4c failure
tail, both with the slot address 0x491c28 pushed) — they are not gate
toggles but stop the three VMs with different state words. `0x00421180` is
their spawn counterpart; the shipped binary contains **no reference to it**
(no E8/E9 rel32 call or jump and no absolute dword anywhere in the image),
so it is reconstructed from its body and its twins, presumably reached in
the original through a dispatch that no longer exists.

## 0x00421180 `SpawnGameManagerBackgroundVmsStackAbi` (stdcall `ret 8`)

Stack0 = the slot, stack1 = a float3 position source (the function reads it
at its second argument slot and forwards it as the native ESI of the
0x004492f0 position publishes — this resolves the apparent "reads past its
argument" oddity once the `ret 8` is recognized).

While the +0x6fc latch is zero, for scripts 0, 1 and 2:

1. pool-allocate a 0x3ac VM record (0x00449950, ESI = owner 0x491c10);
2. `+0x35c |= 0x40000000`, kind `+0x20 = 0xf`;
3. bind the script through 0x00449870 with native ECX = the slot's +0x3c8
   manager-work (the semantic `AssignPoolVmScriptEcxEaxAbi` body feeds its
   bind context internally);
4. link into the owner entity list with the tail-append 0x004489d0 (not the
   list-B front insertion 0x00448ac0), publishing the id into
   DAT_00477824/28/2c.

Then latch `+0x6fc = 1` and publish the caller's float3 into all three VMs
through 0x004492f0 (semantic `SetEntityPositionDirectEsiAbi`, EDX = owner).

Tail on both paths: while the render owner's first signed dword is negative
(idle), store 8 into it and write the two 640x480 rects
`{0, 0, 0x280, 0x1e0}` at owner+0x2c..+0x34 and owner+0x3c..+0x48.

## 0x00421070 `LeaveGameManagerGateStackAbi` / 0x00421300 `EnterGameManagerGateStackAbi` (stdcall `ret 4`)

Identical stop bodies, parameterized only by the stop word and the post-stop
latch value (0x00421070: word 1, latch 0; 0x00421300: word 2, latch 2):

1. While the +0x6fc latch reads exactly 1: for each of the three published
   ids, resolve the entity (0x004491c0) and, when found, write the stop word
   at `entity+0x304` (unconditional) and, when the entity's +0x18 count is
   zero, repeat the word over the `+0x14 {entity, next}` child chain; then —
   after all three stops — clear the three id slots and store the latch.
2. When DAT_00491bec is nonzero, clear it (both twins).

## Verification

`g++ -m32 -std=c++98 -fsyntax-only -I src src/*.cpp` — 0 errors.
