// TH10 ECL script object lifecycle helpers: the 0x1098-byte viewer
// objects (vtable 0x46d0d8 / 0x46d0f0), their deferred-name lists, the
// entity-binding release used at teardown, and the script-file load
// entry that registers section names.

#include "Th10Types.hpp"
#include "EntityHelpers.hpp"
#include "ScriptTestMenu.hpp"
#include "ThreadControl.hpp"

// Boundary callees: the shared file loader (TH10 0x44b360, implemented in
// MainChainFileProbe.cpp) and the script-name registrar (TH10 0x450220,
// implemented in EclScriptNameTable.cpp).
namespace th10 {
void *LoadMainChainFile(const char *path, u32 *file_size, i32 filesystem_mode);
i32 RegisterScriptFileNamesThisStackAbi(void *registry, void *file_block);
} // namespace th10

namespace th10 {

extern void *g_MainChainRenderOwner; // TH10 ds:0x491c10

// Boundaries.
extern void *LoadMainChainFile(const char *path, u32 *file_size,
                               void *unused); // TH10 0x44b360
extern void CrtFree(void *memory);             // TH10 0x4524a1

namespace {
const u32 kEntityReleaseFlag = 0x4000000U;
const u32 kDefaultViewerHeader = 0x4703e4U;
} // namespace

// TH10 0x0040b250. Native ECX = script object, EDX = 0x2c-byte source
// block. Allocates a 0x2c-byte copy, replicates all 11 dwords, stores it
// at object+0x358, and mirrors the copy's first three dwords into the
// object's +0x340 position triple. Returns 0.
i32 StoreScriptObjectInsnBlockEcxEcxAbi(void *object, const void *source) {
    u8 *bytes = static_cast<u8 *>(object);
    u32 *copy = static_cast<u32 *>(::operator new(0x2cU));
    const u32 *src = static_cast<const u32 *>(source);
    for (int i = 0; i < 0xb; ++i) {
        copy[i] = src[i];
    }
    *reinterpret_cast<u32 **>(bytes + 0x358) = copy;
    u32 *position = reinterpret_cast<u32 *>(bytes + 0x340);
    position[0] = copy[0];
    position[1] = copy[1];
    position[2] = copy[2];
    return 0;
}

// TH10 0x0040b3a0. Native EAX = script object. Stops the embedded thread
// control twice (before and after the entity sweep), resolves each of
// the five entity ids at +0x40..+0x50 against the render owner's two
// entity lists (+0x72dad4 / +0x72dadc), raises the 0x4000000 release
// flag on the entity (and over its +0x14 child chain when the +0x18
// count is zero), clears the id slot, and finally publishes the default
// 0x4703e4 header at the +0xd0 sub-record. Returns nothing.
void ReleaseScriptObjectEntityBindingsEaxAbi(void *object) {
    u8 *bytes = static_cast<u8 *>(object);
    u8 *thread_control = bytes + 0xd0; // embedded thread control block
    StopThreadControl(reinterpret_cast<ThreadControl *>(thread_control));

    for (int slot = 0; slot < 5; ++slot) {
        u32 *id_slot = reinterpret_cast<u32 *>(bytes + 0x40 + slot * 4);
        const u32 id = *id_slot;
        if (id == 0) {
            *id_slot = 0;
            continue;
        }
        u8 *entity = static_cast<u8 *>(FindEntityEdxStackAbi(
            g_MainChainRenderOwner, id));
        if (entity != 0) {
            *reinterpret_cast<u32 *>(entity + 0x35c) |= kEntityReleaseFlag;
            if (*reinterpret_cast<u32 *>(entity + 0x18) == 0) {
                u32 *child = reinterpret_cast<u32 *>(
                    *reinterpret_cast<u32 *>(entity + 0x14));
                while (child != 0) {
                    *reinterpret_cast<u32 *>(*child + 0x35c) |=
                        kEntityReleaseFlag;
                    child = reinterpret_cast<u32 *>(child[1]);
                }
            }
        }
        *id_slot = 0;
    }

    *reinterpret_cast<u32 *>(thread_control) = kDefaultViewerHeader;
    StopThreadControl(reinterpret_cast<ThreadControl *>(thread_control));
}

// TH10 0x0040c5d0. Native EAX = viewer: clear the +0x1000/+0x1004 flag
// pair.
void ClearViewerPauseFlagsEaxAbi(void *viewer) {
    u8 *bytes = static_cast<u8 *>(viewer);
    *reinterpret_cast<u32 *>(bytes + 0x1000) = 0;
    *reinterpret_cast<u32 *>(bytes + 0x1004) = 0;
}

// TH10 0x0040c6e0. Native EAX = viewer: free the deferred-name linked
// list at +0x1034 (nodes {data, next}); each node's data pointer and the
// node itself go through the CRT free.
void FreeViewerDeferredListEaxAbi(void *viewer) {
    u8 *bytes = static_cast<u8 *>(viewer);
    u32 *node = *reinterpret_cast<u32 **>(bytes + 0x1034);
    while (node != 0) {
        u32 *next = reinterpret_cast<u32 *>(node[1]);
        CrtFree(reinterpret_cast<void *>(node[0]));
        CrtFree(node);
        node = next;
    }
}

// TH10 0x0040c710. Native EAX = viewer: publish the 0x46d0d8 vtable and
// clear the +0x1010/+0x1014 pair.
void InitViewerHeaderEaxAbi(void *viewer) {
    u8 *bytes = static_cast<u8 *>(viewer);
    *reinterpret_cast<u32 *>(bytes) = 0x46d0d8U;
    *reinterpret_cast<u32 *>(bytes + 0x1010) = 0;
    *reinterpret_cast<u32 *>(bytes + 0x1014) = 0;
}

// TH10 0x0040c730. Native EAX = viewer: clear the +0x1028 busy bit,
// reset the +0x8/+0xc counters, self-pointers +0x101c/+0x4/+0x1030, and
// clear the +0x1018 sentinel, +0x1020 and the +0x1034/+0x1038 list
// heads.
void InitViewerListsEaxAbi(void *viewer) {
    u8 *bytes = static_cast<u8 *>(viewer);
    *reinterpret_cast<u32 *>(bytes + 0x1028) &= ~1u;
    *reinterpret_cast<u32 *>(bytes + 0x8) = 0;
    *reinterpret_cast<u32 *>(bytes + 0xc) = 0;
    *reinterpret_cast<u32 *>(bytes + 0x101c) =
        reinterpret_cast<u32>(viewer);
    *reinterpret_cast<u32 *>(bytes + 0x1018) = 0xffffffffU;
    *reinterpret_cast<u32 *>(bytes + 0x1020) = 0;
    *reinterpret_cast<u32 *>(bytes + 0x4) =
        reinterpret_cast<u32>(bytes + 0x8);
    *reinterpret_cast<u32 *>(bytes + 0x1030) =
        reinterpret_cast<u32>(bytes + 0x8);
    *reinterpret_cast<u32 *>(bytes + 0x1034) = 0;
    *reinterpret_cast<u32 *>(bytes + 0x1038) = 0;
}

// TH10 0x0040c780. Native ECX = viewer: publish the 0x46d0d8 vtable and
// free the +0x1034 deferred list.
void FreeViewerListEcxEcxAbi(void *viewer) {
    InitViewerHeaderEaxAbi(viewer);
    FreeViewerDeferredListEaxAbi(viewer);
}

// TH10 0x0040c7b0. Native ECX = viewer, stack = free flag (ret 4):
// vtable + list free, then release the outer allocation when the flag's
// bit 0 is set. Returns the viewer.
void *DestroyViewerEcxEcxAbi(void *viewer, u32 free_flag) {
    FreeViewerListEcxEcxAbi(viewer);
    if ((free_flag & 1) != 0) {
        CrtFree(viewer);
    }
    return viewer;
}

// TH10 0x0040c800. Native EAX = viewer: clear the +0x1008/+0x100c pair.
void ClearViewerTickFlagsEaxAbi(void *viewer) {
    u8 *bytes = static_cast<u8 *>(viewer);
    *reinterpret_cast<u32 *>(bytes + 0x1008) = 0;
    *reinterpret_cast<u32 *>(bytes + 0x100c) = 0;
}

// TH10 0x0040cc70. Native EDX = the ECL script object. Clears bit 1 of
// the scattered flag dwords at +0x12c..+0x2b8, wipes the eight 0x210-
// byte sub-records at +0x2c4 (each keeping the -1 sentinel at +0x204),
// and clears the +0x142c / +0x1440 flags. Returns the object.
void *ResetScriptObjectSubrecordBatchEdxAbi(void *object) {
    u8 *bytes = static_cast<u8 *>(object);
    static const u32 kFlagOffsets[7] = {
        0x12c, 0x17c, 0x1c8, 0x204, 0x240, 0x27c, 0x2b8};
    for (int i = 0; i < 7; ++i) {
        u32 *flag = reinterpret_cast<u32 *>(bytes + kFlagOffsets[i]);
        *flag &= ~2u;
    }
    u8 *record = bytes + 0x2c4;
    for (int r = 0; r < 8; ++r, record += 0x210) {
        u32 *wipe = reinterpret_cast<u32 *>(record);
        for (int i = 0; i < 0x84; ++i) {
            wipe[i] = 0;
        }
        *reinterpret_cast<u32 *>(record + 0x204) = 0xffffffffU;
    }
    *reinterpret_cast<u32 *>(bytes + 0x142c) &= ~2u;
    *reinterpret_cast<u32 *>(bytes + 0x1440) &= ~2u;
    return object;
}

// TH10 0x0040cf40. Native EDX = a 0x1f8-byte record (ECL instruction
// target): wipe it and seed the default scale 8.0 at +0x2c.
void *InitInstructionTargetRecordEdxAbi(void *record) {
    u32 *wipe = static_cast<u32 *>(record);
    for (int i = 0; i < 0x7e; ++i) {
        wipe[i] = 0;
    }
    *reinterpret_cast<u32 *>(static_cast<u8 *>(record) + 0x2c) =
        0x41000000U; // 8.0f
    return record;
}

// TH10 0x0040cf90. Native EAX = owner, ECX = unique id: walk the linked
// list at +0x18 (next at node+0x8) and return the node whose +0x54
// equals the id, or null.
void *FindScriptObjectByUniqueIdEcxEaxAbi(void *owner, u32 unique_id) {
    u8 *node = *reinterpret_cast<u8 **>(
        static_cast<u8 *>(owner) + 0x18);
    while (node != 0) {
        if (*reinterpret_cast<u32 *>(node + 0x54) == unique_id) {
            return node;
        }
        node = *reinterpret_cast<u8 **>(node + 0x8);
    }
    return 0;
}

// TH10 0x0040cd20. Native ECX = registry, stack = script path (ret 4).
// Copies the path into the shared scratch buffer at 0x497c38, loads it
// through the shared file loader (0x44b360, null size/null tail), hands
// the loaded block to the name registrar (0x450220) and returns 0 when
// the loader reported success (>= 0) or -1 otherwise.
i32 LoadScriptFileAndRegisterNamesEcxEcxAbi(void *registry,
                                            const char *path) {
    char *const scratch = reinterpret_cast<char *>(0x497c38U);
    char *out = scratch;
    const char *in = path;
    do {
        *out++ = *in;
    } while (*in++ != '\0');

    void *block = LoadMainChainFile(scratch, 0, 0);
    // The >= 0 check applies to the registrar's return value.
    const i32 result = RegisterScriptFileNamesThisStackAbi(registry, block);
    return (result >= 0) ? 0 : -1;
}

} // namespace th10
