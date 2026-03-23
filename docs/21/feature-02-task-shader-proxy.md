# Feature 21.2: Task Shader Proxy Decision and Face Flagging

**Epic**: [Epic 21 - Proxy Shading](epic-21-proxy-shading.md)

## Goal

Modify the task shader to decide, per element, whether to emit geometry or flag the face for proxy shading. When an element's screen-space size falls below a threshold, skip mesh emission and write a proxy flag + blend factor to a per-face SSBO.

## Proxy Decision Logic

```glsl
float screenSize = getScreenSpaceSize(...);  // already computed for LOD

float proxyStart = resurfacingUBO.proxyStartThreshold;  // e.g., 0.015 NDC
float proxyEnd   = resurfacingUBO.proxyEndThreshold;    // e.g., 0.005 NDC

float proxyBlend = 1.0 - clamp((screenSize - proxyEnd) / (proxyStart - proxyEnd), 0.0, 1.0);
// proxyBlend = 0.0 when large (full geometry), 1.0 when tiny (full proxy)

bool useProxy = (proxyBlend >= 1.0);  // fully proxy — skip geometry entirely
bool inBlendZone = (proxyBlend > 0.0 && proxyBlend < 1.0);
```

## Per-Face Proxy Buffer

New SSBO (`BINDING_PROXY_FLAGS`, Set 1 binding 7):

```glsl
struct ProxyFaceData {
    float blend;      // 0.0 = full geometry, 1.0 = full proxy
    uint  flags;      // bit 0 = proxy active
    float pad[2];
};

layout(set = SET_HALF_EDGE, binding = BINDING_PROXY_FLAGS) buffer ProxyFlagBuffer {
    ProxyFaceData data[];
} proxyFlags;
```

## Task Shader Changes

```glsl
// Write proxy data for this face
if (resurfacingUBO.enableProxy != 0u) {
    proxyFlags.data[faceId].blend = proxyBlend;
    proxyFlags.data[faceId].flags = useProxy ? 1u : 0u;
}

// Skip geometry emission if fully proxy
if (useProxy) {
    EmitMeshTasksEXT(0, 0, 1);
    return;
}

// In blend zone: emit geometry but with reduced alpha
if (inBlendZone) {
    payload.screenAlpha = 1.0 - proxyBlend;
}
```

## Files Modified

- `shaders/parametric/parametric.task` — proxy decision logic, SSBO writes
- `shaders/include/shaderInterface.h` — `ProxyFaceData` struct, binding define, SSBO declaration
- `include/renderer/renderer.h` — proxy SSBO buffer members, UI state
- `src/renderer/renderer_init.cpp` — create proxy SSBO, add to descriptor layout + pool
- `src/renderer/renderer.cpp` — clear proxy buffer each frame (memset or vkCmdFillBuffer)

## Buffer Clear

The proxy flag buffer must be cleared to 0 each frame before the task shader runs. Use `vkCmdFillBuffer(proxyBuffer, 0, size, 0)` at the start of command buffer recording, before the render pass.

## Verification

- With proxy disabled: identical to current behavior
- With proxy enabled, zoom out: console prints "X faces proxied" from GPU readback
- Element count in stats drops as faces switch to proxy
- No visual gaps — faces that skip geometry are handled by the proxy pass (Feature 21.3)
