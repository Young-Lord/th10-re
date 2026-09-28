# Menu state helpers (0x00434a20-0x0043cb80)

Module: `src/MenuStateHelpers.cpp`

- `0x00434a20 TrimLineAtNewlineEcxEdiStackAbi`: ECX = line, EDI =
  remaining-byte counter, stack = buffer end. Terminates the line at
  `\n`/`\r`, memmoves the remainder (including the terminator) back over
  the buffer, then skips any following CR/LF run while decrementing the
  counter. Used three times by RunManagerStateBodyE (0x00433ef0) while
  parsing text lines. NOTE: the comments in `GameManagerStateBodies.cpp`
  attribute 0x00434a20/0x00434a80 to `ReleaseScoreDisplayRecord` /
  `UpdateScoreDisplayEntity`; the reference disassembly disproves those
  attributions — these are the real bodies of those addresses.
- `0x00434a80 BeginManagerVectorTweenEdxCcxStackAbi`: EAX = manager,
  EDX = start triple (manager+0x334 at the call sites), ECX = target
  triple, stack (ret 8) = kind word + flag byte. Writes manager+0x70..0xb8:
  start/target triples (+0x70/+0x7c), two copies of the time words
  `DAT_00491c14..1c` (+0x88/+0x94), kind +0xb4, flag +0xb8, and primes the
  eased interpolation sub-record at +0xa0 (-999999, easing table
  0x00476f78, flag |1, then -1 target).
- `0x00434ba0` toggles bit 0x10 of record+0x60; `0x00434bc0` appends to
  the disabled-row list at +0x90 (counter +0xd4).
- `0x0043cb80 NextMenuRandomEcxEdiAbi`: u16 LCG `(state ^ 0x9630) - 25939`
  advanced twice per call (dword counter += 2), result
  `(second | first << 16) % modulus`; 0 when modulus 0.
