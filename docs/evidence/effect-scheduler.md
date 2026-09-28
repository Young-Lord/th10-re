# Effect command scheduler (0x00449a20-0x0044a030, 0x0043cb00)

Module: `src/EffectScheduler.cpp`

Scheduler root `DAT_00491be4` with two sorted doubly-linked lists (anchors
at root+0x14 and root+0x38); node records are 0x24 bytes: key +0x00, flags
+0x04 (bit0 allocated, bit1 queued), payload +0x08..+0x10, list-A
prev/next +0x14/+0x18, list-B prev/next +0x1c/+0x20.

- `0x00449ae0`/`0x00449b70`: sorted insert invoking the node constructor
  pointer (+0x0c, called on the +0x20 word) once and clearing it, under
  the `DAT_00492274` critical section with the `DAT_0049231c` nesting
  counter.
- `0x00449ed0`: allocates the node (CRT heap via 0x00452493), initializes,
  seeds +0x08 and sets the allocated bit.
- `0x0044a000`/`0x0044a030`: queue a command (timer word EAX, sort key
  EDI, payload on stack) into list A/B.
- `0x00449f60`: searches list A then list B, unlinks the node and frees it
  when the allocated bit is clear — native quirk: the +0x08..+0x10 words
  are cleared in both branches.
- `0x00449e50`: sweeps list A from root+0x18 detaching every node.
- `0x0043cb00`: detaches the two nodes at holder+8/+0xc under the lock.
- `0x00449a20`: tick counter increment.
