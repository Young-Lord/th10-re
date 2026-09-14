# Hint-Text File Cluster (TH10 0x0041a200, 0x00419960, 0x00419040, 0x00419120)

Implemented in `src/HintTextFile.cpp/.hpp` with the small helpers
0x0041a0a0, 0x004198c0, 0x00418d00, 0x0041ab10, 0x0041ab60, 0x00419f40
and 0x0041ab70 (the last as a boundary). This corrects the earlier
declaration-level reading in `hint-text-loader.md`: 0x0041a200 is the
template *writer* (not a loader) and 0x00419040 is the stage-list
teardown. `src/HintTextLoader.cpp` (0x00418ee0) now calls both semantic
bodies.

## Tip record (0x88 bytes)

Position floats +0/+4, list node at +0x0c (self +0x0c, next +0x10,
prev +0x14), slot dword +0x18/+0x24, text at +0x1c (0x41-byte copy),
count +0x60, align +0x64, base +0x68, time +0x6c, width +0x80, scale
+0x7c, color bytes +0x84/+0x85/+0x86 (B,G,R), alpha +0x87. The stage
lists live in 12-byte stride slots: 16 heads at state+100+12*i (even,
the duplicate list) and state+124+12*i (odd, the primary list), each
holding the first node pointer.

## 0x0041a200 WriteHintTextTemplateStackAbi

`__stdcall(state, file_name)`. malloc(0x1000) line buffer; `_mkdir("hint")`
(0x452abc boundary); when 0x0044b620(file name) reports failure the body
skips straight to the tail. Otherwise it appends (strlen + 0x0044b740
boundary, native ESI = length / EDX = buffer):

- the Shift-JIS header lines "# ===...", "# 東方風神録　攻略ヒント
  ファイル", "#", "# このファイルは、自動的に生成、上書きされます",
  "# 自分で攻略ヒントを構成したい場合は、別のファイルを使用して
  ください", "#" (embedded byte-exact from 0x46d534/0x46d510/0x46d4d8/
  0x46d490),
- the time-stamp line `# Time-stamp: <%.4d/%.2d/%.2d %.2d:%.2d>` from
  time/localtime,
- `\r\n\r\n` and `Version = 0.0\r\n\r\n`.

Then eight stage walks over state+124+12*i: tips with count (+0x70) == 0
are skipped entirely; the first tip emits the `# ===...` rule and
`Stage : %d`; every tip emits `Tips`, optional `Remain` (count > 0),
`Text` (quoted), `Pos` (floats truncated to int), `Count`, `Base`
(reverse lookup in the 61-entry table off_4744b0) and `Align` (3-entry
table off_474494), `Time`, `Alpha`, `Color` (r,g,b order in the file),
`Scale` (%.1f) and `End`; a non-empty stage closes with `StageEnd`.
Tail: free the buffer, return 0.

Decompiler note: the MultiByteToWideChar/QueryPerformanceCounter blocks
IDA shows between the line appends are artifacts of the 0x0044b740
usercall body; the boundary models the observable strlen/append idiom.

## 0x00419960 ParseHintTextFileStackAbi

`__stdcall(state, file_name, duplicate_flag)`, returns 0 (or -1 when
0x0044b360 cannot load the file; that boundary also publishes the size).
Three malloc(0x1000) buffers (line/key/value). Loop over
0x00419f40 lines: split at ':' (the native runs the copy/strchr/trim
sequence twice back to back with identical results — modeled once),
trim both halves (0x0041a0a0), then dispatch:

- `Version` — when the value is not "0.0", run 0x00419040 (clears the
  lists) and keep parsing.
- `Stage` — stage = atol(value); tip count reset; out of 1..7 -> -1.
- `StageEnd` — stage = -1.
- `Tips` (stage > 0): count >= 255 sets stage = -1; otherwise operator
  new(0x88) + 0x00418d00 ctor, ++count, +0x18 = 0, then an inner line
  loop (0x0041a050 split boundary) consuming Pos (comma split, atol ->
  +0/+4 floats), Text (first/last '"' pair, strncpy 0x41 to +0x1c),
  Count/Time (+0x60/+0x6c), Base/Align (0x004198c0 keyword lookup with
  EDX = value), Remain (+0x70), Scale (atof: zero -> 1.0f, sub-normal/
  negative-magnitude -> -1.0f, else the parsed value), Color (three
  atol bytes into +0x86/+0x85/+0x84), Alpha (+0x87) until `End`.

After each tip: width +0x80 = (double)strlen(text) * scale (the native's
negative-length +64 correction cannot fire for real strings), append the
tip node (+0x0c) to the stage list at state+124+12*stage through
0x0041ab10, and — when the duplicate flag is clear — a 0x88 memcpy copy
(with the node self-pointer re-seeded and next/prev cleared) appended to
the paired list at state+100+12*stage. Tail frees the four buffers and
returns 0.

## 0x00419040 FreeHintTipListsEaxAbi

EAX = state. For 8 iterations (12-byte stride from state+0x7c) it frees
both paired lists (heads at cursor-24 and cursor): each node releases
`*(node)` with the shared free and follows node+4 until 0. Returns the
last next pointer (0).

## 0x00419120 UpdateTipEntitiesUsercall

usercall: EAX = the tip-list walker ({tip, next} pairs), EDI = style
byte (forwarded to the text entities' vararg slot), stack = (scene
record, free flag). Per tip: when base (+0x68) is 0 or the current scene
id (DAT_00474cb4) the count (+0x60) decrements; on expiry:

- the scene handle ring (scene+0x1a4 index, handles at scene+0xdc) slot
  is released through the inlined ReleaseEntityById body
  (entity +0x35c |= 0x4000000 with the +0x14 child-chain propagation —
  the shared `ReleaseEntityById` is used) and cleared,
- y += scale * 2, then a replacement entity spawns via 0x00448db0
  (kind ring+2 when scale <= 0.81399995f, ring+12 above) with the
  entity alpha bytes +0x3a0/+0x3a1 set to 15 / 30 / (u8)(scale*0.2)
  for the three scale bands, the motion record seeded through the
  0x0041ab70 boundary and the handle slot cleared for the small band
  (native quirk: the manual list-search path nulls the slot and may
  leave a null entity that the following writes dereference),
- the entity's +0x340 triple and the tip width publish into the scene
  position/width slots, the alignment id (0/1/2) selects the
  0x00447bb0/0x00447a50/0x00447ae0 text entity (color 0xffffff, text at
  tip+0x1c) with the +0x35c alignment bits 0x40000/0x80000 and the
  x-position offset of +/- width*192, and the tip's +0x84 (color word),
  +0x2ff zero byte, +0x30c (time) and +0x310 (alpha byte) fields land on
  the entity,
- the ring index advances `(ring + 1) % 10`, the tip unlinks from its
  list (prev/next at +0x14/+0x10) and is freed when the free flag is set.

## Reconstructed helpers

- 0x0041a0a0 TrimHintStringEaxAbi (space/tab/LF/CR, in place; the
  trailing loop writes the terminator one past the index, preserved).
- 0x004198c0 LookupHintKeywordEdxStackAbi (8-byte {name, value} pairs).
- 0x00418d00 ConstructTipRecordEdxAbi (0x88 zero + node self pointer,
  -1 at +0x84/+0x70, 300 at +0x6c, 1.0f at +0x7c).
- 0x0041ab10 AppendTipListNodeEaxEdxAbi (tail append; EAX = head holder
  minus 4, node next +4 / prev +8).
- 0x0041ab60 GetTipSlotEaxAbi (returns tip+0x24).
- 0x00419f40 ReadHintTextLineUsercall (EOL scan with 4096-byte cap,
  remaining-counter drain, '#' comment strip, skip-until-non-empty loop;
  the final tail copies the remaining bytes and zeroes the counter; the
  native trim is the 0x0041a090 short variant, kept as a boundary).

## Boundaries

0x0044b620 (file create under the resource lock, handle at DAT_00474c38),
0x0044b740 (ESI/EDX line append), 0x0044b360 (whole-file load + size),
0x0045295x/_mkdir (0x452abc), 0x0041a090, 0x0041a050, 0x0041ab70,
0x00448db0, 0x004491c0, 0x00447bb0/0x00447a50/0x00447ae0.

## Notes

- The parser's stage validation (`stage <= 0 || stage >= 8 -> -1`) means
  the file's `Stage : 0` header written by the template is never
  accepted — stage indices are 1-based in the file.
- `UpdateTipEntitiesUsercall`'s EDI style byte is forwarded unchanged;
  the native register/stack split of the 0x0041ab70 seed call is
  modeled as `(entity, {pair})` with the caller-fixed stack values 8/4
  documented in the boundary.
