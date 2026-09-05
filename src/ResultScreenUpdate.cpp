// Result-screen state-machine controller (TH10 0x0042f540) and the stat
// field setter (TH10 0x00430250). Both are callers of the digit presenter
// 0x0042f8b0 (UpdateResultScreenStatDigitsEaxAbi). All helper callees are
// the semantic bodies exported from GameManagerState.cpp.
#include "ResultScreenUpdate.hpp"

#include "GameManagerState.hpp"
#include "ResultScreenDigits.hpp"

namespace th10 {

namespace {

// The fixed input-binding / result-carryover bank: u16 stat words at
// +0x00/+0x02/+0x04/+0x06 and +0x10 (TH10 DAT_00474e88/8c/98).
extern u8 g_MainChainInputBindings[18]; // TH10 DAT_00474e88

// Menu input gates shared with the other manager-state modules.
extern u8 g_MenuInputFlagsByte; // TH10 DAT_00474e34
extern u32 g_ManagerSubGateFlags; // TH10 DAT_00474e36

// The shared-status snapshot the page-6 commit writes (TH10 DAT_00491d4c,
// 17 bytes: four dwords plus one trailing u16).
extern u8 g_ResultStatusSnapshot[17]; // TH10 DAT_00491d4c

u32 LoadU32(const u8 *address)
{
    return static_cast<u32>(address[0]) | (static_cast<u32>(address[1]) << 8)
         | (static_cast<u32>(address[2]) << 16)
         | (static_cast<u32>(address[3]) << 24);
}

void StoreU32(u8 *address, u32 value)
{
    address[0] = static_cast<u8>(value);
    address[1] = static_cast<u8>(value >> 8);
    address[2] = static_cast<u8>(value >> 16);
    address[3] = static_cast<u8>(value >> 24);
}

u16 LoadU16(const u8 *address)
{
    return static_cast<u16>(static_cast<u16>(address[0])
                            | (static_cast<u16>(address[1]) << 8));
}

void StoreU16(u8 *address, u16 value)
{
    address[0] = static_cast<u8>(value);
    address[1] = static_cast<u8>(value >> 8);
}

// Copy the five u16 stat words from the global bank into the state record
// at +0x59cc..+0x59d4 (native word moves from DAT_00474e88/8c/98).
void RefreshStatFieldsFromGlobals(u8 *state)
{
    const u8 *const bank = g_MainChainInputBindings;
    StoreU16(state + 0x59ccU, LoadU16(bank + 0x00U));
    StoreU16(state + 0x59ceU, LoadU16(bank + 0x02U));
    StoreU16(state + 0x59d0U, LoadU16(bank + 0x04U));
    StoreU16(state + 0x59d2U, LoadU16(bank + 0x06U));
    StoreU16(state + 0x59d4U, LoadU16(bank + 0x10U));
}

// Page-6 commit: write the five u16 stat words back to the global bank,
// then snapshot the bank dwords (DAT_00474e88/8c/90/94) and the trailing
// word (DAT_00474e98) into DAT_00491d4c, exactly like the native
// dword/word moves at 0x42f75f..0x42f7d3.
void PublishGlobalsFromStatFields(u8 *state)
{
    u8 *const bank = g_MainChainInputBindings;
    StoreU16(bank + 0x00U, LoadU16(state + 0x59ccU));
    StoreU16(bank + 0x02U, LoadU16(state + 0x59ceU));
    StoreU16(bank + 0x04U, LoadU16(state + 0x59d0U));
    StoreU16(bank + 0x06U, LoadU16(state + 0x59d2U));
    StoreU16(bank + 0x10U, LoadU16(state + 0x59d4U));
    StoreU32(g_ResultStatusSnapshot + 0x00U, LoadU32(bank + 0x00U));
    StoreU32(g_ResultStatusSnapshot + 0x04U, LoadU32(bank + 0x04U));
    StoreU32(g_ResultStatusSnapshot + 0x08U, LoadU32(bank + 0x08U));
    StoreU32(g_ResultStatusSnapshot + 0x0cU, LoadU32(bank + 0x0cU));
    StoreU16(g_ResultStatusSnapshot + 0x10U, LoadU16(bank + 0x10U));
}

// Shared tail of the page-6 accept path (native 0x42f7d9..0x42f802):
// reserve boundary channel 0xb, queue the slot-2 stop word 6 and step to
// sub-state 4.
void RunResultScreenAcceptTail(void *game_manager)
{
    ReserveContextChannel(reinterpret_cast<void *>(0x00492590), 0xbU, 0U);
    SetManagerSlotEntityStopWord(game_manager, 2, 6);
    SetGameManagerSubState(game_manager, 4);
}

} // namespace

const u8 *PollJoystickButtonBytesEcxAbi(u32 device_index);

// TH10 0x0042f540. Native EBX = game manager, plain retn, always returns 1.
i32 UpdateResultScreenStateMachineEbxAbi(void *game_manager)
{
    u8 *const st = static_cast<u8 *>(game_manager);

    switch (LoadU32(st + 0x20U)) {
    case 0: {
        // Arm the cursor record: maximum at +0x2c is 7, and the value at
        // +0x24 is seeded from it with the native sign-split idiom (always
        // zero for the positive maximum, preserved verbatim).
        StoreU32(st + 0x2cU, 7U);
        const i32 maximum = static_cast<i32>(LoadU32(st + 0x2cU));
        if (maximum == 0) {
            StoreU32(st + 0x24U, 0U);
        } else if (maximum > 0) {
            StoreU32(st + 0x24U, 0U);
        } else {
            StoreU32(st + 0x24U, static_cast<u32>(maximum - 1));
        }
        SpawnManagerEntityFromScript(game_manager, 2);
        SetGameManagerSubState(game_manager, 1);
        RefreshStatFieldsFromGlobals(st);
        UpdateResultScreenStatDigitsEaxAbi(game_manager);
        // Native case 0 falls through into the case 1 body.
    }
    // fallthrough
    case 1: {
        if (static_cast<i32>(LoadU32(st + 0x2b4U)) <= 6)
            return 1;
        SetGameManagerSubState(game_manager, 2);
        SetEntityStopWordByIdAndRun(LoadU32(st + 0x2ccU), 3);
        SetManagerSlotEntityStopWord(
            game_manager, 2,
            static_cast<u16>(static_cast<u16>(LoadU16(st + 0x24U)) + 17U));
        return 1;
    }
    case 2: {
        u8 *const cursor = st + 0x24U;
        StoreU32(st + 0x28U, LoadU32(cursor));

        // Left/right cursor shift gated on the 0x10/0x20 masks of the
        // 0x474e36 flag dword low byte or the 0x474e34 gate byte.
        if ((g_ManagerSubGateFlags & 0x10U) != 0U
            || (g_MenuInputFlagsByte & 0x10U) != 0U)
            ShiftManagerSelector(cursor, -1);
        if ((g_ManagerSubGateFlags & 0x20U) != 0U
            || (g_MenuInputFlagsByte & 0x20U) != 0U)
            ShiftManagerSelector(cursor, 1);

        if (LoadU32(st + 0x28U) != LoadU32(cursor)) {
            ReserveContextChannel(reinterpret_cast<void *>(0x00492590),
                                  0xcU, 0U);
            SetEntityStopWordByIdAndRun(LoadU32(st + 0x2ccU), 3);
            SetManagerSlotEntityStopWord(
                game_manager, 2,
                static_cast<u16>(static_cast<u16>(LoadU16(cursor)) + 7U));
        }

        // Scan the 32-byte joystick button bank (indices 0..30 only, the
        // native loop caps at 31) for the first pressed button (byte with
        // the 0x80 bit, tested as a signed byte < 0). A press while the
        // cursor sits on a page 0..4 sets that stat field to the pressed
        // button index.
        const u8 *const buttons = PollJoystickButtonBytesEcxAbi(0U);
        for (u32 i = 0; i < 31U; ++i) {
            if (static_cast<signed char>(buttons[i]) < 0) {
                if (static_cast<i32>(LoadU32(cursor)) <= 4)
                    SetResultScreenStatFieldEaxEdxEcxAbi(
                        game_manager, static_cast<i32>(i),
                        static_cast<i32>(LoadU32(cursor)));
                break;
            }
        }

        // Page-6 accept: refresh the digits from the globals and advance
        // through the shared accept tail.
        if ((g_ManagerSubGateFlags & 0xaU) != 0U
            && static_cast<i32>(LoadU32(cursor)) == 6) {
            RefreshStatFieldsFromGlobals(st);
            UpdateResultScreenStatDigitsEaxAbi(game_manager);
            RunResultScreenAcceptTail(game_manager);
            return 1;
        }

        // Pages 5/6 commit paths gated on the 0x1001 flag mask.
        if ((g_ManagerSubGateFlags & 0x1001U) != 0U) {
            const i32 page = static_cast<i32>(LoadU32(cursor)) - 5;
            if (page == 0) {
                RefreshStatFieldsFromGlobals(st);
                UpdateResultScreenStatDigitsEaxAbi(game_manager);
                ReserveContextChannel(reinterpret_cast<void *>(0x00492590),
                                      0xaU, 0U);
                return 1;
            }
            if (page == 1) {
                PublishGlobalsFromStatFields(st);
                RunResultScreenAcceptTail(game_manager);
                return 1;
            }
        }
        return 1;
    }
    case 4: {
        if (static_cast<i32>(LoadU32(st + 0x2b4U)) >= 10) {
            SetGameManagerState(game_manager, 4);
            Call44BE70(st + 0x24U);
        }
        return 1;
    }
    default:
        return 1;
    }
}

// TH10 0x00430250. Native EAX = state, EDX = value, ECX = field index;
// returns the state pointer in EAX (unchanged).
void *SetResultScreenStatFieldEaxEdxEcxAbi(void *state, i32 value,
                                           i32 field_index)
{
    u8 *const st = static_cast<u8 *>(state);
    const u16 current = LoadU16(st + 0x59ccU + 2U * field_index);
    if (static_cast<i32>(static_cast<signed short>(current)) == value)
        return state;

    // Every other field that already shows `value` takes over the edited
    // field's previous raw value (native re-loads the edited field each
    // iteration; it is not modified inside the loop).
    for (i32 j = 0; j < 5; ++j) {
        if (j == field_index)
            continue;
        u8 *const other = st + 0x59ccU + 2U * j;
        if (static_cast<i32>(static_cast<signed short>(LoadU16(other)))
            == value)
            StoreU16(other, current);
    }
    StoreU16(st + 0x59ccU + 2U * field_index, static_cast<u16>(value));
    UpdateResultScreenStatDigitsEaxAbi(state);
    ReserveContextChannel(reinterpret_cast<void *>(0x00492590), 0xaU, 0U);
    return state;
}

} // namespace th10
