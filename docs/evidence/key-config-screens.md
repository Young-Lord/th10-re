# Key Config Screen Input Handlers (0x44a5f0 / 0x44a9d0 / 0x44ad30)

Reconstructed in `src/KeyConfigScreens.cpp/.hpp` as semantic C++ (C++98,
explicit offsets, no byte matching). All three call the already-reconstructed
`PollJoystickConfigBitmaskEcxStackAbi` (0x44a190, `ECX = accumulated mask`,
stack = `(config slot, joystick index)`, `ret 8`).

## Shared structure

All three bodies are the same two-branch keyboard sampler:

1. Gate `dword_4924FC` (`g_MainChainWindowInputEnabled`, 0x4924fc): if zero,
   the keyboard mask is 0 (EDX/ESI cleared) and the joystick poll still runs.
2. `dword_491FF4 & 0x200` selects the DirectInput keyboard path:
   `GetDeviceState` through vtable `+0x24` of `IDirectInputDevice8A` at
   0x491c38, buffer 0x100 bytes, stack-local. On any failure result:
   - if `result == 0x8007001E` (NOTACQUIRED): one `Acquire` (vtable `+0x1c`),
     then proceed with mask 0 — the state is never re-read;
   - otherwise: one `Acquire`, then proceed with mask 0.
3. Otherwise `GetKeyboardState` fills the same 0x100-byte buffer (its return
   value is ignored).

### Key mask tables

The native compiler merged the per-key tests into shr/shl chains; the tables
below were extracted instruction-by-instruction from those chains (bit N of
the mask = entry N; key-state bytes are `0x00/0x80`, and a bit is set when
the high bit of any listed key byte is set):

| mask bit | Virtual-key (GetKeyboardState) | DIK (GetDeviceState) |
|---------|--------------------------------|----------------------|
| 0x0001 (shot)  | 0x5A `Z`            | 0x2C `DIK_Z` |
| 0x0002 (bomb)  | 0x58 `X`            | 0x2D `DIK_X` |
| 0x0004         | 0x10 `Shift`        | 0x2A `DIK_LSHIFT` |
| 0x0008         | 0x1B `Escape`       | 0x01 `DIK_ESCAPE` |
| 0x0010 (up)    | 0x26 `Up` \| 0x68 `Num8` | 0xC8 \| 0x48 |
| 0x0020 (down)  | 0x28 `Down` \| 0x62 `Num2` | 0xD0 \| 0x50 |
| 0x0040 (left)  | 0x25 `Left` \| 0x64 `Num4` | 0xCB \| 0x4B |
| 0x0080 (right) | 0x27 `Right` \| 0x66 `Num6` | 0xCD \| 0x4D |
| 0x0100         | 0x11 `LControl`     | 0x1D `DIK_LCONTROL` |
| 0x0200         | 0x51 `Q`            | 0x10 `DIK_Q` |
| 0x0400         | 0x53 `S`            | 0x1F `DIK_S` |
| 0x0800         | 0x50 `P` \| 0x24 `Home` | 0x19 \| 0xC7 `DIK_HOME` |
| 0x1000         | 0x0D `Return`       | 0x1C `DIK_RETURN` |
| 0x2000         | 0x44 `D`            | 0x20 `DIK_D` |
| 0x4000         | 0x52 `R`            | 0x13 `DIK_R` |

Numpad diagonals are separate and/neg/sbb tests that OR fixed immediate
combinations of the axis bits (up=0x10, down=0x20, left=0x40, right=0x80):
Num7 → 0x0050, Num1 → 0x0060, Num9 → 0x0090, Num3 → 0x00A0 (DIK 0x47/0x4F/
0x49/0x51). Mask bit 15 is never set by the keyboard.

The native VK chain ORs raw bytes before masking bit 7 (e.g. `ks[0x58] & 0xBF`,
`ks[0x5A] >> 1`); with 0x00/0x80 key bytes this is exactly the OR-of-pressed
tests modeled here.

## Function-specific behavior

### 0x44a5f0 `UpdateKeyConfigInputRecordEcxAbi`

- Native ABI: `ECX = config slot` (also forwarded to the joystick poll),
  `EDX` = dead fastcall argument, caller cleans the stack.
- `poll(keyboard_mask, config_slot=ECX, joystick=0)` (`push 0; push ebx`).
- Refreshes the 0x6a-stride record at `0x474e30 + 0x6a*slot`:
  - `+0x02` = old `+0x00`, `+0x00` = poll result low word;
  - `+0x04` = 0, then for each of the 16 result bits: if set, `++counter`
    (u16 counters at `+0x0A + 2i`); when a counter reaches `0x1A` the bit
    enters `+0x04` and the counter is reloaded by `-8` (native
    `add word ptr, 0xFFF8`); when clear the counter is zeroed;
  - `+0x06` = `current & (current ^ previous)` (pressed edge);
  - `+0x08` = `(current ^ previous) & ~current` (released edge).
- Returns the full joystick-poll EAX (callers use the low word).
- Single xref: 0x41ff80.

### 0x44a9d0 `UpdateKeyConfigInputRecordSlotZeroPollEcxAbi`

Byte-twin of 0x44a5f0 (identical key tables, DI failure flow, and record
epilogue; 0x352 bytes each) with one functional difference: the joystick
poll is called with `push 0; push 0` — config slot 0 regardless of the ECX
slot, which still selects the record that gets refreshed. No direct xrefs in
the IDB (likely reached via the menu code the analyzer did not outline).

### 0x44ad30 `PollKeyConfigMenuInputMask`

Mask-only variant (0x2d5 bytes): identical gate, tables, and DI failure
flow, then `poll(mask, 0, 0)` and return. No input record update; both
fastcall argument registers are dead in the native body. No direct xrefs in
the IDB.

## Quirks preserved

- GetDeviceState failure Acquire-once paths (both NOTACQUIRED and generic
  failure) never re-read the state; mask 0 is still fed to the joystick poll.
- GetKeyboardState return value ignored; buffer uninitialized use avoided by
  zero-fill (native leaves the stack buffer as-is but every consumed index is
  written by the sampling call).
- Repeat reload is a subtraction by 8 after crossing 0x1a (not a reset).
- Bit 15 of the keyboard mask is unreachable.
- 0x44a5f0 forwards its own slot to the joystick poll while 0x44ad30/0x44a9d0
  hardcode slot 0 — kept exactly as in the binary.
