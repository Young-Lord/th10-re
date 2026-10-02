#include "ResultScreenScript.hpp"

#include "AsciiAnimationVm.hpp"
#include "AsciiHudOwner.hpp"
#include "AsciiOverlayFactory.hpp"
#include "BgmRuntime.hpp"
#include "EntityHelpers.hpp"
#include "GameManagerState.hpp"
#include "GameStateManagerObject.hpp"
#include "MainChainContext.hpp"
#include "PlayerFrameworkHelpers.hpp"
#include "PlayerStageHelpers.hpp"
#include "PlayerTimerHelpers.hpp"
#include "TimelineAudioActions.hpp"
#include "TimelineRenderObjects.hpp"
#include "StageEffectHelpers.hpp"
#include "TimelineTextSubmission.hpp"

#include <string.h>

namespace th10 {
namespace {
u32 LoadU32From(const void *address)
{
    const u8 *const bytes = static_cast<const u8 *>(address);
    return static_cast<u32>(bytes[0]) | (static_cast<u32>(bytes[1]) << 8)
         | (static_cast<u32>(bytes[2]) << 16)
         | (static_cast<u32>(bytes[3]) << 24);
}

void StoreU32To(void *address, u32 value)
{
    u8 *const bytes = static_cast<u8 *>(address);
    bytes[0] = static_cast<u8>(value);
    bytes[1] = static_cast<u8>(value >> 8);
    bytes[2] = static_cast<u8>(value >> 16);
    bytes[3] = static_cast<u8>(value >> 24);
}
}


namespace {

extern void *g_MainChainRenderOwner;            // TH10 DAT_00491c10
extern u32 g_MainChainRuntimeOptions;           // TH10 DAT_00491d78
extern MainChainContext g_MainChainContext;     // TH10 DAT_00491c28
extern void *g_AsciiHudConditionalState;        // TH10 DAT_00477704
extern void *g_AsciiHudOwner;                   // TH10 DAT_0047770c
extern void *g_GameStateManager;                // TH10 DAT_00477830
extern u8 *g_OptionPositionBase;                // TH10 DAT_00477834
extern void *g_GameModeObject;                  // TH10 DAT_00477838
extern u8 g_SpellPracticeRecords;               // TH10 record base at 0x0047783c
extern void *g_TitleTransitionRecord;           // TH10 DAT_00477848
extern TransitionRootPartial g_TransitionRoot;  // TH10 DAT_00492590
extern u32 g_StageTextSprites[2];               // TH10 DAT_00474c68 / 00474c6c
extern u32 g_StageScoreSelector[2];             // TH10 DAT_00474c74 / 00474c78
extern i32 g_ActiveTextLayer;                   // TH10 DAT_00474c7c
extern i32 g_ScoreBonusBaseDword;               // TH10 DAT_00474c4c
extern i32 g_PlayerLivesRemaining;              // TH10 DAT_00474c70
extern u16 g_PlayerPowerGaugeWord;              // TH10 DAT_00474c48 (movsx word)
extern u32 g_TextStyleFlag;                     // TH10 DAT_00474c84
extern i32 g_TextStyleAlt;                      // TH10 DAT_00474c8c
extern u32 g_GlobalModeFlags;                   // TH10 DAT_00474ca0
extern u32 g_InputMaskWord;                     // TH10 DAT_00474e5c (dword view)
extern u32 g_ResultGateFlags;                   // TH10 DAT_00474e62
extern float g_MainChainStartupScale;           // TH10 DAT_00476f78

const i32 kStateWordTwo = 2;

// TH10 0x0040c480. Native EAX = &handle: set the resolved entity's u16 state
// word at +0x304 to 2 and, when the entity's dword at +0x18 is zero, the same
// word on every child along the inline node chain at +0x14 (node[0] = child,
// node[1] = next).
void SetEntityStateWordTwoEaxAbi(u32 *handle)
{
    u8 *entity = FindEntityEdxStackAbi(g_MainChainRenderOwner, *handle);
    if (entity == 0)
        return;

    *reinterpret_cast<u16 *>(entity + 0x304) = kStateWordTwo;
    if (*reinterpret_cast<u32 *>(entity + 0x18) != 0)
        return;

    u8 *node = *reinterpret_cast<u8 **>(entity + 0x14);
    while (node != 0) {
        u8 *child = *reinterpret_cast<u8 **>(node);
        *reinterpret_cast<u16 *>(child + 0x304) = kStateWordTwo;
        node = *reinterpret_cast<u8 **>(node + 4);
    }
}

inline u32 RecordPayload(const u8 *record)
{
    return *reinterpret_cast<const u32 *>(record + 4);
}

// Offset of a float3 slot inside state->positions: native computes
// ebp + (select + 8) * 12, i.e. select 0 -> slot 0 and select 1 -> slot 3
// (opcodes 7/8 only ever use these two selectors).
inline const float *PositionAt(ResultScreenScriptState *state, i32 select)
{
    return state->positions[select * 3];
}

i32 *SpawnResultScreenEffect(i32 script_id, u32 kind)
{
    return SpawnSetupEffectVmListABack(script_id, kind);
}

} // namespace

void SetEntityGlyphFromHandleEaxStackAbi(u32 *handle, u32 entry_index)
{
    u8 *entity = FindEntityEdxStackAbi(g_MainChainRenderOwner, *handle);
    if (entity == 0)
        return;
    InitializeAsciiAnimationVmEntry(
        entity, entry_index,
        *reinterpret_cast<void **>(entity + 0x308));
}

void SetEntityGlyphWithResourceEaxStackAbi(u32 *handle, u32 resource,
                                           u32 entry_index)
{
    u8 *entity = FindEntityEdxStackAbi(g_MainChainRenderOwner, *handle);
    if (entity == 0)
        return;
    InitializeAsciiAnimationVmEntry(entity, entry_index,
                                    reinterpret_cast<void *>(resource));
}

namespace {

// Per-character glyph entry offsets for opcode 13 (first/second pair; the
// resolve kinds are 0x18/0x1b except where noted).
struct GlyphPair {
    u32 kind_a;
    u32 entry_a;
    u32 kind_b;
    u32 entry_b;
};

void RunOpcodeThirteen(ResultScreenScriptState *state, const u8 *record)
{
    const u32 payload = RecordPayload(record);
    GlyphPair pair;
    pair.kind_a = 0x18;
    pair.kind_b = 0x1b;
    switch (g_ActiveTextLayer) {
    case 1:
        pair.entry_a = payload + 0x0f;
        pair.entry_b = payload + 0x17;
        break;
    case 2:
        pair.entry_a = payload + 0x15;
        pair.entry_b = payload + 0x1e;
        break;
    case 3:
        pair.entry_a = payload + 0x22;
        pair.entry_b = payload + 0x2b;
        break;
    case 4:
        pair.entry_a = payload + 0x25;
        pair.entry_b = payload + 0x2e;
        break;
    case 5:
        pair.entry_a = payload + 0x16;
        pair.entry_b = payload + 0x1f;
        break;
    case 6:
        pair.kind_a = 0x1c;
        pair.kind_b = 0x1f;
        pair.entry_a = payload + 0x30;
        pair.entry_b = payload + 0x39;
        break;
    case 7:
        if (state->selector >= 1) {
            pair.entry_a = payload + 0x2e;
            pair.entry_b = payload + 0x37;
        } else {
            pair.kind_a = 0x21;
            pair.kind_b = 0x24;
            pair.entry_a = payload + 0x44;
            pair.entry_b = payload + 0x4d;
        }
        break;
    default:
        return;
    }

    const u32 resource = *reinterpret_cast<u32 *>(
        reinterpret_cast<u8 *>(g_AsciiHudConditionalState) + 0x38);
    u32 out_a = 0;
    ResolveChildEntityByKind(&state->handle_b,
                             static_cast<i32>(pair.kind_a), &out_a);
    SetEntityGlyphWithResourceEaxStackAbi(&out_a, resource, pair.entry_a);
    u32 out_b = 0;
    ResolveChildEntityByKind(&state->handle_b,
                             static_cast<i32>(pair.kind_b), &out_b);
    SetEntityGlyphWithResourceEaxStackAbi(&out_b, resource, pair.entry_b);
}

// Opcode 20's spell-practice counter slot: one dword per
// (stage-text pair, stage) at records + 0x4d0.
u32 *SpellPracticeCounter()
{
    const u32 pair = 3 * g_StageTextSprites[0] + g_StageTextSprites[1];
    u8 *base = &g_SpellPracticeRecords;
    return reinterpret_cast<u32 *>(
        base + (pair * 0x10df + g_StageScoreSelector[0]) * 4 + 0x4d0);
}

void BumpSpellPracticeCounter()
{
    u32 *counter = SpellPracticeCounter();
    if (*counter < 99999)
        *counter += 1;
}

// Tail of opcode 20's text-layer-6 path: the 0x20 flag was already set at
// the branch entry; the post-overlay work clears +0x9ecc and latches 0x10.
void RecordSpellCaptureFlags()
{
    AsciiHudOwner &hud =
        *reinterpret_cast<AsciiHudOwner *>(g_AsciiHudOwner);
    hud.render_mode_counter = 0;
    hud.hud_mode_flags |= 0x10;
}

void SubmitBlankLine(u8 *entity, u32 owner)
{
    SubmitTimelineText(entity, g_MainChainRenderOwner, owner, " ");
}

void SubmitDecryptedLine(u8 *entity, u32 owner, const u8 *record)
{
    SubmitTimelineText(entity, g_MainChainRenderOwner, owner,
                       DecryptTimelineRecordText(record + 4));
}

} // namespace

i32 TH10_STDCALL RunResultScreenScriptStreamStackAbi(
    ResultScreenScriptState *state)
{
    if (state->repeat_counter > 0)
        state->repeat_counter -= 1;

    // Re-arm the frame block from the stream's u16 stamp while flagged and
    // the input mask word has bit 8 set. First run seeds the sentinel state.
    if ((state->flags & 1) != 0 && (g_InputMaskWord & 0x100) != 0) {
        const u32 frame = *reinterpret_cast<const u16 *>(state->stream);
        if ((state->timer_a[4] & 1) == 0) {
            state->timer_a[1] = 0;
            state->timer_a[0] = 0xfff0bdc1u; // -999999
            state->timer_a[2] = 0;
            state->timer_a[3] =
                reinterpret_cast<u32>(&g_MainChainStartupScale);
            state->timer_a[4] |= 1;
        }
        state->timer_a[1] = frame;
        state->timer_a[0] = frame - 1;
        *reinterpret_cast<float *>(&state->timer_a[2]) =
            static_cast<float>(frame);
    }

    if (static_cast<i32>(state->timer_a[1]) <
        static_cast<i32>(*reinterpret_cast<const u16 *>(state->stream))) {
        TickTimerForwardEsiAbi(state->timer_a);
        return 0;
    }

    for (;;) {
        const u8 *record = state->stream;
        const u32 opcode = record[2];
        switch (opcode) {
        case 0:
            // End of stream: clear the text style latches.
            if (g_TextStyleFlag != 0)
                g_TextStyleAlt = 0;
            g_TextStyleFlag = 0;
            return -1;

        case 1:
            if (g_StageTextSprites[0] == 0 || g_StageTextSprites[0] == 1) {
                const i32 script = *reinterpret_cast<const i32 *>(
                    g_OptionPositionBase + 0x10);
                state->handle_a = *SpawnResultScreenEffect(script, 29);
            }
            break;

        case 2:
            if (g_ActiveTextLayer == 6) {
                const i32 script = *reinterpret_cast<const i32 *>(
                    reinterpret_cast<u8 *>(g_AsciiHudConditionalState) + 0x38);
                state->handle_b = *SpawnResultScreenEffect(script, 32);
            } else if (g_ActiveTextLayer == 7 && state->selector < 1) {
                const i32 script = *reinterpret_cast<const i32 *>(
                    reinterpret_cast<u8 *>(g_AsciiHudConditionalState) + 0x38);
                state->handle_b = *SpawnResultScreenEffect(script, 37);
            } else if (g_StageTextSprites[0] == 0 ||
                       g_StageTextSprites[0] == 1) {
                const i32 script = *reinterpret_cast<const i32 *>(
                    g_OptionPositionBase + 0x10);
                state->handle_b = *SpawnResultScreenEffect(script, 30);
            }
            break;

        case 3: {
            // Deliberately raw: +0x9ea8 overlaps spell_bars[2].color.
            const i32 script = *reinterpret_cast<const i32 *>(
                reinterpret_cast<u8 *>(g_AsciiHudOwner) + 0x9ea8);
            state->handle_c = *SpawnResultScreenEffect(script, 90);
            break;
        }

        case 4:
            ExpireEntityHandleEaxAbi(&state->handle_a);
            state->handle_a = 0;
            break;

        case 5:
            ExpireEntityHandleEaxAbi(&state->handle_b);
            state->handle_b = 0;
            ExpireEntityHandleEaxAbi(&state->handle_f);
            break;

        case 6:
            ExpireEntityHandleEaxAbi(&state->handle_c);
            ExpireEntityHandleEaxAbi(&state->handle_d);
            ExpireEntityHandleEaxAbi(&state->handle_e);
            break;

        case 7:
            FireEntityHandleEaxAbi(&state->handle_b);
            SetEntityStateWordTwoEaxAbi(&state->handle_a);
            state->select_index = 0;
            SetEntityPositionDirectEsiAbi(g_MainChainRenderOwner,
                                          state->handle_d,
                                          state->positions[0]);
            SetEntityPositionDirectEsiAbi(
                g_MainChainRenderOwner, state->handle_e,
                PositionAt(state, 0));
            state->sub_counter = 0;
            break;

        case 8:
            FireEntityHandleEaxAbi(&state->handle_a);
            SetEntityStateWordTwoEaxAbi(&state->handle_b);
            state->select_index = 1;
            SetEntityPositionDirectEsiAbi(g_MainChainRenderOwner,
                                          state->handle_d,
                                          state->positions[3]);
            SetEntityPositionDirectEsiAbi(
                g_MainChainRenderOwner, state->handle_e,
                PositionAt(state, 1));
            state->sub_counter = 0;
            break;

        case 9:
            state->flags ^= ((state->flags ^ record[4]) & 1);
            break;

        case 10: {
            // Text/BGM wait: arm timer B once its count has run out, then
            // shift it back by one second every frame.
            if (static_cast<i32>(state->timer_b[1]) <= 0)
                TickPlayerTimerEaxStackAbi(
                    state->timer_b, static_cast<i32>(RecordPayload(record)));
            ShiftTimerByEsiStackAbi(state->timer_b, -1.0f);
            if ((g_ResultGateFlags & 0x1001) != 0 ||
                static_cast<i32>(state->timer_b[1]) <= 0) {
                EnqueueBgmSoundValue(&g_TransitionRoot, 0, 0);
                TickPlayerTimerEaxStackAbi(state->timer_b, 0);
                state->sub_counter = 0;
            } else {
                if ((state->flags & 1) == 0 || (g_InputMaskWord & 0x100) == 0)
                    return 0; // early exit without the timer A tick
                TickPlayerTimerEaxStackAbi(state->timer_b, 0);
                state->sub_counter = 0;
            }
            break;
        }

        case 11:
            state->repeat_counter = 1;
            break;

        case 12: {
            const u32 payload = RecordPayload(record);
            u32 out = 0;
            if (g_StageTextSprites[0] == 0) {
                ResolveChildEntityByKind(&state->handle_a, 0x17, &out);
                SetEntityGlyphFromHandleEaxStackAbi(&out, payload + 0x34);
                ResolveChildEntityByKind(&state->handle_a, 0x1a, &out);
                SetEntityGlyphFromHandleEaxStackAbi(&out, payload + 0x3c);
            } else if (g_StageTextSprites[0] == 1) {
                ResolveChildEntityByKind(&state->handle_a, 0x17, &out);
                SetEntityGlyphFromHandleEaxStackAbi(&out, payload + 0x2d);
                ResolveChildEntityByKind(&state->handle_a, 0x1a, &out);
                SetEntityGlyphFromHandleEaxStackAbi(&out, payload + 0x35);
            }
            break;
        }

        case 13:
            RunOpcodeThirteen(state, record);
            break;

        case 14:
        case 15: {
            u32 &handle = (opcode == 14) ? state->handle_d : state->handle_e;
            u8 *entity = FindEntityEdxStackAbi(g_MainChainRenderOwner, handle);
            if (entity == 0)
                handle = 0;
            SubmitDecryptedLine(entity,
                                state->text_owners[state->select_index],
                                record);
            SetEntityStateWordTwoEaxAbi(&handle);
            break;
        }

        case 16: {
            if (state->sub_counter != 0) {
                u8 *entity = FindEntityEdxStackAbi(g_MainChainRenderOwner,
                                                   state->handle_e);
                if (entity == 0)
                    state->handle_e = 0;
                SubmitDecryptedLine(entity,
                                    state->text_owners[state->select_index],
                                    record);
                SetEntityStateWordTwoEaxAbi(&state->handle_e);
                state->sub_counter = 0;
            } else {
                const u32 owner = state->text_owners[state->select_index];
                u8 *entity_a = FindEntityEdxStackAbi(g_MainChainRenderOwner,
                                                     state->handle_d);
                if (entity_a == 0)
                    state->handle_d = 0;
                SubmitBlankLine(entity_a, owner);
                u8 *entity_b = FindEntityEdxStackAbi(g_MainChainRenderOwner,
                                                     state->handle_e);
                if (entity_b == 0)
                    state->handle_e = 0;
                SubmitBlankLine(entity_b, owner);
                entity_a = FindEntityEdxStackAbi(g_MainChainRenderOwner,
                                                 state->handle_d);
                if (entity_a == 0)
                    state->handle_d = 0;
                SubmitDecryptedLine(entity_a, owner, record);
                SetEntityStateWordTwoEaxAbi(&state->handle_d);
                FireEntityHandleEaxAbi(&state->handle_e);
                state->sub_counter += 1;
            }
            break;
        }

        case 17:
            FireEntityHandleEaxAbi(&state->handle_d);
            FireEntityHandleEaxAbi(&state->handle_e);
            break;

        case 18: {
            SelectTimelineAudioMode(
                1, *reinterpret_cast<const i32 *>(
                       reinterpret_cast<u8 *>(g_TitleTransitionRecord) + 0x28));
            AsciiHudOwner &hud =
                *reinterpret_cast<AsciiHudOwner *>(g_AsciiHudOwner);
            const i32 script = static_cast<i32>(
                reinterpret_cast<u32>(hud.stage_script_work));
            SpawnResultScreenEffect(script, 2);
            break;
        }

        case 19: {
            const i32 script = *reinterpret_cast<const i32 *>(
                reinterpret_cast<u8 *>(g_AsciiHudConditionalState) + 0x38);
            switch (g_ActiveTextLayer) {
            case 1:
                state->handle_f = *SpawnResultScreenEffect(script, 13);
                break;
            case 2:
                state->handle_f = *SpawnResultScreenEffect(script, 16);
                break;
            case 3:
                state->handle_f = *SpawnResultScreenEffect(script, 20);
                break;
            case 4:
                state->handle_f = *SpawnResultScreenEffect(script, 21);
                break;
            case 5:
                state->handle_f = *SpawnResultScreenEffect(script, 14);
                break;
            case 6:
                state->handle_f = *SpawnResultScreenEffect(script, 35);
                break;
            case 7:
                state->handle_f = *SpawnResultScreenEffect(script, 29);
                break;
            default:
                break;
            }
            break;
        }

        case 20: {
            const i32 stage = static_cast<i32>(g_StageScoreSelector[0]);
            if (stage != 4) {
                // Two "cleared" flag bytes per (character, stage) slot.
                const u32 pair =
                    3 * g_StageTextSprites[0] + g_StageTextSprites[1];
                const u32 slot = g_ActiveTextLayer + 6 * stage;
                u8 *base = &g_SpellPracticeRecords;
                base[pair * 0x437c + slot * 8 + 0x4e1] = 1;
                base[pair * 0x437c + slot * 8 + 0x4e0] = 1;
            }

            if ((g_GlobalModeFlags & 0x10) != 0) {
                RecordSpellPracticeCaptureEdiAbi(g_GameStateManager);
                break;
            }

            if (g_ActiveTextLayer == 6) {
                (*reinterpret_cast<AsciiHudOwner *>(g_AsciiHudOwner))
                    .hud_mode_flags |= 0x20;
                AddScoreBlockValueEcxStackAbi(
                    reinterpret_cast<void *>(0x474c40u),
                    1000 * g_ScoreBonusBaseDword);
                switch (g_StageScoreSelector[0]) {
                case 0:
                    AddScoreBlockValueEcxStackAbi(
                        reinterpret_cast<void *>(0x474c40u),
                        20000000 * g_PlayerLivesRemaining);
                    AddScoreBlockValueEcxStackAbi(
                        reinterpret_cast<void *>(0x474c40u),
                        100000 * static_cast<i32>(static_cast<short>(g_PlayerPowerGaugeWord)));
                    break;
                case 1:
                    AddScoreBlockValueEcxStackAbi(
                        reinterpret_cast<void *>(0x474c40u),
                        25000000 * g_PlayerLivesRemaining);
                    AddScoreBlockValueEcxStackAbi(
                        reinterpret_cast<void *>(0x474c40u),
                        100000 * static_cast<i32>(static_cast<short>(g_PlayerPowerGaugeWord)));
                    break;
                case 2:
                    AddScoreBlockValueEcxStackAbi(
                        reinterpret_cast<void *>(0x474c40u),
                        35000000 * g_PlayerLivesRemaining);
                    AddScoreBlockValueEcxStackAbi(
                        reinterpret_cast<void *>(0x474c40u),
                        200000 * static_cast<i32>(static_cast<short>(g_PlayerPowerGaugeWord)));
                    break;
                case 3:
                    AddScoreBlockValueEcxStackAbi(
                        reinterpret_cast<void *>(0x474c40u),
                        40000000 * g_PlayerLivesRemaining);
                    AddScoreBlockValueEcxStackAbi(
                        reinterpret_cast<void *>(0x474c40u),
                        300000 * static_cast<i32>(static_cast<short>(g_PlayerPowerGaugeWord)));
                    break;
                case 4:
                    AddScoreBlockValueEcxStackAbi(
                        reinterpret_cast<void *>(0x474c40u),
                        40000000 * g_PlayerLivesRemaining);
                    AddScoreBlockValueEcxStackAbi(
                        reinterpret_cast<void *>(0x474c40u),
                        400000 * static_cast<i32>(static_cast<short>(g_PlayerPowerGaugeWord)));
                    break;
                default:
                    break;
                }
                if (*reinterpret_cast<const u32 *>(
                        reinterpret_cast<u8 *>(g_GameModeObject) + 0x10) == 1) {
                    RequestGameStateTransitionEaxStackAbi(&g_MainChainContext,
                                                          4);
                    break;
                }
                CreateAsciiOverlayContext(5, 0x78, 0, 0, 0, 0x31);
                RecordSpellCaptureFlags();
                BumpSpellPracticeCounter();
                break;
            }

            if (g_ActiveTextLayer == 7) {
                (*reinterpret_cast<AsciiHudOwner *>(g_AsciiHudOwner))
                    .hud_mode_flags |= 0x20;
                AddScoreBlockValueEcxStackAbi(
                    reinterpret_cast<void *>(0x474c40u),
                    1000 * g_ScoreBonusBaseDword);
                AddScoreBlockValueEcxStackAbi(
                    reinterpret_cast<void *>(0x474c40u),
                    40000000 * g_PlayerLivesRemaining);
                AddScoreBlockValueEcxStackAbi(
                    reinterpret_cast<void *>(0x474c40u),
                    400000 * static_cast<i32>(static_cast<short>(g_PlayerPowerGaugeWord)));
                if (*reinterpret_cast<const u32 *>(
                        reinterpret_cast<u8 *>(g_GameModeObject) + 0x10) == 1) {
                    RequestGameStateTransitionEaxStackAbi(&g_MainChainContext,
                                                          4);
                    break;
                }
                RecordSpellPracticeCaptureEdiAbi(g_GameStateManager);
                BumpSpellPracticeCounter();
                break;
            }

            // Any other character: recycle the result list handle, then
            // request the spell-card-list transition state (11).
            {
                AsciiHudOwner &hud =
                    *reinterpret_cast<AsciiHudOwner *>(g_AsciiHudOwner);
                u32 *handle_slot = &hud.first_banner_handle;
                ReleaseEntityById(g_MainChainRenderOwner, *handle_slot);
                *handle_slot = 0;
                // Deliberately raw: +0x9ea8 overlaps spell_bars[2].color.
                const i32 script = *reinterpret_cast<const i32 *>(
                    reinterpret_cast<u8 *>(g_AsciiHudOwner) + 0x9ea8);
                *handle_slot = *SpawnResultScreenEffect(script, 0x4c);
                RequestGameStateTransitionEaxStackAbi(&g_MainChainContext, 11);
                UpdateScoreBlockEaxAbi(reinterpret_cast<void *>(0x474c40u));
            }
            break;
        }

        case 21:
            SetTimelineAudioValue(g_ActiveTextLayer == 6 ? 8.0f : 2.0f);
            break;

        case 22:
            SetEntityStateWordEaxEsiAbi(&state->handle_a, 7);
            break;

        case 23:
            SetEntityStateWordEaxEsiAbi(&state->handle_b, 7);
            break;

        default:
            break;
        }

        // Advance to the next record: 4 bytes of header plus the declared
        // payload length, then stop once the timer falls behind the next
        // record's frame stamp.
        state->stream += record[3] + 4;
        if (static_cast<i32>(state->timer_a[1]) <
            static_cast<i32>(
                *reinterpret_cast<const u16 *>(state->stream))) {
            TickTimerForwardEsiAbi(state->timer_a);
            return 0;
        }
    }
}


// TH10 0x00409d90. Native __thiscall ECX = score block, stack = value,
// ret 4: adds value/10 (signed, truncating) to the +4 total and clamps
// at 999999999 (the >= 1000000000 comparison runs after the store).
i32 AddScoreBlockValueEcxStackAbi(void *score_block, i32 value)
{
    u8 *const block = static_cast<u8 *>(score_block);
    const u32 current = static_cast<u32>(block[4])
        | (static_cast<u32>(block[5]) << 8)
        | (static_cast<u32>(block[6]) << 16)
        | (static_cast<u32>(block[7]) << 24);
    const i32 total = static_cast<i32>(current) + value / 10;
    const u32 stored =
        (total >= 1000000000) ? 999999999U : static_cast<u32>(total);
    block[4] = static_cast<u8>(stored);
    block[5] = static_cast<u8>(stored >> 8);
    block[6] = static_cast<u8>(stored >> 16);
    block[7] = static_cast<u8>(stored >> 24);
    return (total >= 1000000000)
               ? static_cast<i32>(999999999U)
               : static_cast<i32>(stored);
}

// TH10 0x004175e0. Native EAX = score block: advances the +0x3c slot
// index while it is below 7 and publishes the 48-byte slot pointer
// (0x00474788 + 48 * index) to DAT_00477848.
void *UpdateScoreBlockEaxAbi(void *score_block)
{
    u8 *const block = static_cast<u8 *>(score_block);
    const u32 index = static_cast<u32>(block[0x3cU])
        | (static_cast<u32>(block[0x3dU]) << 8)
        | (static_cast<u32>(block[0x3eU]) << 16)
        | (static_cast<u32>(block[0x3fU]) << 24);
    u32 next = index;
    if (index < 7U) {
        next = index + 1U;
        block[0x3cU] = static_cast<u8>(next);
        block[0x3dU] = 0U;
        block[0x3eU] = 0U;
        block[0x3fU] = 0U;
    }
    const u32 slot = 0x474788U + 48U * next;
    StoreU32To(reinterpret_cast<void *>(0x477848U), slot);
    return reinterpret_cast<void *>(slot);
}


// TH10 0x00423370. Native usercall: EDI = the DAT_00477830 spell-practice
// state object; the ECX input is never read (caller garbage at the known
// call sites). When title+0x5c == 1 it publishes the pending shared
// status (2 when 0x491ff4 bit 0x1000 is set, else 4) and returns it.
// Otherwise it enters the spell-practice scene: sub-state 6, the
// +0x10..+0x20 score-anim block (seeded on first use, accumulator forced
// to -1), title+0x58 |= 0x10, two pool VMs (render mode 15, script binds
// 0 and 129, both linked to list B with their ids published to +0x1d8
// and consumed by the overlay creation), the game-over overlay creation
// 0x424480 (32, 16, 384, 448), the HUD owner's +0x9ec8 resource copied
// to +0x2c4, "bgm/th10_17.wav" queued plus the 0x10-gated and plain BGM
// commands, the clear-flag byte at 0x47783c+0x1d8a3, +0x1e4 = 1, and the
// 0x476f78 time-scale float swapped to 1.0 after its old value is copied
// to +0x2c0. Returns 1.
i32 RecordSpellPracticeCaptureEdiAbi(void *spell_state)
{
    const u32 title_state =
        LoadU32From(reinterpret_cast<const void *>(0x477810U));
    if (LoadU32From(reinterpret_cast<const void *>(title_state + 0x5cU))
        == 1U) {
        const u32 pending =
            (LoadU32From(reinterpret_cast<const void *>(0x491ff4U)) & 0x1000U)
                    != 0U
                ? 2U
                : 4U;
        StoreU32To(reinterpret_cast<void *>(0x491fb8U), pending);
        return static_cast<i32>(pending);
    }

    // Typed view over the DAT_00477830 game-state manager record.
    GameStateManager &mgr = *reinterpret_cast<GameStateManager *>(spell_state);
    mgr.mode_0004 = 6;
    const u32 flag = mgr.frame_timer.flags;
    if ((flag & 1U) == 0U) {
        mgr.frame_timer.count = 0;
        mgr.frame_timer.prev = static_cast<i32>(0xfff0bdc1U); // -999999
        mgr.frame_timer.accum = 0;
        mgr.frame_timer.rate = &g_MainChainStartupScale;
        mgr.frame_timer.flags = flag | 1U;
    }
    mgr.frame_timer.count = 0;
    mgr.frame_timer.accum = 0;
    mgr.frame_timer.prev = -1;

    StoreU32To(reinterpret_cast<void *>(title_state + 0x58U),
               LoadU32From(reinterpret_cast<const void *>(title_state + 0x58U))
                   | 0x10U);

    void *const manager = g_MainChainRenderOwner;

    // First overlay VM: script 0, linked to list B, id published to
    // +0x1d8 and used as the overlay entity.
    u32 vm = reinterpret_cast<u32>(AllocatePoolVmEsiAbi(manager));
    StoreU32To(reinterpret_cast<void *>(vm + 0x35cU),
               LoadU32From(reinterpret_cast<const void *>(vm + 0x35cU))
                   | 0x40000000U);
    StoreU32To(reinterpret_cast<void *>(vm + 0x20U), 15U);
    AssignPoolVmScriptEcxEaxAbi(reinterpret_cast<void *>(vm), 0);
    u32 overlay_id = 0U;
    AttachEffectVmToListB(&overlay_id, reinterpret_cast<void *>(vm), manager);
    mgr.handle_b_01d8 = overlay_id;
    (void)CreateGameOverOverlay(g_MainChainRenderOwner,
                                static_cast<i32>(overlay_id), 32, 16, 384,
                                448);

    // Second overlay VM: script 129, linked to list B.
    vm = reinterpret_cast<u32>(AllocatePoolVmEsiAbi(manager));
    StoreU32To(reinterpret_cast<void *>(vm + 0x35cU),
               LoadU32From(reinterpret_cast<const void *>(vm + 0x35cU))
                   | 0x40000000U);
    StoreU32To(reinterpret_cast<void *>(vm + 0x20U), 15U);
    AssignPoolVmScriptEcxEaxAbi(reinterpret_cast<void *>(vm), 129);
    {
        u32 out_id = 0U;
        AttachEffectVmToListB(&out_id, reinterpret_cast<void *>(vm), manager);
        // Native stores the spawned entity id to manager+0x1d4
        // (handle_a_01d4) at 0x0042349f.
        mgr.handle_a_01d4 = out_id;
    }
    // The HUD owner's +0x9ec8 glyph resource (read through the absolute
    // global DAT_0047770c) is copied to +0x2c4.
    AsciiHudOwner &hud_owner = *reinterpret_cast<AsciiHudOwner *>(
        LoadU32From(reinterpret_cast<const void *>(0x47770cU)));
    mgr.front_anm_work_02c4 = hud_owner.front_anm_work;

    StartBgmTrack("bgm/th10_17.wav", 0);
    if ((g_MainChainRuntimeOptions & 0x10U) != 0U) {
        QueueBgmCommand(&g_TransitionRoot, "dummy", 4, 0);
    }
    QueueBgmCommand(&g_TransitionRoot, "dummy", 2, 0);

    u8 *const records = &g_SpellPracticeRecords;
    records[0x1d8a3U] = 1U;
    mgr.spell_practice_flag_01e4 = 1;

    const u32 old_scale =
        LoadU32From(reinterpret_cast<const void *>(0x476f78U));
    *reinterpret_cast<u32 *>(&mgr.saved_time_scale_02c0) = old_scale;
    StoreU32To(reinterpret_cast<void *>(0x476f78U), 0x3f800000U); // 1.0f
    return 1;
}

namespace {

// TH10 0x0040e6a0. Native stack arg = the 0x00477704 conditional state;
// walks the +0x58 record chain, gating on each record's +0x2480 flags and
// +0x2448 counter, and re-arms the sprite batch at record+0x1068 through
// 0x448db0. Still a boundary.
void AdvanceAsciiHudConditionalChainEaxStackAbi(void *state);

// The 0x1c-stride bullet-list record chain head lives at root+0x18; each
// node {vptr@0, next@8, flag@0xc} receives vtable slot +0x14 (node, 0)
// unless its flag is 1.
typedef void (*BulletListNodeCallback)(void *, i32);

// TH10 0x00415b00 spawn step: allocate a 0x3ac pool VM on the render owner,
// stamp render mode 15 (+0x20) and the 0x40000000 flag (+0x35c), bind the
// script, and link it into the active list. The native also pushes the ANM
// manager-work (DAT_004776e0+0x899c) as the 0x449950 stack argument, which
// that helper never reads.
u32 SpawnResultScreenTextVm(i32 script_id)
{
    void *const vm = AllocatePoolVmEsiAbi(g_MainChainRenderOwner);
    StoreU32To(static_cast<u8 *>(vm) + 0x20U, 15U);
    StoreU32To(static_cast<u8 *>(vm) + 0x35cU,
               LoadU32From(static_cast<const u8 *>(vm) + 0x35cU) |
                   0x40000000U);
    AssignPoolVmScriptEcxEaxAbi(vm, script_id);
    u32 id = 0;
    LinkEntityAndAssignIdEaxEsiAbi(&id, vm);
    return id;
}

// One stopped interpolator timer {prev@0, cur@4, accum@8, rate ptr@0xc,
// flags@0x10}: the guarded lazy-init writes are then erased by the
// unconditional stopped reset (prev = -1, cur = 0, accum = 0).
void StopResultScreenStateTimer(u8 *timer)
{
    if ((LoadU32From(timer + 0x10U) & 1U) == 0U) {
        StoreU32To(timer + 0x4U, 0U);
        StoreU32To(timer + 0x0U, 0xfff0bdc1U);
        StoreU32To(timer + 0x8U, 0U);
        StoreU32To(timer + 0xcU, 0x476f78U);
        StoreU32To(timer + 0x10U, LoadU32From(timer + 0x10U) | 1U);
    }
    StoreU32To(timer + 0x4U, 0U);
    StoreU32To(timer + 0x8U, 0U);
    StoreU32To(timer + 0x0U, static_cast<u32>(-1));
}

} // namespace

// TH10 0x00415b00. Native stack args = {state, stream}, ret 8; returns the
// state pointer. See the header for the full contract.
ResultScreenScriptState *InitializeResultScreenScriptState(
    ResultScreenScriptState *state, u8 *stream)
{
    u8 *const bytes = reinterpret_cast<u8 *>(state);
    memset(bytes, 0, 0x90);

    StopResultScreenStateTimer(bytes + 0x4U);
    StopResultScreenStateTimer(bytes + 0x18U);
    StopResultScreenStateTimer(bytes + 0x2cU);

    state->reserved_58 = 0;
    state->stream = stream;

    state->handle_d = SpawnResultScreenTextVm(0);
    state->handle_e = SpawnResultScreenTextVm(1);

    // Stamp byte 0x10 on both text VMs' +0x3a0/+0x3a1 fields. Each id is
    // resolved twice, and the native performs the byte store even when the
    // resolve returned null (after clearing the slot), so the store is
    // deliberately unconditional here as well.
    u32 *const handle_slots[2] = {&state->handle_d, &state->handle_e};
    for (u32 slot = 0; slot != 2U; ++slot) {
        for (u32 which = 0; which != 2U; ++which) {
            u8 *entity = FindEntityEdxStackAbi(g_MainChainRenderOwner,
                                               *handle_slots[slot]);
            if (entity == 0)
                *handle_slots[slot] = 0;
            entity[0x3a0 + which] = 0x10;
        }
    }

    // Position slots 0 and 3 (the select-0 / select-1 pair the executor
    // reads) and the two per-select text colors.
    state->positions[0][0] = 8.0f;
    state->positions[0][1] = 0.0f;
    state->positions[0][2] = 0.0f;
    state->positions[3][0] = 24.0f;
    state->positions[3][1] = 0.0f;
    state->positions[3][2] = 0.0f;
    state->text_owners[0] = 0xf8f08fU;
    state->text_owners[1] = 0x8088ffU;

    // Re-activate every on-stage enemy whose +0x446 state word is neither
    // 0 nor 3: 2000 records of stride 0x7f0 at effect_root+0x60.
    u8 *enemy = static_cast<u8 *>(
                    *reinterpret_cast<void *const *>(0x4776f0U)) + 0x60U;
    for (u32 i = 0; i != 2000U; ++i) {
        const u16 state_word =
            static_cast<u16>(enemy[0x446U]) |
            static_cast<u16>(static_cast<u32>(enemy[0x447U]) << 8);
        if (state_word != 0U && state_word != 3U)
            (void)ActivateStageEnemyEsiAbi(enemy);
        enemy += 0x7f0U;
    }

    // Run the bullet-list root's record chain (vtable slot +0x14, arg 0,
    // skipped while the node flag at +0xc is 1).
    u8 *node = *reinterpret_cast<u8 **>(
        *reinterpret_cast<u8 *const *>(0x47781cU) + 0x18U);
    while (node != 0) {
        u8 *const next = *reinterpret_cast<u8 **>(node + 8U);
        if (*reinterpret_cast<const i32 *>(node + 0xcU) != 1) {
            void **const vtable = *reinterpret_cast<void ***>(node);
            reinterpret_cast<BulletListNodeCallback>(vtable[5])(node, 0);
        }
        node = next;
    }

    AdvanceAsciiHudConditionalChainEaxStackAbi(g_AsciiHudConditionalState);
    return state;
}

} // namespace th10
