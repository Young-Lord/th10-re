# Entity Field Setters And Render-Flag Getters (0x401ce0..0x40c960)

Module: `src/EntityFieldSetters.cpp`.

Setter family — writes a pair or scalar into an entity record and raises
the "geometry dirty" bits in the +0x35c flag word:

- `0x00401ce0` `SetEntitySizePairEaxStackAbi` — EAX = record, stack (a, b;
  ret 8): stores the pair at +0x4c/+0x50, raises dirty bit 0x8 once after
  both stores.
- `0x00401d60` `SetEntityPositionPairEaxStackAbi` — same pattern into
  +0x3c/+0x40, dirty bit 0x8.
- `0x00405170` `SetEntityField40EaxStackAbi` / `0x00405190`
  `SetEntityField3cEaxStackAbi` — scalar setters of the same two fields
  (ret 4), dirty bit 0x8.
- `0x004087a0` `SetEntityRotationEaxStackAbi` — writes the rotation at
  +0x2c and raises dirty bit 0x4 (not 0x8).
- `0x0040c960` `SetEntityScalePairEaxStackAbi` — writes +0x34/+0x38 with no
  dirty-flag side effects on this record type (evidenced difference).

Getter family — bits of the entity's +0x58 render-flags dword:

- `0x00408780` `GetEntityRenderFlag2EaxAbi` — bit 2 (0x4).
- `0x00408790` `GetEntityRenderFlag2Or0EaxAbi` — the native computes
  `((flags >> 2) | flags) & 1`; the OR with the unshifted value makes bit 0
  of +0x58 count as well (preserved oddity).
- `0x004087c0` `GetEntityRenderFlag1EaxAbi` — bit 1 (0x2).

All entries use the EAX = record usercall ABI with stack arguments; the
register ABI remains a thunk boundary.
