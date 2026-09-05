# ZunMath Evidence

`0x0041f800` receives an output pointer in `ECX`, angle and length as two stack
floats, and returns with `ret 8`. Its x87 sequence is `fsincos`, then multiplies
and stores cosine to output `+0` and sine to output `+4`.

MCP callers include `0x0041cfd0`, which passes fields at `+0x3c/+0x40` and
adds the resulting vector to position fields. The name `SetVectorFromAngle` is
an analysis name describing this fully observed behavior; it does not assert
the original source declaration or module ownership.

`ZunMath.obj` was exported with `ExportTh10Delinker.java` and compared with
the NASM object on 2026-08-28. Its `.text` sections are both 28 bytes and
objdiff reports `100.0%`; their disassemblies are byte-identical.

`0x0041f7a0` tests a point plus/minus supplied X/Y radii against four global
playfield bounds. It returns zero only when all tests pass and one when any
test reaches an outside branch. Its exact x87 condition behavior is preserved
in `ZunMathBounds.asm`; it has not been simplified to C++ comparisons.

`ZunMathBounds.obj` was compared on 2026-08-28. The current exporter records
the function body as 91 bytes, with no trailing alignment byte; objdiff
reports `100.0%`, and the original/rebuilt `.text` sections compare
byte-for-byte equal.
