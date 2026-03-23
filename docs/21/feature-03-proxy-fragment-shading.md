# Feature 21.3: Base Mesh Proxy Fragment Shading

**Epic**: [Epic 21 - Proxy Shading](epic-21-proxy-shading.md)

## Goal

Modify the base mesh fragment shader to read per-face proxy flags and, when active, shade the face using the precomputed aggregate PBR parameters instead of the default base mesh material. This makes proxied faces look like they're covered by the procedural elements without any geometry being generated.

## Approach

The base mesh is always rendered (in solid mode, triggered automatically when proxy is active). For faces flagged as proxy, the fragment shader:

1. Reads `proxyFlags.data[faceId]` to get blend factor
2. Reads `elementProxyParams[elementType]` to get aggregate PBR params
3. Computes PBR shading with:
   - `roughness = aggregateRoughness` (wider than material roughness due to normal spread)
   - `albedo = baseColor * selfShadowScale * coverageFraction + backgroundColor * (1 - coverageFraction)`
   - Normal slightly perturbed by `meanNormalTilt` (optional, for directional appearance)

## Base Mesh Fragment Shader Changes

```glsl
// New inputs
layout(set = SET_HALF_EDGE, binding = BINDING_PROXY_FLAGS) readonly buffer ProxyFlagBuffer {
    ProxyFaceData data[];
} proxyFlags;

// Proxy params UBO (small, per element type)
layout(set = SET_PER_OBJECT, binding = BINDING_PROXY_PARAMS) uniform ProxyParamsBlock {
    ElementProxyParams params[10];  // one per element type
} proxyParams;

void main() {
    // ... existing base mesh shading ...

    // Check if this face should be proxy-shaded
    ProxyFaceData pd = proxyFlags.data[inFaceId];
    if (pd.flags != 0u || pd.blend > 0.0) {
        ElementProxyParams pp = proxyParams.params[elementType];

        // Proxy PBR: aggregate roughness captures element normal distribution
        vec3 proxyColor = cookTorrancePBR(
            inWorldPos, N,
            lightPos, cameraPos,
            matBaseColor * pp.selfShadowScale,
            pp.aggregateRoughness,
            matMetallic,
            matF0,
            ambient,
            matEnvRefl,
            lightIntensity
        );

        // Blend between base mesh color and proxy color
        color = mix(color, proxyColor, pd.blend);
    }
}
```

## Auto-Enable Base Mesh for Proxy Faces

When proxy shading is active, the base mesh must be visible for proxied faces. Options:
1. Force `baseMeshMode = 2` (solid) when proxy is enabled — simplest
2. Render a separate proxy-only pass that only draws faces with `proxyFlag == 1`

Option 1 is simpler. The base mesh solid pipeline already renders all faces; the fragment shader just needs to switch materials for proxied faces.

## Files Modified

- `shaders/basemesh/basemesh.frag` — proxy flag check, aggregate PBR shading
- `shaders/include/shaderInterface.h` — `ElementProxyParams` GLSL struct
- `src/renderer/renderer_init.cpp` — bind proxy SSBO + params UBO to base mesh descriptor sets
- `src/renderer/renderer.cpp` — auto-enable base mesh solid when proxy is active

## Verification

- Proxied faces at distance should have a smooth, matte appearance matching the element color
- Transition zone should blend seamlessly between geometry and proxy
- Toggling proxy on/off should show obvious difference at distance (proxy = smooth, no proxy = aliased)
- Color should match the procedural element material, not the base mesh material
