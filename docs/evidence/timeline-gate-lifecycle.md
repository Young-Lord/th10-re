# Timeline Gate Lifecycle Evidence

`0x0040b940` allocates a `0x28`-byte gate object, sets flag bit 1, publishes
`DAT_00477700`, and calls `0x0040b560`. Failure runs `0x0040b7b0` and frees the
allocation.

`0x0040b560` registers two scheduler callbacks at priority 3:

- `0x0040ba70` calls `0x0040bd20` on `gate+0x18` and updates `DAT_00491fb8`
  when the controller returns nonzero.
- `0x0040ba80` is a constant-one draw stub.

Initialization also ensures `AsciiManager+0x89a4` has a kind-6 continuation
object, derives `gate+0x1c` from `DAT_00474c68/6c/74/90`, sets audio flag
bytes in `DAT_0047783c`, loads `e00.msg`..`staff.msg` through the packed
archive helper into `gate+0x14`, and seeds a `0xec`-byte inner state through
`0x0040ba90`.

`0x0040ba90` zeroes `0xec` bytes, creates five preset text slots through
`0x00449950`/`0x00449870` using `text.anm` (`AsciiManager+0x899c`) clone fields
`0x42`..`0x46`, validates each handle, stores the first record pointer at
`+0x54`, initializes the three embedded timer blocks, sets flags bit 0 and color
`0xffffff`.

`0x0040b7b0` removes both scheduler records under the global lock, releases
owner cache-pair slot 0 through `0x00447f70`, destroys the inner state through
`0x0040b3a0`, frees four owner work slots at `+0x3ad0e0..+0x3ad0ec`, frees the
stream buffer, and clears `DAT_00477700` and `DAT_004918a4`.
