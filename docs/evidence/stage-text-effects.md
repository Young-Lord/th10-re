# Stage Text Effects — Floating Digit Counters (0x42b430 / 0x42b4d0 / 0x42b6f0 / 0x42b780 / 0x42b9c0)

Reconstructed in `src/StageTextEffects.cpp/.hpp`. This is the in-game
floating digit-text owner published at `DAT_00477840` (0xb884 bytes): a ring
of 0x40-byte text records at owner+0x3c4 behind the owner's 0x3ac-byte ASCII
animation VM record (owner+0x18), drawn through
`DrawAsciiAnimationVmUnscaledToOwner` (0x00443080). Callers of the spawner
0x0042b9c0 sit in the 0x41b4xx stage-script cluster (seven call sites);
lifecycle wrappers are 0x0042b660 (operator new 0xb884 + init + register,
with 0x0042b570 destroy / 0x0042b6b0 / 0x0042b6d0 delete wrappers).

## Owner layout

| Offset | Meaning |
| ------ | ------- |
| `+0x0010` | resource-state pointer (seeded from `AsciiManager +0x8994`); `[resource+0x118]` is the 0x44-byte glyph-entry table base |
| `+0x0014` | u32 ring write counter (0x2d0 live slots, wraps to 0) |
| `+0x0018` | VM record; `VM+0x340` (owner+0x358) is the current glyph y, `VM+0x344` (owner+0x35c) the glyph x, `VM+0x35c` (owner+0x374) flag |= 8, `VM+0x4c` glyph field, `VM+0x2fc/0x2ff/0x394` param / size byte / glyph pointer |
| `+0x03c4` | 0x40-byte records; record: `+0` text bytes, `+0xc/+0x10/+0x14` Float3 world position, `+0x18` param, `+0x1c` frame mirror, `+0x20` frame counter, `+0x24` float accumulator, `+0x28` rate-table pointer, `+0x2c` flags (bit0 = seeded), `+0x38` active byte, `+0x39` char count |

## 0x0042b430 — `InitializeTextEffectOwnerEdxAbi(owner)`

The native body clears bit 1 of eight VM fields (`owner+0x18+0x6c/0xb0/
0xfc/0x128/0x174/0x1b0/0x1fc/0x228/0x378`), resets the VM, stores 0xffff at
`VM+0x384`, clears the `+0x2c` flag word of all 0x2d3 records — and then
executes `rep stos` of 0x2e21 dwords (0xb884 bytes) from the owner base,
which erases every store above. Only the observable tail is implemented:
zero the owner, `[owner] |= 2`, publish `DAT_00477840`. Dead stores
preserved as a documented quirk.

## 0x0042b4d0 — `RegisterTextEffectOwnerSchedulerRecordsEaxAbi(owner)`

- `owner+0x10 = [DAT_004776e0 + 0x8994]` (resource pointer).
- Tick record: `Create(0x0042b9a0)` thunk (`mov eax,ecx; jmp 0x42b6f0`),
  flags bit 1 **cleared** (added disabled), arg = owner,
  `AddToCalculationChain(scheduler, rec, 0xf)`; stored at `owner+8`.
- Draw record: `Create(0x0042b9b0)` thunk (`mov edi,ecx; call 0x42b780`),
  flags bit 1 cleared, arg = owner, `AddToDrawChain(..., 0x23)`; stored at
  `owner+0xc`.
- `ResetAsciiAnimationVmRecord(owner+0x18)`, `VM+0x308 = resource`, then
  `InitializeAsciiAnimationVmEntry(vm, 0xc4, resource)`. Returns 0.

## 0x0042b6f0 — `TickTextEffectRecordsEaxAbi(owner)` (update body)

Scans **0x2d3** records (three past the 0x2d0-slot ring — quirk). For each
active (`+0x38 != 0`) record:

1. `y (+0x10) -= 1.0 * 0.5` (`flt_476f78 = 1.0` times `flt_470b0c = 0.5`).
2. `+0x1c = +0x20` (frame mirror).
3. `rate = *[[+0x28]]`. Two native fcomp selects: the accumulator path is
   taken when `rate == 0.99 || rate < 0.99 || rate < 1.01 || NaN`
   (unordered); only `rate > 0.99 && rate >= 1.01` (i.e. `rate >= 1.01`)
   uses the smooth path.
   - Smooth: `frames++` stored to `+0x20`, `+0x24 += 1.0` (`flt_470afc`).
   - Accumulator: `+0x24 += rate`, `+0x20 = ftol(+0x24)` (0x463b2c).
4. `+0x20 > 60` deactivates the record (`+0x38 = 0`).

Returns 1. Update callback = 0x0042b9a0 (EAX=owner thunk), priority 15.

## 0x0042b780 — `DrawTextEffectRecordsEdiAbi(owner)` (draw body)

Native `__usercall`, owner in EDI (draw thunk 0x0042b9b0), always falls
through to `ret` with EAX=1.

- **Fog teardown.** With `byte DAT_00491d78` bit 2 clear and
  `DAT_00492378` (the ASCII fog cache) nonzero: flush the render owner
  `DAT_00491c10` via 0x00442f50, clear the cache and issue device command
  `0x1c, param 0` through `D3D9Device` (DAT_00491c30) vtable slot 0x39.
- Scans **0x2d3** records. Active records only:
  - `y_step = (frames < 8) ? 8.0f / [rec+0x24] : 8.0f` (`flt_470bd0`).
  - `VM+0x340 = [rec+0xc] - count*y_step*0.5 + 224.0f` (`flt_470b4c`);
    `VM+0x344 = [rec+0x10] + 16.0f` (`flt_470b48`); `VM+0x2fc = [rec+0x18]`.
  - Glyph size from the squared camera distance (`DAT_00477834 +0x3c0/+0x3c4`
    minus the record position; `ftol` of dx²+dy²):
    `> 0x1000 → 208 (0xd0)`; `> 0x400 → 80 + (d-0x400)/24` (native magic
    0x2aaaaaab with `sar 9` after `shl 7`, i.e. `((d-0x400)<<7)*M >> 41` —
    continuous at both boundaries: 0x50 at 0x400, 0xd0 at 0x1000); else
    `80 (0x50)`.
  - Per character, walking the text **backwards** from `rec+count-1` (the
    spawner writes digits low-first), one glyph per iteration:
    glyph index = `ch + 0xc4` normally; `ch + 0xcf` when
    `frames >= 0x34 && ch != '\n' && frames < 0x38`; `ch + 0xd9` when
    `frames >= 0x38 && ch != '\n'` (aged/tinted rows). Glyph entry =
    `[resource+0x118] + index*0x44`. Stores `owner+0x3ac` (glyph pointer),
    `owner+0x317` (size byte), `VM+0x4c = [glyph+0x34]`, `VM+0x35c |= 8`,
    then `DrawAsciiAnimationVmUnscaledToOwner(vm, DAT_00491c10)` and
    `VM+0x340 += y_step` (vertical text run).

## 0x0042b9c0 — `AddTextEffectNumberEcxEaxEdiStackAbi(owner, value, param, position)`

Native: ECX = owner, EAX = value, EDI = Float3 position, stack = param
(`ret 4`).

1. Ring counter `owner+0x14`; wraps **to 0** (not modulo) when `>= 0x2d0`.
2. Record `= owner+0x3c4 + counter*0x40`; `+0x38 = 1`.
3. Text formatting at record+0: negative values store the single byte
   `'\n'`; zero stores an empty byte; otherwise decimal digits written
   **low digit first** (`idiv 10` loop), count at `+0x39`.
4. `+0x18 = param`.
5. First-use seeding (`+0x2c` bit 0 clear): sets `+0x2c |= 1`, `+0x20 = 0`,
   `+0x1c = 0xfff0bdc1` (dead — overwritten below), `+0x24 = 0`,
   `+0x28 = 0x476f78` (float 1.0 rate table).
6. Unconditionally: `+0x20 = 0`, `+0x24 = 0`, `+0x1c = -1`.
7. Copies the Float3 into `+0x0c/+0x10/+0x14`; `owner+0x14 = counter + 1`.

## Boundaries

- `0x00443080` `DrawAsciiAnimationVmUnscaledToOwner`,
  `0x00442f50` `FlushRenderOwnerPendingVerticesEsiAbi`,
  `0x00401de0` `ResetAsciiAnimationVmRecord`, `0x0043e5a0`
  `InitializeAsciiAnimationVmEntry`, `0x00449ed0/0x00449ae0/0x00449b70`
  scheduler create/add (CallbackSchedulerApi), `0x00463b2c` ftol —
  reconstructed elsewhere.
- The `DAT_00491c30` vtable+0xe4 device command and the D3D flush are
  modeled on the shared globals (`g_MainChainD3D9Device`,
  `g_MainChainRenderOwner`, `g_AsciiFogEnableCache`,
  `g_MainChainRuntimeOptions`).
