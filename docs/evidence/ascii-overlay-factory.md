# ASCII Overlay Factory Evidence

`0x0043c8b0` allocates and zeroes a `0x44`-byte overlay context, sets flag
bit 1 at `+0x00`, and configures it through `0x0043c970`. Native inputs are
five stack words and draw priority in EBX; it returns with `ret 20`. Its
fourth stack word is intentionally discarded, while the fifth is copied to
both context `+0x24` and `+0x28`.

`0x0043c970` receives the context in ESI and draw priority in EBX. It creates
one calculation node at priority 14 and an optional draw node at the caller
priority, then stores cleanup callback `0x0043c870` in the calculation node.
The native kind map is:

| Kind | Calculation | Draw |
| --- | --- | --- |
| 0 | `0x43bd40` | `0x43c1a0` |
| 1 | `0x43c550` | none |
| 2 | `0x43c230` | `0x43c2c0` |
| 3 | `0x43bd40` | `0x43c2c0` |
| 4 | `0x43c230` | `0x43c1a0` |
| 5 | `0x43c460` | `0x43c500` |
| 6 | `0x43c310` | `0x43c3c0` |
| 7 | `0x43c310` | `0x43c410` |
| 8 | `0x43c710` | none |

Kind 3 initializes `+0x18` to `0xff`. The configuration initializes timing
state at `+0x30/+0x34/+0x38/+0x3c`, then copies the retained explicit values.
There is no defensive node-allocation handling: invalid kinds still
dereference `context+0x08` to write the cleanup callback, and reconfiguration
does not unregister prior nodes.
