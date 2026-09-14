# Effect pool slots (0x0041b8e0) and effect-node list (0x0041c330)

Reconstruction in `src/EffectPoolEntityUpdate.cpp/.hpp`. Analysis was
done offline with objdump; every address below was read from the raw
listing of `resources/th10.exe`.

## Slot array layout

The effect manager published through DAT_004776f0 owns a 0x3f0-byte
slot array (first slot at +0x14, 0x896 slots — the loop counter 0x896 at
0x41b8ef). Each slot starts with a 0x3ac-byte animation VM record
followed by the effect payload:

- VM record: alpha byte at +0x2ff (the high byte of the +0x2fc timer
  dword that 0x0043e8b0 seeds with -1), world position floats at
  +0x334/+0x338/+0x33c, bound script word at +0x384.
- Payload: scripted position floats at +0x3b0/+0x3b4/+0x3b8, live flag
  dword at +0x3dc, current script id dword at +0x3e4.

## 0x0041b8e0 TickEffectPoolSlots

Native input is the pool base in EAX (plain ret, EAX = 1). For every
live slot:

1. VM position = scripted position + (224.0f @0x470b4c, 16.0f
   @0x470b48, 0).
2. When the scripted y (+16) is below 8.0f (0x470bd0, ordered less; the
   `test ah,0x5 / jp` idiom sends greater/equal/unordered to the other
   branch): the VM y is clamped to 24.0f (0x41c00000), the drop
   `old_y - 8.0f` is scaled by 0.03125f (0x470d20) * 128.0f (0x470bf8)
   and rounded half-away-from-zero (0x463b2c) into the alpha byte
   (+0x2ff, low byte only — a negative drop wraps), unless the drop is
   >= 3.0f (0x470bcc) which pins 0xff. Script change check uses base
   0x161.
3. Otherwise the script change check uses base 0x157 and a respawn
   writes alpha = 0xff.
4. The VM spawn is 0x0043e5a0 (ECX = pool, EAX = slot, EDX = script id
   + base) whenever the sign-extended +0x384 word no longer matches.
5. Every live slot ends with the render dispatch 0x004451c0 (ECX =
   DAT_00491c10, EAX = slot).

The two small gate wrappers 0x41ba00/0x41ba30 check DAT_00477810+0x58
flags (bits 0/2/0x400 skip; bit 4 skips via 0x41ba30) before tail-jumping
into 0x41b8e0 and stay thunk boundaries.

## 0x0041c330 TickEffectNodeList

Native input is the effect-node container in EDI (plain ret, EAX = 1).
The container keeps its node list head at +0x18, tail at +0x434 and node
count at +0x438. Nodes: +0 vtable, +4/+8 links, +0xc kind dword,
+0x14 timer int, +0x18 accumulator float, +0x1c rate pointer, +0x50
finish latch byte.

Per node, with `next` captured before any mutation:

- A non-zero latch is incremented and stored; reaching 2 (always, unless
  the byte wraps 0xff -> 0 — the native `inc` does not influence the
  carry, and a wrapped 0 falls through to the kind check) runs the
  finish vtable slot (+0x10) and removes the node.
- Kind 1 nodes always finish and remove.
- Otherwise the update vtable slot (+0x08) runs with ECX = node; a
  non-zero result finishes/removes, zero advances the timer record:
  publish +0x10 = +0x14, then in the 0.99f (0x470b68) .. 1.01f
  (0x470b64) window the timer and accumulator step by 1
  (0x470afc), otherwise the accumulator += *rate and the timer is the
  half-away-from-zero rounding of the new accumulator (0x463b2c).
- Removal decrements container+0x438, repairs both link neighbours,
  resets container+0x434 to the previous node when the tail was removed,
  and frees the node (0x4524a1).

The gate wrappers 0x41c450/0x41c480/0x41c4e0 (render-only walk, the
latched tick with the frame-time-scale save/zero/restore when
DAT_00477810+0x58 bit 0x2 is set, and the render walk gated on bit 0x4)
remain thunk boundaries.
