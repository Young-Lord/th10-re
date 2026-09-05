# TH10 0x004269d0 — Player Death Processor

Module: `src/PlayerDeathProcessor.cpp` (`ProcessPlayerDeathStackAbi`).
Native entry receives the player as one stack argument, `ret 4`, no return
value. Called only from the dispatcher's mode-4 `t > 8` path.

## Behavior (program order)

1. Power penalty on the **dword** at `0x474c4c` (distinct from the word
   gauge `0x474c48`): remove one third of the amount above the 5000 floor
   (signed truncating division), clamp at 5000.
2. `0x474c70 -= 1`; when still >= 0, refresh the nine life-icon slots at
   `g_AsciiHudOwner + 0x4cdc` (0x3ac stride, bit 1 = shown) through the
   EAX-register helper `0x413790`.
3. `player+0x458 = 2` (death state; the dispatcher then runs the mode-2
   body in the same call).
4. Timer block `+0x474`: lazy first-use init (NaN sentinel `0xfff0bdc1`,
   rate pointer `&g_FrameTimeScale`, flag bit 0) then unconditional reset
   (`+0x478 = 0`, `+0x47c = 0`, `+0x474 = 0xffffffff`).
5. Timer block `+0x430c`: same lazy init, then `+0x4310 = 180`,
   `+0x4314 = 180.0f`, `+0x430c = 179`.
6. Rebinds the embedded 0x3ac-byte effect record at `player+0x14` through
   `0x43e710` (ECX = `player+0x10`, EAX = `player+0x14`, EBX = 0).
7. Option teardown: for each of the 4 records at `+0x32a0` (stride 0x98),
   zero `rec+0`, then hard-kill (`word ent+0x304 = 1`, propagated to the
   child list at `ent+0x14` when `ent+0x18 == 0`) both entity ids
   `rec+0x68` and `rec+0x6c` resolved over the two manager lists at
   `g_MainChainRenderOwner+0x72dad4/+0x72dadc`. No soft-flag use; no
   helper calls — destruction is deferred to the marker sweeper.
8. `player+0x3500 = 0`; "Caution!" text via `0x424650` (ESI = text manager
   `[0x477814]`) unless `g_GameModeObject+0x10 == 1`.
9. Boss-bullet flags: when `[(*g_SpellBulletBase)+0x3738] >= 60`, zero
   `+0x3790` and clear bit 1 of the eight flag dwords at offsets
   `0x378c/0xad4/0xe80/0x15d8/0x1984/0x1d30/0x20dc/0x2488`.
10. `0x474c98 -= 0x400`, clamped to [-0x400, 0x400].

## Explicitly untouched (verified)

Position and fixed-point fields, `player+0x45c`, the option tier latches
(`rec+0x8c` family), the sub-effect records, the history ring, bombs/score/
replay fields, and `g_GameStateManager`. The deathbomb path (mode 4
`t <= 8` with the bomb key) never reaches this function.

## Dispatcher corrections from this pass

The mode-2 body in `PlayerModeDispatcher.cpp` was updated after
disassembly verification: scaled-timer branch direction (fixed step inside
the 0.99..1.01 window, rate-scaled outside/NaN), sub-effect F2I
destination (`rec+0x48`, with `rec+0x44 = rec+0x48` copied before the
adjustment), the `rec+0x0 += rec+0x4` / `rec+0x8 += rec+0xc`
accumulation, the polar-path z zeroing, the death-burst angle call
(`ECX = player`, `EAX = &{0, y-224, 0}`), and the item magnet running
every gameplay frame rather than only on the 60-frame boundary.
