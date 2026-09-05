# Joystick Config Poll (TH10 0x0044a190)

Implemented as `PollJoystickConfigBitmaskEcxStackAbi` in
`src/JoystickConfigPoll.cpp/.hpp`.

## ABI

- Native entry: `ECX` = accumulated key bitmask (full 32-bit register
  participates in the ORs), `EDX` = dead slot, two stack arguments
  (`ret 8`): config slot index into the 0x6a-stride bank at `0x474e30`,
  and joystick index (0/1).
- The success path returns the full combined 32-bit `EAX`; the failure
  paths execute `mov ax, si` over the failing API result, so the API's
  high word survives and only the low word carries the bitmask. The C++
  body models this exactly via `FailureResult`.

## Config Bank

Slot base `0x474e30 + slot * 0x6a`; words are signed button indices,
negative means unassigned. Mapping:

| word    | result bit |
|---------|------------|
| +0x58   | 0x001 (shot) |
| +0x5a   | 0x002 (bomb) |
| +0x5c   | 0x004 |
| +0x5e   | 0x008 |
| +0x68   | 0x100 |

## winmm Path (`0x491ff4` bit 0x400 clear)

- `joyGetPosEx(joystick_index != 0, &info)` with `dwSize = 0x34`,
  `dwFlags = 0xff` (JOY_RETURNALL); nonzero status returns the failure
  result immediately.
- Buttons: bit set when `(dwButtons & (1 << index)) != 0` for each mapped
  config word.
- Axes use the winmm caps array at `0x4918b8` (stride `0x194`;
  wXmin +0x20, wXmax +0x22, wYmin +0x24, wYmax +0x26). Thresholds are
  32-bit unsigned: middle = `(min+max)>>1`, quarter = `(max-min)>>2`;
  right 0x80 when `mid+quarter < xpos`, left 0x40 when
  `xpos < mid-quarter`; down 0x20 / up 0x10 symmetrically on Y.

## DirectInput Path (bit 0x400 set)

- Device pointer `0x491c3c[index]`; vtable offsets: Poll `+0x64`,
  Acquire `+0x1c`, GetDeviceState `+0x24`.
- On Poll success: `GetDeviceState(0x110, buf)`; failure returns the
  failure result. Buttons are bytes at `buf+0x30`, combined with shift
  masks (`>>7`, `>>6 & 2`, `>>4 & 8`, `>>5 & 4`, `(b & 0x80) << 1` for
  0x100). Axes are signed longs at `buf+0/+4` compared against the
  signed deadzone words `0x491d5e` (X) / `0x491d60` (Y):
  `> +dz` down/right, `< -dz` up/left.
- On Poll failure: Acquire once; while it reports `DIERR_NOTACQUIRED`
  (`0x8007001e`) it retries up to `0x190` times. Every exit from this
  branch — including a successful re-acquire — returns the bitmask
  unchanged; the native never reads state after re-acquiring.

## Callers

`0x44a5f0`, `0x44a9d0`, `0x44ad30` (key-config screens).

## Status

Baselines pass (`scripts/compile-main-chain-cpp.sh`, g++ -m32 -std=c++98
syntax check). IDA name/comment applied at 0x44a190.

## Button-byte poll (TH10 0x0044a4e0)

`PollJoystickButtonBytesEcxEsiAbi` (native usercall: ECX = DirectInput
device index, ESI = value forwarded to Poll) refreshes the 224-byte
button/axis byte array at `0x497bb0`:

- DirectInput path (0x491ff4 bit 0x400): Poll (+0x64, with the forwarded
  ESI value as its second stack slot per the native's usercall ABI);
  on success `GetDeviceState(0x110)` into a local and the first 0xe0
  bytes are copied into the array. On Poll failure the same
  DIERR_NOTACQUIRED re-acquire loop as 0x44a190 runs (up to 0x190
  retries, state never re-read afterwards).
- winmm path: `joyGetPosEx(joystick 0)`; each set bit of `dwButtons`
  writes 0x80 at its bit index in the array (bits 0..31). The device
  index is ignored on this path.

Returns the array base. The key-config screens (0x44a5f0/0x44a9d0/
0x44ad30) consume this array through the 0x474e30 config words.
