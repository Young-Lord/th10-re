#include "BgmUpdateLoop.hpp"

#include "AsciiAnimationVm.hpp"
#include "BgmRuntime.hpp"
#include "EntityHelpers.hpp"
#include "MainChainRuntime.hpp"

namespace th10 {

namespace {

extern TransitionRootPartial g_TransitionRoot; // TH10 DAT_00492590
extern void *g_MainChainRenderOwner;           // TH10 DAT_00491c10

extern signed char g_BgmVolumeInputConfig;   // TH10 DAT_00491d68
extern signed char g_BgmVolumeEnabledConfig; // TH10 DAT_00491d69
extern i32 g_TransitionVolumeScale;          // TH10 DAT_00497854
extern i32 g_BgmSoundSequenceVolumeScale;    // TH10 DAT_00497858

// White and emphasized color digit glyph base codes on the options screen.
const i32 kDigitBasePlain = 0x33;
const i32 kDigitBaseEmphasis = 0x3d;

// The options screen text container handle cached at options_state + 0x2c8.
const u32 kOptionsTextHandleSlot = 0x2c8 / 4;

// Digit glyph slot ids inside the options screen text container. Ids 37-39 /
// 41-43 show the BGM volume in the plain / emphasized style, ids 45-47 /
// 49-51 the SE volume. Each container child stores its id at +0x38a.
const u16 kBgmVolumeDigitsPlain[3] = {37, 38, 39};
const u16 kBgmVolumeDigitsEmphasis[3] = {41, 42, 43};
const u16 kSeVolumeDigitsPlain[3] = {45, 46, 47};
const u16 kSeVolumeDigitsEmphasis[3] = {49, 50, 51};

// Resolves the container entity through the cached handle (clearing the cache
// when the lookup fails), then walks its child node list (nodes are
// {child_entity, next}) for the child whose u16 id at +0x38a equals slot_id
// and returns that child's first dword, its own manager handle. The walk is
// unchecked exactly like the native code: a stale container handle leaves a
// zero cache and the next resolution walks the list head at address 0x10.
u32 ResolveVolumeDigitHandleId(void *owner, u32 *cached_handle, u16 slot_id)
{
    u8 *container =
        FindEntityEdxStackAbi(owner, *cached_handle);
    if (container == 0)
        *cached_handle = 0;

    u8 *node = container + 0x10;
    while (node != 0) {
        u8 *child = *reinterpret_cast<u8 **>(node);
        if (*reinterpret_cast<u16 *>(child + 0x38a) == slot_id)
            return *reinterpret_cast<u32 *>(child);
        node = *reinterpret_cast<u8 **>(node + 4);
    }
    return 0;
}

// Re-binds one volume digit glyph: resolves the child's VM record again in
// the owner list and runs the 0x43e5a0 entry with the digit character code,
// passing the VM's own +0x308 resource block.
void BindVolumeDigit(void *owner, u32 *cached_handle, u16 slot_id,
                     i32 digit_char)
{
    const u32 child_id = ResolveVolumeDigitHandleId(owner, cached_handle, slot_id);
    u8 *vm = FindEntityEdxStackAbi(owner, child_id);
    if (vm != 0) {
        InitializeAsciiAnimationVmEntry(
            vm, static_cast<u32>(digit_char),
            *reinterpret_cast<void **>(vm + 0x308));
    }
}

// Toggles the disabled bit (0x200) of one digit glyph's state word at +0x35c.
// Like the native code this does not check the resolved VM record.
void SetVolumeDigitVisibility(void *owner, u32 *cached_handle, u16 slot_id,
                              bool visible)
{
    const u32 child_id = ResolveVolumeDigitHandleId(owner, cached_handle, slot_id);
    u8 *vm = FindEntityEdxStackAbi(owner, child_id);
    u32 *state_word = reinterpret_cast<u32 *>(vm + 0x35c);
    if (visible)
        *state_word |= 2u;
    else
        *state_word &= ~2u;
}

// Applies the native leading-zero rule to one volume's four glyphs (plain
// hundreds/tens/ones plus emphasized hundreds): both hundreds digits and the
// plain tens digit hide while the value has fewer digits.
void UpdateVolumeDigitGroup(void *owner, u32 *cached_handle,
                            const u16 plain_digits[3],
                            const u16 emphasis_digits[3], i32 hundreds_visible,
                            i32 tens_visible)
{
    SetVolumeDigitVisibility(owner, cached_handle, plain_digits[0],
                             hundreds_visible != 0);
    SetVolumeDigitVisibility(owner, cached_handle, plain_digits[1],
                             tens_visible != 0);
    SetVolumeDigitVisibility(owner, cached_handle, emphasis_digits[0],
                             hundreds_visible != 0);
    SetVolumeDigitVisibility(owner, cached_handle, emphasis_digits[1],
                             tens_visible != 0);
}

} // namespace

i32 g_BgmDirectSoundAttenuation; // TH10 DAT_0049795c

// TH10 0x0042e5a0.
void RefreshAudioOptionPresentationEdiAbi(void *options_state)
{
    u32 *const words = static_cast<u32 *>(options_state);
    u32 *const cached_text_handle = words + kOptionsTextHandleSlot;

    // Publish the BGM volume and queue the volume-update command for the BGM
    // worker before touching the SE volume state.
    g_TransitionVolumeScale = g_BgmVolumeInputConfig;
    QueueBgmCommand(&g_TransitionRoot, "SetVol", 8, 0);

    g_BgmSoundSequenceVolumeScale = g_BgmVolumeEnabledConfig;
    if (g_BgmSoundSequenceVolumeScale != 0) {
        const double falloff = 1.0 - static_cast<double>(g_TransitionVolumeScale) *
                                         0.0099999998f;
        g_BgmDirectSoundAttenuation =
            -5000 - static_cast<i32>((1.0 - falloff * falloff) * -5000.0);
    } else {
        g_BgmDirectSoundAttenuation = -10000;
    }

    void *const owner = g_MainChainRenderOwner;

    const i32 music_volume = g_BgmVolumeInputConfig;   // DAT_00491d68
    const i32 sound_volume = g_BgmVolumeEnabledConfig; // DAT_00491d69

    // Twelve digit glyph re-binds, in native order: BGM plain, BGM emphasized,
    // SE plain, SE emphasized; each hundreds/tens/ones.
    BindVolumeDigit(owner, cached_text_handle, kBgmVolumeDigitsPlain[0],
                    music_volume / 100 + kDigitBasePlain);
    BindVolumeDigit(owner, cached_text_handle, kBgmVolumeDigitsPlain[1],
                    music_volume / 10 % 10 + kDigitBasePlain);
    BindVolumeDigit(owner, cached_text_handle, kBgmVolumeDigitsPlain[2],
                    music_volume % 10 + kDigitBasePlain);
    BindVolumeDigit(owner, cached_text_handle, kBgmVolumeDigitsEmphasis[0],
                    music_volume / 100 + kDigitBaseEmphasis);
    BindVolumeDigit(owner, cached_text_handle, kBgmVolumeDigitsEmphasis[1],
                    music_volume / 10 % 10 + kDigitBaseEmphasis);
    BindVolumeDigit(owner, cached_text_handle, kBgmVolumeDigitsEmphasis[2],
                    music_volume % 10 + kDigitBaseEmphasis);
    BindVolumeDigit(owner, cached_text_handle, kSeVolumeDigitsPlain[0],
                    sound_volume / 100 + kDigitBasePlain);
    BindVolumeDigit(owner, cached_text_handle, kSeVolumeDigitsPlain[1],
                    sound_volume / 10 % 10 + kDigitBasePlain);
    BindVolumeDigit(owner, cached_text_handle, kSeVolumeDigitsPlain[2],
                    sound_volume % 10 + kDigitBasePlain);
    BindVolumeDigit(owner, cached_text_handle, kSeVolumeDigitsEmphasis[0],
                    sound_volume / 100 + kDigitBaseEmphasis);
    BindVolumeDigit(owner, cached_text_handle, kSeVolumeDigitsEmphasis[1],
                    sound_volume / 10 % 10 + kDigitBaseEmphasis);
    BindVolumeDigit(owner, cached_text_handle, kSeVolumeDigitsEmphasis[2],
                    sound_volume % 10 + kDigitBaseEmphasis);

    // Leading-zero visibility: hide hundreds (and the plain tens digit for
    // single-digit values) exactly as the native branch structure does.
    const i32 music_hundreds = music_volume >= 100 ? 1 : 0;
    const i32 music_tens = music_volume >= 10 ? 1 : 0;
    UpdateVolumeDigitGroup(owner, cached_text_handle, kBgmVolumeDigitsPlain,
                           kBgmVolumeDigitsEmphasis, music_hundreds, music_tens);
    const i32 sound_hundreds = sound_volume >= 100 ? 1 : 0;
    const i32 sound_tens = sound_volume >= 10 ? 1 : 0;
    UpdateVolumeDigitGroup(owner, cached_text_handle, kSeVolumeDigitsPlain,
                           kSeVolumeDigitsEmphasis, sound_hundreds, sound_tens);
}

} // namespace th10
