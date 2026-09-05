# Main-Chain Startup Global Reset (`0x004216f0`)

## Scope and evidence boundary

This note covers exactly the 512-byte global storage interval
`0x00491d7c..0x00491f7b`; the requested final dword begins at
`0x00491f78`.  `0x004216f0` is its fixed startup reset routine.

The evidence establishes storage extents and several rendering-facing uses,
but **does not establish an owning C++ object**.  In particular, the two
contiguous records below must not yet be made members of `MainChainContext`,
the renderer, or a guessed camera class merely because they are consumed by
rendering code.

## ABI and control flow

| Property | Fact |
| --- | --- |
| Entry | `0x004216f0` |
| Inputs | No register or stack input is read before it is overwritten. |
| Stack | Reserves 12 bytes (`sub esp, 0xc`) as three dword constant temporaries. |
| Calls / branches | None.  The body is straight-line stores only. |
| Exit | Restores `ESP` and executes plain `ret` at `0x004218c0`. |
| Register result | The final values of `EAX`, `ECX`, and `EDX` are all zero.  No caller examined an integer return value in the inspected startup path, so the semantic C++ signature is `void`. |
| Preserved registers | `EBX`, `ESI`, `EDI`, and `EBP` are not modified. |

There are no allocations, no teardown operations, no synchronization, and no
implicit bulk clear.  Every changed word is listed below; words omitted from
the write table retain their prior contents.

## Exact writes

`0x3f060a92` is the IEEE-754 `float` value approximately `0.52359879f`
(`pi / 6`).  `0x447a0000` is `1000.0f` and `0x3f800000` is `1.0f`.

| Absolute address | Relative block offset | Stored bits | Conservative value spelling |
| --- | ---: | ---: | --- |
| `0x491d7c` | A `+0x00` | `0x00000000` | `0` |
| `0x491d80` | A `+0x04` | `0x00000000` | `0` |
| `0x491d84` | A `+0x08` | `0x447a0000` | `1000.0f` |
| `0x491d88` | A `+0x0c` | `0x00000000` | `0` |
| `0x491d8c` | A `+0x10` | `0x00000000` | `0` |
| `0x491d90` | A `+0x14` | `0x00000000` | `0` |
| `0x491d94` | A `+0x18` | `0x00000000` | `0` |
| `0x491d98` | A `+0x1c` | `0x3f800000` | `1.0f` |
| `0x491d9c` | A `+0x20` | `0x00000000` | `0` |
| `0x491db8` | A `+0x3c` | `0x00000000` | `0` |
| `0x491dbc` | A `+0x40` | `0x00000000` | `0` |
| `0x491dc0` | A `+0x44` | `0x00000000` | `0` |
| `0x491dc4` | A `+0x48` | `0x3f060a92` | `0.52359879f` |
| `0x491e48` | A `+0xcc` | `0x00000020` | `32` |
| `0x491e4c` | A `+0xd0` | `0x00000010` | `16` |
| `0x491e50` | A `+0xd4` | `0x00000180` | `384` |
| `0x491e54` | A `+0xd8` | `0x000001c0` | `448` |
| `0x491e58` | A `+0xdc` | `0x00000000` | `0.0f` |
| `0x491e5c` | A `+0xe0` | `0x3f800000` | `1.0f` |
| `0x491e60` | A `+0xe4` | `0x00000000` | `0` |
| `0x491e94` | B `+0x00` | `0x00000000` | `0` |
| `0x491e98` | B `+0x04` | `0x00000000` | `0` |
| `0x491e9c` | B `+0x08` | `0x447a0000` | `1000.0f` |
| `0x491ea0` | B `+0x0c` | `0x00000000` | `0` |
| `0x491ea4` | B `+0x10` | `0x00000000` | `0` |
| `0x491ea8` | B `+0x14` | `0x00000000` | `0` |
| `0x491eac` | B `+0x18` | `0x00000000` | `0` |
| `0x491eb0` | B `+0x1c` | `0x3f800000` | `1.0f` |
| `0x491eb4` | B `+0x20` | `0x00000000` | `0` |
| `0x491ed0` | B `+0x3c` | `0x00000000` | `0` |
| `0x491ed4` | B `+0x40` | `0x00000000` | `0` |
| `0x491ed8` | B `+0x44` | `0x00000000` | `0` |
| `0x491edc` | B `+0x48` | `0x3f060a92` | `0.52359879f` |
| `0x491f60` | B `+0xcc` | `0x00000000` | `0` |
| `0x491f64` | B `+0xd0` | `0x00000000` | `0` |
| `0x491f68` | B `+0xd4` | `0x00000280` | `640` |
| `0x491f6c` | B `+0xd8` | `0x000001e0` | `480` |
| `0x491f70` | B `+0xdc` | `0x00000000` | `0.0f` |
| `0x491f74` | B `+0xe0` | `0x3f800000` | `1.0f` |
| `0x491f78` | B `+0xe4` | `0x00000001` | `1` |

## Complete interval layout

The storage divides without a gap at `0x491e94`:

| Block | Address interval | Size | Direct structural evidence |
| --- | --- | ---: | --- |
| A | `0x491d7c..0x491e93` | `0x118` | `FUN_00402230` copies exactly `0x46` dwords from A into an object-local buffer; `FUN_00402720` copies exactly those `0x46` dwords back. |
| B | `0x491e94..0x491f7b` | `0x0e8` | Its final required word is at `+0xe4`; no evidence here proves that the following global belongs to B. |

Both blocks have a 24-byte `D3DVIEWPORT9`-compatible subobject at `+0xcc`.
The concrete evidence is the virtual `IDirect3DDevice9::SetViewport` call with
`block + 0xcc` in the existing draw paths.  `D3DVIEWPORT9` is a six-dword
Win32/D3D9 layout:

```text
+0x00 DWORD X          +0x0c DWORD Height
+0x04 DWORD Y          +0x10 float MinZ
+0x08 DWORD Width      +0x14 float MaxZ
```

This proves the offset and representation, not that either containing block
is a specific known camera type.

Every non-written part of the requested 512-byte interval is accounted for
below.  These are retained, not zero-filled, by `0x004216f0`.

| Block | Relative dword ranges left unchanged | Exact address ranges |
| --- | --- | --- |
| A | `+0x24..+0x38`, `+0x4c..+0xc8`, `+0xe8..+0x114` | `0x491da0..0x491db4`, `0x491dc8..0x491e44`, `0x491e64..0x491e90` |
| B | `+0x24..+0x38`, `+0x4c..+0xc8` | `0x491eb8..0x491ecc`, `0x491ee0..0x491f5c` |

The following limited semantic observations are justified by direct uses:

- A `+0x00/+0x04/+0x08` is read as three `float` coordinates by
  `FUN_00443fb0` and `FUN_004445c0`; each computes a Euclidean distance from
  this triple.  It is therefore safe to describe it as a coordinate triple,
  but not yet as a named camera position.
- A `+0x24/+0x28/+0x2c` is exposed as script values `0x2723..0x2725` by
  `FUN_0043eac0`.  The reset routine intentionally leaves it unchanged.
- A `+0xe8/+0xec` is reset by `MainChainDrawFinalize` (`0x004200d0`), and A
  `+0xf0/+0xf4/+0xf8` are read as floats and added to object coordinates by
  `FUN_0043ee30` and `FUN_0040dc80`.  Their owner and broader role remain
  unproven.
- B `+0xcc` has the startup viewport `(0, 0, 640, 480, 0.0f, 1.0f)`.  A
  `+0xcc` instead starts at `(32, 16, 384, 448, 0.0f, 1.0f)`.

## Conservative C++ representation

The following preserves every address and explicitly avoids an ownership
claim.  `u32` should be a 32-bit unsigned type and `f32` an IEEE-754
single-precision type in the reconstruction's platform header.

```cpp
struct StartupViewport {
    u32 x;
    u32 y;
    u32 width;
    u32 height;
    f32 min_z;
    f32 max_z;
};

struct StartupGlobalBlockA {
    u32 before_viewport[0x33]; // +0x000 .. +0x0cb
    StartupViewport viewport;  // +0x0cc .. +0x0e3
    u32 after_viewport[0x0d];  // +0x0e4 .. +0x117
};

struct StartupGlobalBlockB {
    u32 before_viewport[0x33]; // +0x000 .. +0x0cb
    StartupViewport viewport;  // +0x0cc .. +0x0e3
    u32 trailing_word;         // +0x0e4
};

struct MainChainStartupGlobalStorage {
    StartupGlobalBlockA block_a; // 0x00491d7c
    StartupGlobalBlockB block_b; // 0x00491e94
};

typedef char AssertStartupViewportSize[
    sizeof(StartupViewport) == 0x18 ? 1 : -1];
typedef char AssertStartupGlobalBlockASize[
    sizeof(StartupGlobalBlockA) == 0x118 ? 1 : -1];
typedef char AssertStartupGlobalBlockBSize[
    sizeof(StartupGlobalBlockB) == 0x0e8 ? 1 : -1];
typedef char AssertMainChainStartupGlobalStorageSize[
    sizeof(MainChainStartupGlobalStorage) == 0x200 ? 1 : -1];
```

The `before_viewport` arrays deliberately remain raw words.  The reset's
first fields can be accessed through offset constants or conservative helper
references after a future owner/type recovery; declaring them now as a vector,
matrix, or projection object would overstate the evidence.

## Semantic C++ implementation

This is the behavior of the binary, expressed without depending on an
unproven owner.  It deliberately performs stores only; it must not replace
the routine with `memset`, value-initialize either block, or assign a complete
viewport-bearing object, because the binary preserves the listed holes.

```cpp
void ResetMainChainStartupGlobals(MainChainStartupGlobalStorage& globals) {
    u32* const a = globals.block_a.before_viewport;
    u32* const b = globals.block_b.before_viewport;

    a[0] = 0; a[1] = 0; a[2] = 0x447a0000;
    a[3] = 0; a[4] = 0; a[5] = 0; a[6] = 0;
    a[7] = 0x3f800000; a[8] = 0;
    a[15] = 0; a[16] = 0; a[17] = 0; a[18] = 0x3f060a92;
    globals.block_a.viewport.x = 32;
    globals.block_a.viewport.y = 16;
    globals.block_a.viewport.width = 384;
    globals.block_a.viewport.height = 448;
    globals.block_a.viewport.min_z = 0.0f;
    globals.block_a.viewport.max_z = 1.0f;
    globals.block_a.after_viewport[0] = 0;

    b[0] = 0; b[1] = 0; b[2] = 0x447a0000;
    b[3] = 0; b[4] = 0; b[5] = 0; b[6] = 0;
    b[7] = 0x3f800000; b[8] = 0;
    b[15] = 0; b[16] = 0; b[17] = 0; b[18] = 0x3f060a92;
    globals.block_b.viewport.x = 0;
    globals.block_b.viewport.y = 0;
    globals.block_b.viewport.width = 640;
    globals.block_b.viewport.height = 480;
    globals.block_b.viewport.min_z = 0.0f;
    globals.block_b.viewport.max_z = 1.0f;
    globals.block_b.trailing_word = 1;
}
```

Using hexadecimal writes for the two initial `1000.0f`, `1.0f`, and
`pi / 6` bit patterns preserves the actual word-level behavior without
prematurely assigning types to their containing header fields.  The viewport
`MinZ`/`MaxZ` are typed because their D3D9 layout is independently proven.

## Reconstruction constraints

- Keep this as a free reset function over a storage reference until a caller
  and allocation/ownership analysis proves the aggregate owner.
- Do not infer the identity of A and B from their similar default fields;
  their extents and non-viewport offsets differ.
- Do not clear the unchanged spans, including the script-visible A
  `+0x24/+0x28/+0x2c` words and the rendering-adjacent tail of A.
- The function is semantically `void`; an eventual exact ABI wrapper may
  preserve the binary's scratch-stack and register details separately.
