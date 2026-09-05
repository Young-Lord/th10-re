#include <stdio.h>

#include "PackedArchive.hpp"
#include "VersionData.hpp"

namespace th10 {

namespace {

extern u32 g_VersionDataSize; // TH10 DAT_00492388
extern void *g_VersionData; // TH10 DAT_0049238c

extern void ReportVersionDataFailure(const char *message); // 0x44b8e0
extern void ReleaseResourceBuffer(void *pointer); // TH10 0x00452422
extern PackedArchive g_PackedArchive; // TH10 DAT_00497990

const char kArchiveName[] = "th10.dat";
const char kArchiveMissing[] =
    "error : \x83\x66\x81\x5b\x83\x5e\x83\x74\x83\x40\x83\x43"
    "\x83\x8b\x82\xaa\x91\xb6\x8d\xdd\x82\xb5\x82\xdc\x82\xb9\x82\xf1\r\n";
const char kVersionMismatch[] =
    "error : \x83\x66\x81\x5b\x83\x5e\x82\xcc\x83\x6f\x81\x5b\x83\x57"
    "\x83\x87\x83\x93\x82\xaa\x88\xe1\x82\xa2\x82\xdc\x82\xb7\r\n";

} // namespace

// TH10 0x00420100. The binary reports failures but its caller deliberately
// continues startup; that caller-side policy is preserved in the hook.
i32 InitializeVersionData()
{
    if (!g_PackedArchive.OpenAndIndex(kArchiveName)) {
        ReportVersionDataFailure(kArchiveMissing);
        return -1;
    }

    char name[0x100];
    sprintf(name, "th10_%.4x%c.ver", 0x100, 'a');
    u32 size = 0;
    g_VersionData = LoadPackedResource(name, &size, 0);
    g_VersionDataSize = size;

    if (g_VersionData != 0)
        return 0;

    g_VersionData = 0;
    ReportVersionDataFailure(kVersionMismatch);
    return -1;
}

void ReleaseVersionData()
{
    if (g_VersionData != 0)
        ReleaseResourceBuffer(g_VersionData);
    g_VersionData = 0;
}

} // namespace th10
