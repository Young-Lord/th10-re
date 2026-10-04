#pragma once

#include "Th10Platform.hpp"
#include "Th10Types.hpp"

namespace th10 {

// TH10 0x0041ba00. Native ECX = the bullet manager (DAT_00477818, also
// forwarded as the stack argument), one stack argument, result in EAX.
// Gate check on the game-mode record DAT_00477810: while it exists and its
// +0x58 flags say a termination/teardown phase is running
// (((flags | flags>>2) & 1) != 0 or flags & 0x400) the frame is skipped with
// result 1; otherwise the full bullet update 0x0041afd0 runs on the same
// manager and its result is returned.
i32 BulletCalcRecordCallbackEcxStackAbi(void *bullet_manager);

// TH10 0x0041afd0. Native one stack argument (the bullet manager whose
// 0x896 bullets of 0x3f0 bytes start at +0x14), __stdcall ret 4, returns 1.
// Per-bullet movement state machine (slot state_03dc, native +0x3dc), the
// 11-case item/bonus switch on the bullet kind (kind_03e0, native +0x3e0)
// for on-screen bullets, the off-screen state-4 retargeting, the
// animation-VM tick and the timer_03c8 distance bookkeeping (native
// +0x3c8/+0x3cc/+0x3d0); a deferred flag runs 0x0041ba50 over the first
// 150 bullets.
i32 UpdateBulletManagerStackAbi(void *bullet_manager);

// TH10 0x0041ba50. Native ESI = the bullet manager; plain ret. For the
// first 150 bullet slots (record = manager+0x14 + i*0x3f0): an active
// bullet (state_03dc != 0; the tested dwords are the slot state_03dc /
// kind_03e0 the frame update walks) whose kind is 1 or 4 is deactivated
// (state zeroed) and respawned as kind 9, and kinds 10/11 are respawned
// as kind 5; both share the tail: the death sound 0x0041bb00 is queued
// (angle -pi/2 0xbfc90fdb, speed 2.2 0x400ccccd, color -1) and a death
// entity is spawned through 0x00448db0. The killed slot's state is
// zeroed and +0x3ac (position_x_03ac) is NOT cleared.
void KillPendingBulletsEsiAbi(void *bullet_manager);

// TH10 0x0041beb0. Native thiscall ECX = float[2] vector, two stack floats.
// v[0] = cos(angle) * radius; v[1] = sin(angle) * radius; returns the vector.
void *SetPolarVectorThiscall(float *vector, float angle, float radius);

// TH10 0x00426660. Native EAX = float[2] position, ECX = the
// option-position manager (DAT_00477834; player position at +0x3c0/+0x3c4);
// result in st0. Returns atan2(py - y, px - x), with the native
// special case (dx == 30.0f && dy == 30.0f) -> 1.75f.
float AngleToPlayerPositionEaxEcxAbi(const float *position, void *manager);

// TH10 0x00405b60. Native EAX = the 0x474c40 frame-state block, ECX = signed
// delta; plain ret. Adds the delta to +0x58 (DAT_00474c98) clamped to
// [-1024, 1024].
void AddPowerValueEaxEcxAbi(void *frame_state, i32 delta);

// TH10 0x0041be80. Native thiscall ECX = the 0x474c40 frame-state block,
// one stack value; ret 4. Adds value/10 to +0x0c (DAT_00474c4c) capped at
// 99999; returns the stored value.
i32 AddPivValueEcxStackAbi(void *frame_state, i32 value);

// TH10 0x00418930. Native EAX = the 0x474c40 frame-state block, one stack
// i16 increment (ECX is an unmodeled register argument the body never uses
// meaningfully); ret 4. Adds the increment to the word at +8 (DAT_00474c48)
// unless it is already >= 100; crossing above 100 clamps to 100, releases
// the life-fragment entity handle held at
// [*(u32*)DAT_0047773c + 40472] and respawns it (kind 73), then returns
// whether the fragment/20 ladder crossed a multiple of 20.
i32 AddLifeFragmentEaxStackAbi(void *frame_state, i16 increment);

// TH10 0x0043dd10. Native EBX = effect kind index, ESI = the effect manager
// (0x00492590), one stack float (screen-space y offset); result in EAX =
// (int)(offset * -990.0). Enqueues the kind into the 12-slot x 128-entry
// pending-effect ring of the manager (deduplicating on kind).
i32 QueueBulletDeathEffectEbxEsiStackAbi(i32 kind, void *manager,
                                         float offset);

// TH10 0x00412ff0. Native EDI = the 0x474c40 frame-state block, one stack
// int (frame count). While the popup timer record at +0x14/+0x18 has not
// reached 130 the timer is shifted forward; on crossing 130 the record is
// re-seeded (first-use pattern) with 130 / 150.0f / -999999.
void AdvanceScorePopupTimerEdiStackAbi(void *frame_state, i32 frames);

// TH10 0x004054b0. Native EDX = digit quotient, ESI = the ASCII HUD owner
// (DAT_0047770c), one stack int (digit remainder argument); plain ret.
// Boundary: drives the item-get digit animation through three
// 0x0043e5a0 (InitializeAsciiAnimationVmEntry) invocations on the VM pair
// at hud+0x6a8c with the resource pointer [hud+0x9ec8]; glyph indices
// base+8, arg/10+8 and arg%10+8.
void RunItemGetDigitAnimEdxEsiStackAbi(i32 quotient, void *hud_owner,
                                       i32 digit);

// TH10 0x0042b9c0. Native EAX = signed value, EDI = unmodeled register
// argument (native EDI staleness preserved at the call sites), ESI = the
// point-of-collection digit state (DAT_00477840), one stack color; plain
// ret. Boundary: writes the decimal digits of the value into the 720-entry
// x 64-byte digit ring at esi+0x14 (kind byte 10 for negative values),
// stores the color at slot+24 and runs the float tail over
// flt_476f78-scaled values.
void SetPointItemDigitsEaxEdiEsiStackAbi(i32 value, void *edi_arg,
                                         void *piv_state, i32 color);

} // namespace th10
