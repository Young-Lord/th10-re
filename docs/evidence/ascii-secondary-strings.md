# Ascii Secondary Strings (0x4014d0..0x401d90)

Module: `src/AsciiSecondaryStrings.cpp`. The AsciiManager keeps a 64-entry
secondary-string ring at +0x6f6c (0x68-byte entries, live count at +0x896c),
the current color/scale/is_gui state at +0x8974..+0x8984 and a frame tick
counter at +0x8990.

- `0x004014d0` `ClearAsciiSecondaryStringsAndTickEaxAbi` — native EAX =
  manager, returns 1 (callers ignore it). Clears the +0x896c live count and
  the +0x8970 mirror, then increments the +0x8990 frame tick.
- `0x004014f0` `ClearAsciiSecondaryStringsAndTickEcxAbi` — identical body
  with ECX = manager; modeled as a tail call to the EAX form.
- `0x00401d90` `ClearAsciiSecondaryStringsEaxAbi` — clears the two counters
  without the +0x8990 tick and without a return value.
- `0x004015c0` `AddAsciiSecondaryStringEsiAbi` — native ESI = manager,
  ECX = text, EBX = source Float3. Silently drops the append once the live
  count reaches 0x40. Copies the text byte by byte (including the
  terminator), the three position floats, the manager's current
  color/scale_x/scale_y/is_gui state, zeroes the entry's is_selected word,
  and leaves the trailing entry fields untouched (evidenced native
  partial-init quirk).
- `0x00401700` `AddAsciiSecondaryStringFormattedEsiStackAbi` — native
  stdcall (stack: format, va_list; ret 8): vsprintf into a 0x200-byte stack
  buffer, then a tail call to 0x004015c0 with the buffer and its position.
  The vsprintf/va_list bridge stays a native thunk; the semantic anchor is
  registered so the address resolves in the status CSV.

Native register ABIs (EAX/ECX/ESI) remain thunk boundaries per the project
ABI rule; all field offsets verified against the canonical binary.
