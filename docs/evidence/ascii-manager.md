# AsciiManager Evidence

## Animation VM reset (`0x00401de0`)

The reusable 0x3ac-byte VM-record reset receives its record in `EDX` and has
no stack arguments. It preserves dwords `+0x20`, `+0x340`, `+0x344`, and
`+0x348`, clears the full record, restores those dwords, then writes the
observed default scale, color, flags, and self-pointer fields. The semantic
body is `ResetAsciiAnimationVmRecord`; its native register ABI remains a
thunk boundary.

## Glyph quad construction (`0x00443080`, `0x00443290`)

Both glyph helpers write X/Y/Z for four 0x1c-byte staging vertices at
`0x4978c0`, respecting the two 2-bit anchors in VM `+0x35c`; anchor value 3
intentionally leaves the existing axis coordinates untouched. The unscaled
path floor-centers the extent and submits with pixel snap enabled. The scaled
path uses half the scaled height and submits without pixel snap. Shared
submission (`0x00442670`) remains an explicit boundary for UV/color, clipping,
texture-state changes, and batch append.

The following facts are derived from the TH10 1.00a database through Ghidra MCP.
Names are user-defined analysis names, not recovered debug symbols.

| Address | Name | Evidence |
| --- | --- | --- |
| `0x00401000` | `AsciiManager::AsciiManager` | Allocated by `0x00401440` with size `0x89ac`; installs a vtable-like pointer, zeroes manager storage, and stores its pointer at `0x004776e0`. |
| `0x00401110` | `AsciiManager::Initialize` | Loads `ascii.anm`, `text.anm`, and `capture.anm`; creates three callbacks and initializes two internal animation VMs. |
| `0x00401260` | `AsciiManager::~AsciiManager` | Releases callback objects and dynamically allocated manager storage; clears `g_AsciiManager`. |
| `0x00401440` | `CreateAsciiManager` | Allocates `0x89ac`, calls constructor then initializer, and destroys/frees on initializer failure. |
| `0x00401530` | `AsciiManager::AddString` | Appends one `0x68`-byte entry at `this + 0x76c`, up to 256 entries. |
| `0x00401630` | `AsciiManager::AddFormatText` | Formats a 512-byte stack buffer with `vsprintf` before calling `AddString`. |
| `0x00401690` | `AsciiManager::AddFormatTextSelected` | Same formatting path, then marks the new entry's `+0x60` field as one. |

## Confirmed Layout Slice

`AsciiManagerString` has size `0x68`. Its observed fields are `text[64]` at
`+0x00`, a three-float position at `+0x40`, color at `+0x4c`, scales at
`+0x50/+0x54`, GUI mode at `+0x5c`, selected mode at `+0x60`, and text mode at
`+0x64`. The word at `+0x58` is not written by `AddString` and remains unknown.

The string array is at `AsciiManager + 0x76c`; it ends at `+0x6f6c`, and the
next observed field, `num_strings`, is at `+0x896c`.
The manager has observed defaults at `+0x8974` (color), `+0x8978/+0x897c`
(scales), `+0x8980` (GUI mode), `+0x8984` (zero), `+0x8988` (text mode), and
`+0x898c` (nine). Fields outside this slice intentionally remain opaque.

## ABI Caveat

At `0x00401530`, the original receives manager in `ECX`, position in `EBX`, and
text in `EAX`; `0x00401630` and `0x00401690` demonstrate this directly. The
C++ member declaration documents semantics and layout but is not yet an
object-match claim. A per-function calling-convention wrapper is required
before using this implementation for strict matching.

## Object Group

`config/ghidra_ns_to_obj.csv` exports `AsciiManagerStrings.obj` from the three
implemented functions by exact address. The `@0x...` form is intentional:
Ghidra MCP stores the current `AsciiManager::` names as flat symbols, while
the exporter accepts both names/namespaces and exact function addresses.

## First Object Diff

On 2026-08-28, `ExportTh10Delinker.java` exported
`build/objdiff/orig/AsciiManagerStrings.obj` from an independently imported
copy of the locked TH10 executable. `scripts/compare-ascii-manager.sh` compiles
the semantic C++ implementation with MSVC 7.1 and writes the objdiff report to
`build/objdiff/ascii-manager-strings.json`.

The initial semantic C++ object match was `0.0%`: its ordinary member-function
stack arguments do not match the original register ABI, inline string copy, or
variadic wrapper setup. This was used as a diagnostic baseline, not counted as
progress.

The NASM ABI layer now produces a `0x13d`-byte `.text` section, equal to the
original COFF text size, and objdiff reports `100.0%` for the object group.
A raw section comparison has only eight differing bytes: the two four-byte
call displacements at original COFF offsets `0xc0` and `0x110`. Delinker
intentionally externalizes those group-internal `AddString` calls as `DISP32`
relocations; NASM resolves them within the rebuilt section. Their surrounding
instructions and all other text bytes are identical. The matched claim is
therefore limited to the exported COFF object group, not the final program.

`src/AsciiManagerStrings.asm` is the next-stage ABI layer. It is intentionally
separate from the C++ semantic source: NASM emits the observed `ECX`/`EBX`/`EAX`
contract and exact instruction sequence, while `AsciiManager.cpp` documents the
same operation in source-level form.

## Lifecycle Reconstruction

`src/AsciiManagerLifecycle.cpp` contains semantic C++ bodies for the five
lifecycle entries. The constructor retains the observed transient pre-clear
accesses before its full 0x89ac-byte zero; the factory deliberately calls the
initializer even when allocation returned null. Initialization uses resource
slots 2, 0, and 3 for `ascii.anm`, `text.anm`, and `capture.anm`, publishes the
three returned pointers at `+0x8994/+0x899c/+0x8998`, and registers disabled
callbacks at calculation priority 4 and draw priorities 48 then 38.

The `0x401520` semantic body draws the secondary queue at `+0x6f6c` (count at
`+0x8970`), not the primary `AddString` queue. It preserves normal byte-level
glyph indexing, newline/space behavior, GUI-mode flush/view transitions, and
the final default-view restoration. Destruction writes its vtable, removes
each owned callback under the scheduler lock/activity protocol, releases
resource slots 2/0/3, then releases its two internal buffers through the
buffer-release boundary before factory-owned outer storage is freed.

The priority-48 callback at `0x401510` instead forwards to primary queue draw
body `0x401760`. It uses `+0x76c/+0x896c`, treats entry `+0x60` as the
font-variant selector and `+0x64` as the shadow-pass flag, and obtains glyphs
from `ascii.anm + 0x118 + (98 * variant + byte - 0x20) * 0x44`.

## VM metadata initialization (`0x0043e5a0`)

The VM initializer accepts `EAX=vm`, `EDX=entry index`, and `ECX=resource`.
It returns `-1` before any VM write when resource `+0x108` is null or
`+0x124` is nonzero. Otherwise it selects `resource+0x118 + index*0x44`,
publishes entry/resource pointers and dimensions, resets two VM parameter
blocks, clones `+0x23c..+0x27b` to `+0x27c..+0x2bb`, and derives normalized
extents from entry fields without validating denominators. In particular, the
second AsciiManager call uses index 98: this is metadata selection, not an
animation-bytecode execution interface.
