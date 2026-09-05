# Registration Draw Render Adapter Evidence

## Scope

This note recovers the conservative text-rendering boundary used by the
`RegistrationDrawOwner` FPS callback at `0x004135d0`.  It covers
`0x00401690`, its immediate queueing dependencies, and the ASCII manager draw
callbacks needed to establish when the queued FPS text is consumed.  Names are
descriptive; no original C++ symbols are present in `resources/th10.exe`.

The facts below come from direct disassembly of `0x00401000-0x00401c8a`, the
FPS callback at `0x004135d0-0x0041369a`, and representative callers at
`0x0040a940` and `0x0042c360`.

## Relevant manager slices

`0x00401000` constructs and publishes the `0x89ac`-byte ASCII manager at
`0x004776e0`.  The fields used by this adapter are:

```cpp
struct Float3 {
    float x;
    float y;
    float z;
};

struct AsciiTextEntry {               // sizeof = 0x68
    char text[0x40];                  // +0x00
    Float3 position;                  // +0x40
    unsigned int color;               // +0x4c
    float scale_x;                    // +0x50
    float scale_y;                    // +0x54
    unsigned int unknown_58;          // +0x58, not written by insertion
    unsigned int gui_mode;            // +0x5c
    unsigned int selected;            // +0x60
    unsigned int text_mode;           // +0x64
};

struct AsciiManagerAdapterSlice {
    unsigned char opaque_0000[0x76c];
    AsciiTextEntry primary_entries[256]; // +0x76c
    unsigned char opaque_6f6c[0x1a00];
    int primary_count;                // +0x896c
    int secondary_count;              // +0x8970
    unsigned int color;               // +0x8974
    float scale_x;                    // +0x8978
    float scale_y;                    // +0x897c
    unsigned int gui_mode;            // +0x8980
    unsigned int unknown_8984;        // +0x8984
    unsigned int text_mode;           // +0x8988
    unsigned int glyph_height_units;  // +0x898c, constructor writes 9
};
```

The constructor initializes `color` to `0xffffffff`, both scales to `1.0f`,
`unknown_8984` to zero, and `glyph_height_units` to nine.  It does not prove
the original semantic names of `gui_mode` or `text_mode`.

The calculation callback `0x004014f0` clears both counts and increments the
dword at `+0x8990`.  The primary-text draw callback is registered at priority
48 and enters `0x00401760`; a separate secondary-text draw callback is
registered at priority 38 and enters `0x00401a50`.  The registration owner is
itself a draw callback at priority 47.  Since the scheduler is ascending by
priority, the owner queues its FPS entry before the priority-48 primary draw
callback processes that primary entry in the same draw dispatch.

## `0x00401690`: formatted primary entry with flag

`0x00401690` formats into a 512-byte stack buffer through `0x004524a6`
(`vsprintf`-like behavior) and then calls `0x00401530`.  This is a queueing
operation, not an immediate renderer call.

Its real ABI is not an ordinary C++ variadic member function:

```text
input:  ESI = AsciiManager *manager
        EBX = Float3 *position
        [ESP+4] = const char *format
        [ESP+8...] = variadic arguments
output: no caller-observed result
stack:  plain ret; caller removes format and variadic arguments
```

After formatting, `0x00401690` invokes `0x00401530` with `ECX = ESI`,
`EBX = position`, and `EAX = stack_buffer`.  `0x00401530` appends to the
primary array only when `primary_count < 256`; it copies the NUL-terminated
formatted buffer with no bound check, copies the three-float position, and
snapshots the manager's current color, scales, GUI mode, and text mode.  It
sets the new entry's `selected` word to zero.

`0x00401690` then writes one to:

```text
manager + primary_count * 0x68 + 0x764
```

Because `primary_count` was already incremented by a successful append, this
is `primary_entries[primary_count - 1].selected`.  Thus the most direct
behavioral name is `QueuePrimaryFormatSelected`, not a claim that it renders
through the separate secondary queue.

There is a material edge case.  `0x00401690` does not check whether
`0x00401530` accepted the string.  At a full primary queue (`primary_count ==
256`), insertion does nothing and this final store instead sets the selected
word of entry 255.  A semantic C++ adapter that merely drops the request when
full would differ from the executable.  The same entry insertion also uses
unbounded `vsprintf` and unbounded `strcpy`; both are original behavior, not a
safe input contract.

## FPS call site

At `0x004135d0`, after the timing update and excluding only global state 14,
the callback loads `ESI = *(AsciiManager **)0x004776e0`.  When non-null it:

1. Writes one of `0xff5050ff`, `0xffa0a0ff`, or `0xffffffff` to
   `manager+0x8974` according to the literal x87 comparison path.
2. Materializes `Float3{590.0f, 470.0f, 0.0f}` on its stack and puts its
   address in `EBX`.
3. Pushes format `"%2.1ffps"` and the `sampled_fps` value widened to `double`.
4. Calls `0x00401690` with `ESI` and `EBX` as above.
5. Reloads global `0x004776e0` and unconditionally writes `0xffffffff` to
   `+0x8974`.

The post-call write is a reset to the known default, not a save-and-restore of
the former color.  It relies on the manager remaining alive throughout the
call.  The primary entry already contains the color snapshot, so that reset
does not alter the queued FPS entry.

The `%2.1ffps` bytes are at `0x0046d198`.  The same register/stack calling
pattern and color-then-reset convention occur at non-FPS callers including
`0x0040a940` and `0x0042c360`; this adapter is general UI text infrastructure.

## Consumption boundary

`0x00401760` iterates `primary_count` entries at `manager+0x76c`.  It reads
the copied position, color, scales, GUI mode, selected word, and text mode
from each entry while producing renderer work through the opaque render
services.  In particular it branches on `entry+0x60 == 1`, so the bit written
by `0x00401690` is consumed by the primary draw path.  This proves that the
FPS string is a flagged primary entry; it does not yet prove a user-facing
meaning for the flag such as focus, highlight, or selection.

The adjacent helper at `0x004015c0` appends instead to the secondary storage
starting at `manager+0x6f6c`, with an independent cap of 64 and count at
`+0x8970`.  Its format wrapper at `0x00401700` calls it with `ESI=manager`,
`EBX=position`, and formatted text in `ECX`.  `0x00401a50` consumes that
secondary collection.  `0x00401690` neither calls this helper nor changes
`secondary_count`; it must not be modeled as an alias for the secondary-text
API.

## Conservative C++ boundary

Ordinary C++ can express the semantic behavior, while a narrow assembly or
compiler-specific wrapper is needed only at the nonstandard executable ABI
boundary.  The following names deliberately avoid claiming the original class
or selected-flag semantics:

```cpp
class AsciiRenderAdapter {
public:
    // Semantic form of 0x00401690.  The executable ABI is ESI/EBX plus varargs.
    void QueuePrimaryFormatSelected(
        AsciiManagerAdapterSlice *manager,
        const Float3 &position,
        const char *format,
        ...);

    // Semantic form of the 0x004135d0 text portion.  Caller computes color.
    void QueueFpsOverlay(
        AsciiManagerAdapterSlice *manager,
        unsigned int color,
        float sampled_fps);
};
```

`QueueFpsOverlay` should set `manager->color`, queue the fixed position and
`"%2.1ffps"` format with `double(sampled_fps)`, then write
`0xffffffff` rather than restoring a saved value.  It should not call a D3D
API directly, and it should not redirect the request to the secondary queue.
For executable-faithful behavior it must preserve the selected-store behavior
at capacity and keep the `vsprintf`/copy risk isolated rather than silently
claiming that the original has bounded formatting.

The raw ABI entry should remain separate from this semantic interface:

```cpp
// Needs a dedicated x86 register-adapting thunk; not __thiscall or __cdecl.
extern "C" void QueuePrimaryFormatSelectedEsiEbxAbi();
```

Declaring it as a normal variadic member method would pass manager and
position differently and is therefore unsuitable for object-code matching.

## Verification sources

- `resources/th10.exe`, `objdump -D -Mintel`, ranges
  `0x00401000-0x00401c8a`, `0x0040a940-0x0040ab20`,
  `0x004135d0-0x0041369a`, and `0x0042c360-0x0042c570`.
- `resources/th10.exe`, `objdump -s`, range `0x0046d180-0x0046d1b0` for the
  literal `%2.1ffps`.
- `docs/evidence/callback-scheduler.md` for draw priority ordering and
  `docs/evidence/registration-draw-owner.md` for the owning callback's ABI.
