#pragma once

#include <stddef.h>

#include "Th10Types.hpp"

namespace th10 {

struct RenderOwnerPartial;
struct MainChainContext;

struct D3D9Device {
    void **vtable;
};

struct D3DViewport {
    u32 x;
    u32 y;
    u32 width;
    u32 height;
    float min_z;
    float max_z;
};

struct D3DVector3 {
    float x;
    float y;
    float z;
};

struct D3DMatrix {
    float values[16];
};

// Cache record at render-owner +0x3ad6e0. Deferred batch and lazy-primary
// paths establish only width/height; the remaining 20 bytes are unknown.
struct CachedRenderSurfaceDescriptor {
    u32 width;
    u32 height;
    u8 unknown_0008[0x14];
};

struct MainChainCameraWork {
    D3DVector3 translation;
    D3DVector3 eye;
    const D3DVector3 *up;
    u8 unknown_001c[0x14];
    D3DVector3 normalized_cross;
    D3DVector3 target;
    float fov;
    D3DMatrix view;
    D3DMatrix projection;
    D3DViewport viewport;
    u8 unknown_00e4[4];
    u32 owner_value_00e8;
    u32 owner_value_00ec;
    u8 unknown_00f0[0x28];
};

// Sparse view of the object held in TH10 DAT_00491c10. This callback writes
// only these fields; the object has a substantially larger, still unknown
// layout shared by rendering and manager-work systems.
struct MainChainRenderOwnerFrameState {
    u8 unknown_0000[0x5c];
    u32 camera_value_005c;
    u32 camera_value_0060;
    u8 unknown_0064[0x3ada00];
    u32 field_3ada64;
    u8 field_3ada68;
    u8 field_3ada69;
    u8 field_3ada6a;
    u8 field_3ada6b;
    u8 field_3ada6c;
    u8 unknown_3ada6d;
    u8 field_3ada6e;
    u8 unknown_3ada6f;
    u32 field_3ada70;
    u8 unknown_3ada74[0x3849e4];
    u32 field_732458;
    u32 field_73245c;
};

typedef char AssertD3DViewportSize[sizeof(D3DViewport) == 0x18 ? 1 : -1];
typedef char AssertD3DMatrixSize[sizeof(D3DMatrix) == 0x40 ? 1 : -1];
typedef char AssertCachedRenderSurfaceDescriptorSize[
    sizeof(CachedRenderSurfaceDescriptor) == 0x1c ? 1 : -1];
typedef char AssertCameraViewOffset[
    offsetof(MainChainCameraWork, view) == 0x4c ? 1 : -1];
typedef char AssertCameraProjectionOffset[
    offsetof(MainChainCameraWork, projection) == 0x8c ? 1 : -1];
typedef char AssertCameraViewportOffset[
    offsetof(MainChainCameraWork, viewport) == 0xcc ? 1 : -1];
typedef char AssertCameraWorkSize[
    sizeof(MainChainCameraWork) == 0x118 ? 1 : -1];
typedef char AssertRenderOwnerCameraValueOffset[
    offsetof(MainChainRenderOwnerFrameState, camera_value_005c) == 0x5c ? 1 : -1];
typedef char AssertRenderOwnerField3ada64Offset[
    offsetof(MainChainRenderOwnerFrameState, field_3ada64) == 0x3ada64 ? 1 : -1];
typedef char AssertRenderOwnerField3ada70Offset[
    offsetof(MainChainRenderOwnerFrameState, field_3ada70) == 0x3ada70 ? 1 : -1];
typedef char AssertRenderOwnerField732458Offset[
    offsetof(MainChainRenderOwnerFrameState, field_732458) == 0x732458 ? 1 : -1];

// `0x004215a0` requires work in EDI. These semantic functions preserve the
// recovered D3DX/D3D9 effects; the native register ABI remains a thunk task.
void UpdateMainChainCameraWorkEdiAbi(MainChainCameraWork *work);
// TH10 0x421480 semantic body. The native entry receives work in EDI and
// returns the final render-owner global value in EAX.
void *UpdateMainChainD3DFrameStateEdiAbi(MainChainCameraWork *work);
void SetD3D9Viewport(D3D9Device *device, const D3DViewport *viewport);
void ClearD3D9Target(D3D9Device *device, u32 color);
void FlushRenderOwnerPendingVertices(RenderOwnerPartial *owner);
// TH10 0x438a60. Submits the two deferred render batches selected by the
// owner sentinels. The native entry receives the owner in ESI/plain-ret.
void FlushRenderOwnerPendingRenderBatches(void *owner);
// Semantic submit paths selected by 0x438a60. Their original ABIs use mixed
// registers and stack arguments; the source-level model exposes the fields.
void SubmitLargeRenderOwnerFirstDeferredBatch(void *owner);
void SubmitLargeRenderOwnerSecondDeferredBatch(void *owner);
// TH10 0x447f70. Native entry receives ESI=cache slot and EDI=owner.
void ReleaseLargeRenderOwnerCachedSurfacePair(void *owner, u32 cache_slot);
// TH10 0x447fd0 semantic body. Native entry receives EAX=cache slot,
// EBX=owner, and four stack scalars; its return register is not stable.
void UpdateLargeRenderOwnerCachedSurfaceRegion(void *owner, u32 cache_slot,
                                               i32 source_left, i32 source_top,
                                               i32 destination_x,
                                               i32 destination_y);
// TH10 0x448120 semantic body. Native entry receives EBX=cache slot and
// six stack scalar coordinates after the owner pointer.
void UpdateLargeRenderOwnerCachedSurfaceRectangle(void *owner, u32 cache_slot,
                                                  i32 destination_x,
                                                  i32 destination_y,
                                                  i32 source_x, i32 source_y,
                                                  i32 source_width,
                                                  i32 source_height);
// TH10 0x00446220 semantic body. The native entry is stack-argument/ret-4.
void DestroyLargeRenderOwnerInPlace(void *owner);
// TH10 0x00445900 semantic body. Native entry takes one stack argument and
// returns it in EAX with ret 4.
void *ConstructLargeRenderOwner(void *owner);
// TH10 0x00438a30 semantic body. Native entry takes owner in EAX/plain ret.
void ReleaseResetSensitiveRenderSlots(void *owner);
// TH10 0x00439d20. Reinstates the fixed D3D9 state tables after device reset.
void RestoreMainChainD3DRenderStates();

// Semantic bodies for the EDI/plain-ret render-state entries at 0x420c90
// through 0x420d85. Native register ABI thunks remain separate.
i32 EnableMainChainFogIfNeeded(MainChainContext *context);
i32 DisableMainChainFogIfNeeded(MainChainContext *context);
i32 EnableMainChainZWriteIfNeeded(MainChainContext *context);
i32 DisableMainChainZWriteIfNeeded(MainChainContext *context);

// TH10 0x420d90: flushes before forwarding an arbitrary D3D render state.
i32 FlushThenSetMainChainRenderState(MainChainContext *context, u32 state,
                                     u32 value);

} // namespace th10
