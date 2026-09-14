# Coverage Gap Status — COMPLETE (2026-09-06)

Source of truth: `config/function-status.csv` column 1.
Enumeration: 1351 game functions in 0x401000-0x452000 (`docs/coverage-gap.csv`,
136 rows >= 0x100 bytes were uncovered when this tracking started).

## Final state

- **592 addresses registered as implemented.**
- **All 136 originally-uncovered functions >= 0x100 bytes are now
  reconstructed** (remaining from the enumeration: 0).
- What remains below 0x100 bytes: ~631 tiny thunks (< 0x40) and ~235
  0x40-0xFF stubs — mostly register-ABI thunks and one-line wrappers that the
  semantic bodies supersede; they were deliberately out of scope per
  HANDOUT.md ("Preserve evidenced native oddities ... unusual register ABI
  only at thin thunk boundaries").

## Verification

- `g++ -m32 -std=c++98 -fsyntax-only -I src src/*.cpp` — clean.
- `scripts/compile-main-chain-cpp.sh` (per-file MSVC build) — exit 0.
- `git diff --check` — clean.

## Outstanding items for the exact-match pass

1. IDA MCP server was down for the final reconstruction passes; all modules
   from `src/SceneTriggerFeatures.cpp` onward were built from objdump
   disassembly of `resources/th10.exe`. Cross-check against IDA when the
   server recovers, and apply the Ghidra/IDA names from
   `config/function-status.csv`.
2. RESOLVED: the `0x4918b0` dual views were reconciled — objdump arbitration
   showed 0x4918b0 is exclusively the LCG-B state word (replay setup reads it
   into the per-stage record at +2; the restore path seeds it back from
   record+2 and clears the 0x4918b4 draw counter). Both replay modules now
   use `g_TimelinePrngStateB` (accessors `ReplayPrngSeedWord` /
   `ReplayPrngDrawCounter`).
3. RESOLVED: 0x40c480's child-chain write was arbitrated via objdump (native
   dereferences the {entity, next} node first); SetPlayerShotEntityHitState
   fixed to match.
4. Object-file-level matching (objdiff groups) against the reference build is
   the next phase; the semantic layer is complete.
