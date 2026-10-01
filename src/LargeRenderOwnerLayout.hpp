#pragma once

#include <stddef.h>

#include "MainChainRender.hpp"
#include "Th10Types.hpp"
#include "VmRecord.hpp"

namespace th10 {

// Intrusive list link for the render-owner node chains (node+0x4, list
// heads at owner+0x72dad4/+0x72dad8/+0x72dadc/+0x72dae0).
struct OwnerLink {
    void *self_node;
    OwnerLink *next;
    OwnerLink *previous;
};

typedef char AssertOwnerLinkSize[sizeof(OwnerLink) == 0xc ? 1 : -1];

// Canonical layout of the 0x732460-byte render owner held in TH10
// DAT_00491c10 (source alias g_MainChainRenderOwner). It allocates as one
// block (AllocateLargeRenderOwner, TH10 0x00452493) and hosts: the 4096-slot
// VmRecord node pool, the manager-work slots, the render-target/surface
// caches, the per-frame D3D9 render-state caches, the pending-quad vertex
// arena, and the four node-list heads. Field evidence: src/LargeRenderOwner.cpp
// (construct/teardown), src/MainChainRenderAdapters.cpp, src/
// RenderOwnerDrawHelpers.cpp, src/LargeRenderOwnerFrameLoop.cpp, src/
// MainChainRender.hpp (MainChainRenderOwnerFrameState). The same layout is
// declared in the IDB as struct LargeRenderOwnerLayout.
struct LargeRenderOwnerLayout {
    i32 deferred_slot_0;        // +0x0000 (first deferred batch cache slot; -1 sentinel)
    i32 deferred_slot_1;        // +0x0004
    u32 batch1_rects[8];        // +0x0008 (second batch {src x,y,w,h; dst x,y,w,h})
    u32 batch1_source_slot;     // +0x0028
    u32 batch0_rects[8];        // +0x002c (first batch rects)
    i32 live_object_count;      // +0x004c
    u8 gap0050[4];              // +0x0050
    u32 render_mode_counter;    // +0x0054
    u32 flush_counter;          // +0x0058
    u32 camera_value_005c;      // +0x005c (camera work +0xe8)
    u32 camera_value_0060;      // +0x0060 (camera work +0xec)
    u32 frame_counter;          // +0x0064 (per-node iterate counter)
    VmRecord pooled_nodes[4096];// +0x0068 (stride 0x3ac)
    u8 pooled_node_active[4096];// +0x3ac068
    u32 pool_cursor;            // +0x3ad068 ((v+1)&0x80000fff wrap)
    void *work_slots[33];       // +0x3ad06c (manager work / texture source groups)
    float world_matrix[16];     // +0x3ad0f0 (D3DMatrix, ASCII projected rendering)
    u8 gap3ad130[856];          // +0x3ad130
    void *owned_3ad488;         // +0x3ad488 (released with the owner)
    u8 gap3ad48c[84];           // +0x3ad48c
    void *render_targets[32];   // +0x3ad4e0 (device-reset sensitive)
    void *shadow_surfaces[32];  // +0x3ad560 (system-memory pool 3)
    void *cached_file_buffers[32]; // +0x3ad5e0
    u32 cached_file_sizes[32];  // +0x3ad660
    CachedRenderSurfaceDescriptor surface_descriptors[32]; // +0x3ad6e0
    u32 modulation_color;       // +0x3ada60 (cached vertex modulation color)
    void *bound_texture;        // +0x3ada64 (SetTexture cache)
    u8 blend_mode_cache;        // +0x3ada68 (entity flags bits 4-5; reset 3)
    u8 state_cache_3ada69;      // +0x3ada69 (reset 0xff)
    u8 fvf_active_cache;        // +0x3ada6a (reset 0xff; 1/2/3 -> SetFVF 0x144)
    u8 state_cache_3ada6b;      // +0x3ada6b (reset 0xff)
    u8 ascii_scene_active;      // +0x3ada6c (construct 0xff; 1 in ASCII scene)
    u8 gap3ada6d;               // +0x3ada6d
    u8 sampler_filter_cache;    // +0x3ada6e (0 linear -> 2, 1 point -> 1)
    u8 gap3ada6f;               // +0x3ada6f
    void *glyph_texture_cache;  // +0x3ada70 (mode-8 glyph texture)
    void *com_viewport_interface; // +0x3ada74 (viewport creator / mode-8 VB)
    float default_constants[20];// +0x3ada78 (-128.0/128.0/1.0 block)
    u32 pending_quad_count;     // +0x3adac8 (reset to 0 after flush)
    u8 vertex_arena[0x380000];  // +0x3adacc (6 verts/quad appended per quad)
    void *vertex_write_cursor;  // +0x72dacc (next vertex write; reset to base)
    void *draw_source;          // +0x72dad0 (DrawPrimitiveUP source at flush)
    OwnerLink *first_list_a;    // +0x72dad4
    OwnerLink *last_list_a;     // +0x72dad8
    OwnerLink *first_list_b;    // +0x72dadc
    OwnerLink *last_list_b;     // +0x72dae0
    VmRecord late_nodes[20];    // +0x72dae4 (19 kind-bucket sentinels + tail)
    i32 node_id_counter;        // +0x732454 (wraps past 0 to 1)
    u32 clear_color;            // +0x732458 (also per-byte modulation, >>7)
    u32 custom_color_gate;      // +0x73245c
};

typedef char AssertLargeRenderOwnerLayoutSize[
    sizeof(LargeRenderOwnerLayout) == 0x732460 ? 1 : -1];
typedef char AssertLargeRenderOwnerPooledNodesOffset[
    offsetof(LargeRenderOwnerLayout, pooled_nodes) == 0x68 ? 1 : -1];
typedef char AssertLargeRenderOwnerNodeActiveOffset[
    offsetof(LargeRenderOwnerLayout, pooled_node_active) == 0x3ac068 ? 1 : -1];
typedef char AssertLargeRenderOwnerPoolCursorOffset[
    offsetof(LargeRenderOwnerLayout, pool_cursor) == 0x3ad068 ? 1 : -1];
typedef char AssertLargeRenderOwnerWorkSlotsOffset[
    offsetof(LargeRenderOwnerLayout, work_slots) == 0x3ad06c ? 1 : -1];
typedef char AssertLargeRenderOwnerWorldMatrixOffset[
    offsetof(LargeRenderOwnerLayout, world_matrix) == 0x3ad0f0 ? 1 : -1];
typedef char AssertLargeRenderOwnerOwnedOffset[
    offsetof(LargeRenderOwnerLayout, owned_3ad488) == 0x3ad488 ? 1 : -1];
typedef char AssertLargeRenderOwnerRenderTargetsOffset[
    offsetof(LargeRenderOwnerLayout, render_targets) == 0x3ad4e0 ? 1 : -1];
typedef char AssertLargeRenderOwnerShadowSurfacesOffset[
    offsetof(LargeRenderOwnerLayout, shadow_surfaces) == 0x3ad560 ? 1 : -1];
typedef char AssertLargeRenderOwnerSurfaceDescriptorsOffset[
    offsetof(LargeRenderOwnerLayout, surface_descriptors) == 0x3ad6e0 ? 1 : -1];
typedef char AssertLargeRenderOwnerModulationColorOffset[
    offsetof(LargeRenderOwnerLayout, modulation_color) == 0x3ada60 ? 1 : -1];
typedef char AssertLargeRenderOwnerBoundTextureOffset[
    offsetof(LargeRenderOwnerLayout, bound_texture) == 0x3ada64 ? 1 : -1];
typedef char AssertLargeRenderOwnerFvfCacheOffset[
    offsetof(LargeRenderOwnerLayout, fvf_active_cache) == 0x3ada6a ? 1 : -1];
typedef char AssertLargeRenderOwnerAsciiSceneOffset[
    offsetof(LargeRenderOwnerLayout, ascii_scene_active) == 0x3ada6c ? 1 : -1];
typedef char AssertLargeRenderOwnerSamplerCacheOffset[
    offsetof(LargeRenderOwnerLayout, sampler_filter_cache) == 0x3ada6e ? 1 : -1];
typedef char AssertLargeRenderOwnerGlyphTextureOffset[
    offsetof(LargeRenderOwnerLayout, glyph_texture_cache) == 0x3ada70 ? 1 : -1];
typedef char AssertLargeRenderOwnerDefaultConstantsOffset[
    offsetof(LargeRenderOwnerLayout, default_constants) == 0x3ada78 ? 1 : -1];
typedef char AssertLargeRenderOwnerPendingQuadCountOffset[
    offsetof(LargeRenderOwnerLayout, pending_quad_count) == 0x3adac8 ? 1 : -1];
typedef char AssertLargeRenderOwnerVertexArenaOffset[
    offsetof(LargeRenderOwnerLayout, vertex_arena) == 0x3adacc ? 1 : -1];
typedef char AssertLargeRenderOwnerVertexCursorOffset[
    offsetof(LargeRenderOwnerLayout, vertex_write_cursor) == 0x72dacc ? 1 : -1];
typedef char AssertLargeRenderOwnerDrawSourceOffset[
    offsetof(LargeRenderOwnerLayout, draw_source) == 0x72dad0 ? 1 : -1];
typedef char AssertLargeRenderOwnerFirstListAOffset[
    offsetof(LargeRenderOwnerLayout, first_list_a) == 0x72dad4 ? 1 : -1];
typedef char AssertLargeRenderOwnerLateNodesOffset[
    offsetof(LargeRenderOwnerLayout, late_nodes) == 0x72dae4 ? 1 : -1];
typedef char AssertLargeRenderOwnerNodeIdCounterOffset[
    offsetof(LargeRenderOwnerLayout, node_id_counter) == 0x732454 ? 1 : -1];
typedef char AssertLargeRenderOwnerClearColorOffset[
    offsetof(LargeRenderOwnerLayout, clear_color) == 0x732458 ? 1 : -1];
typedef char AssertLargeRenderOwnerCustomColorGateOffset[
    offsetof(LargeRenderOwnerLayout, custom_color_gate) == 0x73245c ? 1 : -1];

} // namespace th10
