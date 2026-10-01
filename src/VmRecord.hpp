#pragma once

#include <stddef.h>

#include "Th10Types.hpp"

namespace th10 {

// ---------------------------------------------------------------------------
// Canonical layout of the 0x3AC-byte entity / VM record used by every
// timeline-render system: the render-owner node pool (owner+0x68, 4096
// slots), the effect pools, stage objects, spell/bullet pools, HUD VM
// arrays, and the heap fallback of AllocatePoolVmEsiAbi (TH10 0x449950).
// Layout evidence is consolidated from src/EntityHelpers.cpp,
// src/TimelineRenderObjectSetup.cpp (the ANM setup-opcode interpreter),
// src/AsciiAnimationVm.cpp, src/VmLeafHelpers.cpp, src/EntityFieldSetters.cpp,
// src/EntityRenderStateApply.cpp, src/PlayerShotData.cpp and the Ascii*
// renderers. The same layout is declared in the IDB as struct VmRecord.
//
// +0x35c flags bit map (see VmRecordFlags below for the named masks).
// ---------------------------------------------------------------------------

enum VmRecordFlags {
    // bit 0: script running (ECL opcode 3 set; 1/0xFFFF/bind clear).
    VmRecordFlag_ScriptRunning = 0x1,
    // bit 1: enabled/visible; renderers require (flags & 3) == 3.
    VmRecordFlag_Enabled = 0x2,
    VmRecordFlag_RotationDirty = 0x4,
    VmRecordFlag_ScaleDirty = 0x8,
    // bits 4-5: blend mode 0-3 -> D3DRS_DESTBLEND.
    VmRecordFlag_BlendModeShift = 4,
    VmRecordFlag_BlendModeMask = 0x30,
    // bits 8-9: cleared on script bind (position-mode select pair).
    VmRecordFlag_PositionModeMask = 0x600,
    VmRecordFlag_KindRestartClear = 0x1000,
    // bit 13: opcode 0x4a adds the global scroll offsets to +0x340.
    VmRecordFlag_AddGlobalScroll = 0x2000,
    // bit 14: matrix-rebuild inhibit gate for render modes 7/8.
    VmRecordFlag_MatrixRebuildInhibit = 0x4000,
    // bit 15: color source select (0: +0x2fc primary, 1: +0x300 secondary).
    VmRecordFlag_SecondaryColorSource = 0x8000,
    // bits 18-19 / 20-21: X/Y anchor (0 center, 1 left/top, 2 right/bottom).
    VmRecordFlag_AnchorXShift = 18,
    VmRecordFlag_AnchorXMask = 0xc0000,
    VmRecordFlag_AnchorYShift = 20,
    VmRecordFlag_AnchorYMask = 0x300000,
    // bits 22-25: render mode 0-15 consumed by AsciiRenderModeDispatcher.
    VmRecordFlag_RenderModeShift = 22,
    VmRecordFlag_RenderModeMask = 0x3c00000,
    // bit 26: soft release/recycle (ReleaseEntityById 0x4492a0).
    VmRecordFlag_SoftRelease = 0x4000000,
    // bit 27/28: toggled by setup opcodes 0x52 / 0x55.
    VmRecordFlag_Op52Toggle = 0x8000000,
    VmRecordFlag_Op55Toggle = 0x10000000,
    // bit 29: force frame-time scale 1.0 during setup (opcode 0x56).
    VmRecordFlag_ForceFrameTimeScale = 0x20000000,
    // bit 30: script-bind/alive marker; also selects PRNG state A.
    VmRecordFlag_ScriptBound = 0x40000000,
    // bit 31: sampler select (0 linear / 1 point), opcode 0x59.
    VmRecordFlag_PointSampler = 0x80000000,
};

// Generic vec3 animation block (0x4c): position (base +0x70), RGB color 1
// (+0xbc), rotation (+0x134), RGB color 2 (+0x1bc). Evidence pins flags at
// base+0x40 (0xb0 for the position block, 0xfc for rgb_anim_1), so the
// timer holds four dwords.
struct Vec3InterpBlock {
    float start[3];       // +0x00
    float end[3];         // +0x0c
    float handle1[3];     // +0x18
    float handle2[3];     // +0x24
    i32 timer[4];       // +0x30
    u32 flags;          // +0x40 (bit 0 armed)
    i32 duration;       // +0x44
    i32 mode;           // +0x48
};

// Scalar animation block (0x2c): alpha 1 (base +0x108, output +0x2ff) and
// alpha 2 (+0x208, output +0x303).
struct AlphaInterpBlock {
    float start;          // +0x00
    float end;            // +0x04
    float delta_0008;     // +0x08
    float delta_000c;     // +0x0c
    i32 timer[4];       // +0x10
    u32 flags;          // +0x20 (bit 0 armed)
    i32 duration;       // +0x24
    i32 mode;           // +0x28
};

// Two-component scale animation block (0x3c, base +0x180, outputs +0x3c/+0x40).
struct ScaleInterpBlock {
    float snapshot[2];    // +0x00
    float target[2];      // +0x08
    u32 ctrl_0010;      // +0x10
    u32 ctrl_0014;      // +0x14
    u32 ctrl_0018;      // +0x18
    u32 ctrl_001c;      // +0x1c
    i32 timer[3];       // +0x20
    u32 gap_002c;       // +0x2c
    u32 flags;          // +0x30 (bit 0 armed)
    i32 duration;       // +0x34
    i32 mode;           // +0x38
};

struct VmRecord {
    i32 entity_id;              // +0x000 id/handle (FindEntityEdxStackAbi key)
    void *link_self;            // +0x004 intrusive list node {self, next, prev}
    void *link_next;            // +0x008
    void *link_prev;            // +0x00c
    void *child_head;           // +0x010 child-list sentinel (self pointer)
    void *first_child;          // +0x014
    void *parent_link;          // +0x018 container field (0 => propagate)
    void *chain_next;           // +0x01c (kind-bucket chain next; 0 = none)
    u32 render_kind;            // +0x020 (setup opcode 0x44 / render mode 15)
    float rotation_x;             // +0x024
    float rotation_y;             // +0x028
    float rotation_z;             // +0x02c
    float rotation_rate_x;        // +0x030 (setup opcodes 0x35; see note)
    float rotation_rate_y;        // +0x034
    float rotation_rate_z;        // +0x038
    float scale_x;                // +0x03c
    float scale_y;                // +0x040
    float scale_rate_x;           // +0x044 (opcode 0x36)
    float scale_rate_y;           // +0x048
    u32 width;                  // +0x04c (pair written by 0x40c960)
    u32 height;                 // +0x050
    float texture_u;              // +0x054 (UV scroll, integrated from +0x234)
    float texture_v;              // +0x058
    i32 timer_prev;             // +0x05c (script timer, poison 0xfff0bdc1)
    i32 timer_cur;              // +0x060
    float timer_accum;            // +0x064
    const float *timer_rate;      // +0x068 (-> g_FrameTimeScale 0x476f78)
    u32 timer_flags;            // +0x06c (bit 0 armed)
    Vec3InterpBlock position_anim;  // +0x070 (opcode 0x39)
    Vec3InterpBlock rgb_anim_1;     // +0x0bc (outputs +0x2fc..0x2fe)
    AlphaInterpBlock alpha_anim_1;  // +0x108 (output byte +0x2ff)
    Vec3InterpBlock rotation_anim;  // +0x134 (opcode 0x3b, outputs +0x24..2c)
    ScaleInterpBlock scale_anim;    // +0x180 (outputs +0x3c/+0x40)
    Vec3InterpBlock rgb_anim_2;     // +0x1bc (0x442050, outputs +0x300..302)
    AlphaInterpBlock alpha_anim_2;  // +0x208 (0x441f50, output byte +0x303)
    float uv_scroll_rate_x;       // +0x234 (opcode 0x46)
    float uv_scroll_rate_y;       // +0x238 (opcode 0x47)
    float base_matrix[16];        // +0x23c (identity default)
    float world_matrix[16];       // +0x27c (base x scale x rotations)
    float texture_matrix[16];     // +0x2bc (from anim entry ratios)
    u32 primary_color;          // +0x2fc (alpha in the top byte, +0x2ff)
    u32 secondary_color;        // +0x300 (alpha in the top byte, +0x303)
    u16 state_word;             // +0x304 (stop codes 1/2/3/6, kind selector)
    u16 gap0306;                // +0x306
    void *bound_resource;       // +0x308 (anm work / manager work)
    i32 reg_10000;              // +0x30c (also polyline vertex count)
    i32 reg_10001;              // +0x310 (polyline radial step)
    i32 reg_10002;              // +0x314 (payload id for scripts 0x19f/0x1a0)
    i32 reg_10003;              // +0x318
    float reg_10004;              // +0x31c
    float reg_10005;              // +0x320
    float reg_10006;              // +0x324
    float reg_10007;              // +0x328
    i32 reg_10008;              // +0x32c
    i32 reg_10009;              // +0x330
    float base_pos_x;             // +0x334 (register 10013)
    float base_pos_y;             // +0x338 (register 10014)
    float base_pos_z;             // +0x33c (register 10015)
    float delta_pos_x;            // +0x340 (published with the +224/+16 offset)
    float delta_pos_y;            // +0x344
    float delta_pos_z;            // +0x348
    float alt_pos_x;              // +0x34c (opcode 0x30 flag-0x100 path)
    float alt_pos_y;              // +0x350
    float alt_pos_z;              // +0x354
    void *vertex_buffer;        // +0x358 (ribbon 0x4b0 / polyline count*0x38)
    u32 flags;                  // +0x35c (VmRecordFlags)
    u8 gap0360[8];              // +0x360
    i32 saved_timer_prev;       // +0x368 (opcode 0x51 restore snapshot)
    i32 saved_timer_cur;        // +0x36c
    float saved_timer_accum;      // +0x370
    const float *saved_timer_rate;// +0x374
    u32 saved_timer_flags;      // +0x378
    void *saved_script;         // +0x37c (restored to +0x390)
    i32 bind_frame_stamp;       // +0x380 (= timer_cur at sprite bind)
    u16 sprite_entry_id;        // +0x384 (0xffff sentinel)
    u16 bound_file_id;          // +0x386 (= *(u16*)anm_work)
    u16 sprite_entry_copy;      // +0x388 (row-batch copy of +0x384)
    u16 bound_script_id;        // +0x38a (AssignPoolVmScript key)
    void *script_base;          // +0x38c (kind-script scan start)
    void *current_instruction;  // +0x390 (PC; liveness probe)
    void *anim_entry;           // +0x394 (0x44-byte texture-source record)
    void *frame_callback;       // +0x398 (RibbonFrameUpdateCallback 0x445620)
    void *render_callback;      // +0x39c (RibbonRenderCallback 0x445880)
    u8 anim_field_3a0;          // +0x3a0 (preset-clone 0x10)
    u8 anim_field_3a1;          // +0x3a1
    u8 gap03a2[10];             // +0x3a2
};

typedef char AssertVmRecordSize[sizeof(VmRecord) == 0x3ac ? 1 : -1];
typedef char AssertVmRecordChildHeadOffset[
    offsetof(VmRecord, child_head) == 0x10 ? 1 : -1];
typedef char AssertVmRecordRenderKindOffset[
    offsetof(VmRecord, render_kind) == 0x20 ? 1 : -1];
typedef char AssertVmRecordScaleXOffset[
    offsetof(VmRecord, scale_x) == 0x3c ? 1 : -1];
typedef char AssertVmRecordTimerPrevOffset[
    offsetof(VmRecord, timer_prev) == 0x5c ? 1 : -1];
typedef char AssertVmRecordPositionAnimOffset[
    offsetof(VmRecord, position_anim) == 0x70 ? 1 : -1];
typedef char AssertVmRecordAlphaAnim1Offset[
    offsetof(VmRecord, alpha_anim_1) == 0x108 ? 1 : -1];
typedef char AssertVmRecordRotationAnimOffset[
    offsetof(VmRecord, rotation_anim) == 0x134 ? 1 : -1];
typedef char AssertVmRecordScaleAnimOffset[
    offsetof(VmRecord, scale_anim) == 0x180 ? 1 : -1];
typedef char AssertVmRecordBaseMatrixOffset[
    offsetof(VmRecord, base_matrix) == 0x23c ? 1 : -1];
typedef char AssertVmRecordWorldMatrixOffset[
    offsetof(VmRecord, world_matrix) == 0x27c ? 1 : -1];
typedef char AssertVmRecordTextureMatrixOffset[
    offsetof(VmRecord, texture_matrix) == 0x2bc ? 1 : -1];
typedef char AssertVmRecordStateWordOffset[
    offsetof(VmRecord, state_word) == 0x304 ? 1 : -1];
typedef char AssertVmRecordBoundResourceOffset[
    offsetof(VmRecord, bound_resource) == 0x308 ? 1 : -1];
typedef char AssertVmRecordBasePosXOffset[
    offsetof(VmRecord, base_pos_x) == 0x334 ? 1 : -1];
typedef char AssertVmRecordVertexBufferOffset[
    offsetof(VmRecord, vertex_buffer) == 0x358 ? 1 : -1];
typedef char AssertVmRecordFlagsOffset[
    offsetof(VmRecord, flags) == 0x35c ? 1 : -1];
typedef char AssertVmRecordSpriteEntryIdOffset[
    offsetof(VmRecord, sprite_entry_id) == 0x384 ? 1 : -1];
typedef char AssertVmRecordCurrentInstructionOffset[
    offsetof(VmRecord, current_instruction) == 0x390 ? 1 : -1];
typedef char AssertVmRecordAnimEntryOffset[
    offsetof(VmRecord, anim_entry) == 0x394 ? 1 : -1];

// Note: +0x34/+0x38 carry a documented conflict — the ANM setup-opcode
// interpreter (0x446590 family) treats them as rotation angular velocities,
// while the "scale pair" setter 0x40c960 writes them without a dirty flag.
// One of the two labels is wrong; both call sites are preserved verbatim.

} // namespace th10
