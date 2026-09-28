# Scene Trigger Popup (0x405750 / 0x405ac0 / 0x405be0 / 0x4073e0)

Module: `src/SceneTriggerPopup.cpp`.

- `0x00405750` `UpdateSceneTriggerPopupEdiAbi` — native EDI = popup object
  (0x48 bytes, DAT_004776ec). State machine:
  - state +0x28 == 0: done, return 1.
  - state != 1: fall through to the +0x14 timer tick and return 1.
  - state == 1: resolve the +0x2c handle (0x4491c0); when gone, clear
    +0x2c/+0x28 and return 1. Otherwise pick the position delta from the
    stage-state phase ([0x4776f4]+0x378c bit 0 and the +0x3788 phase value
    in [0x5d..0x60] or == 0x6d give 0.5 (0x470b0c); otherwise DAT_00474c68
    being zero gives 1.0 (0x470afc), else 1.3 (0x470d34)), subtract it from
    +0x34, publish the offset position (+0x30 + 224, +0x34 + 16, +0x38)
    through 0x4492f0, copy the resolved object's +0x40 into +0x3c, run the
    entrance submitter 0x405ac0, and finally tick the +0x14 timer
    (0x404ed0). Returns 1.
- `0x00405ac0` `SubmitSceneTriggerPopupEntranceEsiAbi` — native ESI = popup
  object. When the +0x28 state is live, runs the intro-activation scan
  (0x408100) against the effect manager root with the popup's +0x30
  position (require-unused selects 0 inside the story phase window
  [0x5d..0x60]/0x6d, else 1), then broadcasts the entrance tween (0x41c800)
  for the same position with the radius at +0x3c. Returns 0.
- `0x00405be0` `ResetSceneTriggerFrameTimersEaxAbi` — native EAX = trigger
  record. Zeroes the +0x446 state word, lazily initializes the two timer
  records at +0x3f8 and +0x40c (sentinel int 0xfff0bdc1 = -999999, zero
  counters, default rate pointer 0x476f78, flag bit 0), then hard-resets
  both timer ints to -1 and their counters to zero.
- `0x004073e0` `SceneTriggerAimAndSpreadInstructionEaxEcxEcxStackAbi` —
  native EAX = the trigger descriptor record, ECX = spawn float argument,
  stack = the manager work (ret 4). Computes the aim angle toward the
  screen target block (DAT_00477834 +0x3c0/+0x3c4 minus descriptor +0x4/+8;
  pi/2 (0x3fc90fdb) when the offset is exactly zero — the native guards
  both components against +0.0 — otherwise fpatan via the 0x408710
  boundary), then scans the row/column grid ([+0x1f6] rows x [+0x1f4]
  columns) spawning through 0x4067d0 until a spawn reports success. When
  the +0x1fc flags carry bit 0x200, enqueues the descriptor's +0x200 sound
  index with the +0x4 x position through the BGM runtime (0x43dd10).
  Returns 0.

Note: `src/SceneTriggerObject.cpp` carries a stale extern comment claiming
0x4073e0 for `SpawnSceneTriggerPacketEaxStackAbi`; the decompilation
settles 0x4073e0 as the aim-and-spread instruction above.
