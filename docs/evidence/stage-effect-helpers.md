# TH10 Stage Effect Helpers And Narrow Boundaries

Module: `src/StageEffectHelpers.cpp/.hpp`.

## 0x00448db0 SpawnStageEffectEdxEbxAbi

Pool-allocates a VM (`0x449950`), zeroes `vm+0x20`, sets flag
`0x40000000`, publishes the position with the +224/+16 offsets, binds the
script through the semantic `0x43e710` body (silent whole-record wipe on
a missing script), and links into manager list A via the semantic
`0x4489d0` — the assigned id lands in `*(effect context)`.

## 0x00405500 CleanupStageLifeFlags

When the signed dword at stage+0x3738 exceeds 0x3b: zero `+0x3790`, then
clear bit 1 of `+0x378c` and the seven per-life dwords at `0xad4/0xe80/
0x15d8/0x1984/0x1d30/0x20dc/0x2488` (exactly seven — no ninth entry; this
corrects the earlier "+0x2fc0-style" guess). The death processor's flag
clearing (0x4269d0) shares the same offsets.

## 0x00420a90 StartBgmTrack

Copies the path, rewrites the extension to ".wav" in a local 256-byte
buffer (strrchr without a null check — preserved as a guard here), and
pushes (opcode 1, track slot = param) through the existing semantic
`QueueBgmCommand` body of `0x0043e460` with the fixed root 0x492590.
`0x0043e460` itself was already reconstructed as `QueueBgmCommand`; the
"halt named sounds" reading was corrected — it is a queue push, and the
mode/flag fields are consumed by the queue.

## 0x00448ac0 AttachEffectVmToListB

List-B twin of `0x4489d0` (head/tail `0x72dadc/0x72dae0`), node at vm+4
(`{id, next, prev}`), tail-splice quirk identical, id counter at
`manager+0x732454` wrapping past zero to 1, dual id output.

## 0x00424480 CreateGameOverOverlay

ESI = overlay target, stack (id, p2, p3, p4, p5), ret 0x14. Resolves the
entity by id (no null check — preserved), reads the parameter block at
entity+0x394, fails with -1 when `target+4 >= 0`, and copies the rounded
`+0x30/+0x34/+0x0c` floats plus `**(entity+0x308)` into the target with
the passthrough fields at `+0x8/+0xc/+0x10/+0x14`.

## 0x0041a120 CreateTextEffect

Allocates 0x88 bytes, zeroes them, self-links the node at +0xc, applies
the defaults (`+0x84 = -1`, `+0x6c = 300`, `+0x7c = 1.0f`, `+0x70` ends
at 30), walks to the tail of the active layer's list
(`list_base + (layer*3 + 0x1e)*4` with the layer global `0x474c7c`),
doubly links, copies the position vec3, strncpy's 64 bytes of text to
`+0x1c`, and sets `+0x64 = 0`, `+0x68 = 0x474c84`, `+0x60 = 0x474c84 != 0
? 0x474c8c : 0x474c88`. `ShowCautionText` now forwards the position its
native caller pushed (previously modeled as ignored).

## 0x0041beb0 InitMovementBlock

The block is just `{vel_x, vel_y}` from one FSINCOS (`cos(angle)*speed`,
`sin(angle)*speed`) — no stored angle.

## 0x0043ee30 resolution

The VM interpreter is the already-reconstructed
`FinalizeTimelineRenderObjectSetup` (TimelineRenderObjectSetup.cpp) — the
same interpreter family as the timeline module, one stack argument
(`vm`), dispatch table 0x4413a4 for opcodes -1..0x5b. The three register
args previously modeled at the call sites were caller garbage. All three
call sites (player dispatcher, script binds) now call the semantic body
directly; the `UpdatePlayerAnimationVmEcxAbi` /
`UpdateAnimationVmStackAbi` / `UpdateAnimationVmIndexedEcxDxStackAbi`
externs are gone. Known native quirks documented there: opcode 0x5c
overruns the dispatch table (unused), NaN deliberately matches the zero
sentinel in scale checks, and `vm+0x390`/`vm+0x394` are
self-referential/unchecked.

## 0x00448b40 AttachEffectVmToListBFront (front twin)

Cross-checked with IDA: list-B prepend twin of 0x448ac0. Node at
`vm+4 = {id, next, prev}`, head/tail at `manager+0x72dadc/+0x72dae0`,
shared id counter at `manager+0x732454` (wrap past zero to 1). Empty list
sets the tail once; otherwise the new node is pushed to the head and the
old head's prev is rewired; the head always becomes the new node.
