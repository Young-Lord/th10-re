// Scene-owner stage script resource open (TH10 0x00413a20). Companion of
// 0x00413980 (which loads "front.anm" into +0x9ec8 and then calls here).
#include "StageScriptOpen.hpp"

#include "AsciiHudOwner.hpp"
#include "ManagerWork.hpp"

namespace th10 {

namespace {

// ---- shared globals ----------------------------------------------------

extern void *g_PublishedModeRecord; // TH10 DAT_00477848 (+0x20 script name,
                                    //  +0x18 stage-name table)
extern void *g_EntityPoolManager;   // TH10 DAT_00491c10 (manager-work owner)
extern void *g_PreopenedScriptHandle; // TH10 DAT_00491be8 (demo handle)
extern u32 g_SceneStageIndexA;      // TH10 DAT_00474c68
extern u32 g_SceneSubTimer;         // TH10 DAT_00474c44

// TH10 DAT_00497c38: the shared scene path scratch. Each module models the
// singleton as its own internal buffer (matching the TU-local pattern used
// by the timeline loaders).
char g_SceneScriptPathScratch[256];

// Boundary leaves (native ABIs noted at each site).

// TH10 0x0044b360 (body in MainChainFileProbe.cpp). Native EAX = path,
// stack = (0, 0): open through the resource loader with no size out and
// source mode 0.
extern void *LoadMainChainFile(const char *path, u32 *file_size,
                               i32 filesystem_mode);

// TH10 0x0044b810. Native ECX = text context (DAT_00474f70), stack =
// format string; formats the load-error text.
extern void FormatLoadErrorText(void *text_context, const char *format);
const u32 kErrorTextContext = 0x474f70U;
const char kLoadErrorFormat[] =
    "ERROR: cannot open the stage script resource"; // TH10 0x0046cb68
                                                    // (boundary text)

// Scene owner offsets (the record is the 0x9ed0-byte ASCII HUD owner; the
// +0x9e9c open-script handle overlaps spell_bars[1].value and stays raw).
const u32 kOffScriptHandle = 0x9e9cU; // open script handle

const u32 kScriptWorkSlot = 28U; // native ECX = 0x1c

inline u32 LoadU32(const u8 *bytes, u32 offset)
{
    return *reinterpret_cast<const u32 *>(bytes + offset);
}

inline void StoreU32(u8 *bytes, u32 offset, u32 value)
{
    *reinterpret_cast<u32 *>(bytes + offset) = value;
}

// strcat into the path scratch, then reopen it as the loader path.
void AppendPathScratch(const char *name)
{
    g_SceneScriptPathScratch[0] = '\0';
    char *dst = g_SceneScriptPathScratch;
    while (*dst != '\0')
        ++dst;
    const char *src = name;
    char c;
    do {
        c = *src++;
        *dst++ = c;
    } while (c != '\0');
}

} // namespace

// TH10 0x00413a20. Native stdcall (ret 4).
i32 OpenSceneScriptResource(void *scene_owner)
{
    AsciiHudOwner &hud = *reinterpret_cast<AsciiHudOwner *>(scene_owner);

    void *const mode_record = g_PublishedModeRecord;
    const char *const *name_slot = reinterpret_cast<const char *const *>(
        const_cast<u8 *>(static_cast<const u8 *>(mode_record)) + 0x20U);
    const char *script_name = *name_slot;

    void *work = RequestManagerWork(
        static_cast<ManagerWorkOwnerPartial *>(g_EntityPoolManager),
        static_cast<i32>(kScriptWorkSlot), script_name);
    hud.stage_script_work = work;
    if (work == 0) {
        FormatLoadErrorText(reinterpret_cast<void *>(kErrorTextContext),
                            kLoadErrorFormat);
        return -1;
    }

    void *preopened = g_PreopenedScriptHandle;
    if (preopened != 0) {
        // Demo/reuse path: consume the pre-opened handle once.
        StoreU32(reinterpret_cast<u8 *>(&hud), kOffScriptHandle,
                 reinterpret_cast<u32>(preopened));
        g_PreopenedScriptHandle = 0;
    } else {
        const u8 *name_table = static_cast<const u8 *>(mode_record);
        char *const *slot = reinterpret_cast<char *const *>(
            const_cast<u8 *>(name_table) + 0x18U + 4U * g_SceneStageIndexA);
        const char *stage_name = *slot;
        AppendPathScratch(stage_name);
        void *handle = LoadMainChainFile(g_SceneScriptPathScratch, 0, 0);
        StoreU32(reinterpret_cast<u8 *>(&hud), kOffScriptHandle,
                 reinterpret_cast<u32>(handle));
        if (handle == 0) {
            FormatLoadErrorText(reinterpret_cast<void *>(kErrorTextContext),
                                kLoadErrorFormat);
            return -1;
        }
    }

    // Arm the script timer block (lazy init guarded by bit 0 of
    // hud_timer.flags, then the unconditional -1/0/0 reset) and copy the
    // scene sub-timer into the displayed-score dword (shared duty).
    const u32 timer_flags = hud.hud_timer.flags;
    if ((timer_flags & 1U) == 0U) {
        hud.hud_timer.prev = static_cast<i32>(-999999);
        hud.hud_timer.count = 0;
        hud.hud_timer.accum = 0;
        hud.hud_timer.rate = reinterpret_cast<const float *>(0x476f78U);
        hud.hud_timer.flags = timer_flags | 1U;
    }
    hud.hud_timer.count = 0;
    hud.hud_timer.accum = 0;
    hud.hud_timer.prev = static_cast<i32>(-1);
    hud.spell_countdown = -1;
    hud.last_spell_countdown = -1;
    hud.displayed_score = g_SceneSubTimer;
    return 0;
}

} // namespace th10
