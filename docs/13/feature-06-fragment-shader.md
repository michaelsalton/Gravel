# Feature 13.6: Fragment Shader

**Epic**: [Epic 13 - Pebble Generation](epic-13-pebble-generation.md)
**Estimated Time**: 1-2 hours
**Prerequisites**: [Feature 13.1](feature-01-pebble-ubo-pipeline.md)

## Goal

Create the pebble fragment shader with Phong shading, per-face random color variation, AO texture support, and debug visualization modes.

## What You'll Build

- `pebble.frag` fragment shader that reuses existing shading infrastructure
- Per-face random color variation for organic appearance
- AO texture sampling from base mesh UV
- Debug visualization (normals, UV, primitive ID)

## Files Created

- `shaders/pebbles/pebble.frag` -- Pebble fragment shader (replaces placeholder)

## Implementation Details

### Fragment Shader Structure

```glsl
#version 460

#define FRAGMENT_SHADER
#define PEBBLE_PIPELINE

#include "../include/shaderInterface.h"
#include "../include/noise.glsl"
#include "../include/shading.glsl"
```

The `PEBBLE_PIPELINE` define ensures the PebbleUBO is available. The `FRAGMENT_SHADER` define can be used to conditionally include fragment-only code.

### Per-Vertex Input

The mesh shader outputs per-vertex data:
```glsl
layout(location = 0) in vec4 worldPosU;  // xyz = world position, w = local U
layout(location = 1) in vec4 normalV;    // xyz = normal, w = local V
```

And per-primitive data (flat):
```glsl
layout(location = 2) flat in uvec4 primData;  // x = faceID, yzw = reserved
```

### Base UV Lookup

To sample the AO texture, we need the base mesh UV for this face:
```glsl
vec2 getBaseUv(uint faceId) {
    uint vertId = uint(getHalfEdgeVertex(uint(getFaceEdge(faceId))));
    return getVertexTexCoord(vertId);
}
```

### Shading

Reuse the existing `shading()` function from `shading.glsl`:
```glsl
void main() {
    vec3 worldPos = worldPosU.xyz;
    vec3 normal = normalize(normalV.xyz);
    vec2 localUV = vec2(worldPosU.w, normalV.w);

    uint faceId = primData.x;
    vec2 baseUV = getBaseUv(faceId);
    baseUV.y = 1.0 - baseUV.y;  // flip V

    // Phong shading
    outColor = vec4(shading(worldPos, normal, localUV, baseUV), 1.0);

    // Per-face random color variation for organic look
    seed = faceId;
    outColor *= rand(0.5, 1.0);
}
```

The `rand(0.5, 1.0)` multiplication gives each face a slightly different brightness, creating a natural cobblestone appearance.

### Debug Visualization Modes

Support the existing debug mode system via push constants:
```glsl
if (debugMode == 1) {
    outColor = vec4(normal * 0.5 + 0.5, 1.0);  // Normal visualization
} else if (debugMode == 2) {
    outColor = vec4(localUV, 0.0, 1.0);  // UV visualization
} else if (debugMode == 3) {
    // Per-primitive ID coloring
    seed = faceId;
    outColor = vec4(rand(), rand(), rand(), 1.0);
}
```

### AO Texture Integration

If the AO texture is loaded and enabled:
```glsl
if (hasAOTexture != 0) {
    float ao = texture(sampler2D(textures[AO_TEXTURE], samplers[LINEAR_SAMPLER]), baseUV).r;
    outColor.rgb *= ao;
}
```

## Acceptance Criteria

- [ ] Pebbles render with proper Phong lighting
- [ ] Per-face color variation creates organic appearance
- [ ] AO texture applies correctly when loaded
- [ ] Debug modes (normals, UV, primID) work
- [ ] No visual artifacts at patch boundaries

## Common Pitfalls

- UV flip (`baseUV.y = 1.0 - baseUV.y`) may need to be adjusted depending on texture format
- `seed` must be set before calling `rand()` to get deterministic per-face colors
- Normal should be renormalized after interpolation across the triangle

## Next Steps

**[Feature 13.7 - LOD System](feature-07-lod-system.md)**
