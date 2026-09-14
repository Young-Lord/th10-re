# Registration draw timing (0x004134b0) — real body

Reconstruction in `src/RegistrationDrawOwner.cpp`
(`UpdateRegistrationDrawTiming`, called by the 0x00413690/0x004135d0
scheduler callback). This replaces the previous semantic boundary that
relied on four dangling externs. Native input is the owner in ESI, plain
`ret`, EAX = 1 on every path; the FPU stack is used without x87 state
preservation.

## Control flow (verified against raw disassembly 0x4134b0..0x4135c7)

1. `current = GetMainChainFrameTime()` (0x00439540).
2. `fcom` owner baseline (+0x14) with the `test ah,0x5 / jp` idiom:
   the store happens only on the *ordered less* outcome, so an unordered
   comparison keeps the old baseline (the earlier evidence note claiming
   "or unordered" replaced the baseline misread the parity test).
3. `delta = current - baseline`; exit (EAX=1, baseline untouched) when
   `!(delta >= 0.5)` — the comparison at 0x470bc0 routes less *and*
   unordered to the exit, while an exact 0.5 continues.
4. Baseline advance: `baseline = baseline + delta` (only after a
   successful sample; sub-0.5s frames accumulate the delta).
5. `sampled_fps (+0x34) = unsigned_32(+0x20) / delta`. The native uses
   `fild` (signed) and adds 2^32 (0x470b30) when bit 31 is set, i.e. an
   unsigned interpretation of the frame accumulator. There is no
   zero-guard before the division.
6. If `!(fps > 0.0)` or unordered (fcomp 0.0f at 0x470bb8, `test
   ah,0x41 / jne`): clear `phase_count (+0x1c)` and skip the sampling
   actions. Otherwise increment the phase counter; on 2, read the tick
   again and store it to all four doubles 0x492540, 0x492538, 0x492528,
   0x492530 (in that native store order); on 4, first zero *both* dwords
   of the QueryPerformanceFrequency pair (0x492508/0x49250c) — which
   silently switches that one tick read onto the timeGetTime fallback —
   and then perform the same four stores.
7. If DAT_00477810 is non-null: `test byte [+0x58],0x14`; when neither
   bit is set, add 60.0 (0x470bb0) to `elapsed_window (+0x2c)` and add
   either 60.0 (fps > 57.0f at 0x470ba8, ordered) or the fps value
   (otherwise, including equal and unordered) to `displayed_fps (+0x24)`.
   Then clear bit 0x80 of the state flags word in both branches.
8. Clear `frame_accumulator (+0x20)`; return 1.

## Callback color correction (0x004135d0)

The raw listing at 0x4135f5 shows the same `test ah,0x5 / jp` idiom on
the fps-vs-30.0f comparison: the jump to the second comparison happens
only on the *unordered* outcome. Every ordered value — less, equal or
greater — selects 0xff5050ff directly, the 0xffa0a0ff band is
unreachable dead code, and a NaN fps falls through the second unordered
check (0x470c64) to 0xffffffff. The previous three-band C++ reading was
incorrect and is fixed in `RegistrationDrawCallback`.

The rest of the callback matches the existing description: state 14
(DAT_00491fb8) or a null DAT_004776e0 suppresses the FPS text, the
temporary color is stored to ascii manager +0x8974 and reset to -1 after
the 0x00401690 call, and `frame_accumulator += 1 + byte 0x491d66` runs
on every path.
