# Feature 6.6: Pebble LOD and Fragment Shader

**Epic**: [Epic 6 - Pebble Generation](../../06/epic-06-pebble-generation.md)
**Estimated Time**: 2 hours
**Prerequisites**: [Feature 6.5 - Procedural Noise](feature-05-procedural-noise.md)

## Goal

Add LOD support for pebbles and implement fragment shader with proper shading.

## Implementation

### LOD in Task Shader

Update `pebble.task`:

```glsl
// Compute screen-space size from extruded bounds
float boundingRadius = computeBoundingRadius(payload.area, push.extrusionAmount, 2.0);
float screenSize = getScreenSpaceSize(faceCenter, faceNormal, boundingRadius, push.mvp);

// Map screen size to subdivision level (0-6)
uint subdivLevel;
if (screenSize < 0.1) {
    subdivLevel = 1;  // 2×2
} else if (screenSize < 0.3) {
    subdivLevel = 2;  // 4×4
} else if (screenSize < 0.6) {
    subdivLevel = 3;  // 8×8
} else {
    subdivLevel = 4;  // 16×16
}

payload.subdivisionLevel = min(subdivLevel, push.maxSubdivisionLevel);
```

### Fragment Shader

Create `shaders/pebbles/pebble.frag`:

```glsl
#version 450
#extension GL_GOOGLE_include_directive : require

#include "../shaderInterface.h"
#include "../shading.glsl"

layout(location = 0) in PerVertexData {
    vec4 worldPosU;
    vec4 normalV;
} vIn;

layout(location = 0) out vec4 outColor;

layout(set = SET_SCENE, binding = BINDING_VIEW_UBO) uniform ViewUBO {
    mat4 view;
    mat4 projection;
    vec4 cameraPosition;
    float nearPlane;
    float farPlane;
} viewUBO;

layout(set = SET_SCENE, binding = BINDING_SHADING_UBO) uniform GlobalShadingUBO {
    vec4 lightPosition;
    vec4 ambient;
    float diffuse;
    float specular;
    float shininess;
} shadingUBO;

void main() {
    vec3 worldPos = vIn.worldPosU.xyz;
    vec3 normal = normalize(vIn.normalV.xyz);

    // Blinn-Phong shading
    vec3 color = blinnPhongUBO(worldPos, normal, shadingUBO, viewUBO.cameraPosition.xyz);

    outColor = vec4(color, 1.0);
}
```

## Acceptance Criteria

- [ ] LOD reduces subdivision for distant pebbles
- [ ] FPS improves with LOD
- [ ] Shading matches parametric quality
- [ ] All controls functional

## Expected Performance

| Subdivision | FPS (1000 faces) |
|-------------|------------------|
| Level 1 (2×2) | 200+ |
| Level 2 (4×4) | 150+ |
| Level 3 (8×8) | 100+ |
| Level 4 (16×16) | 50+ |

## Summary

**Epic 6 Complete!**

You've implemented:
- ✅ Pebble pipeline infrastructure
- ✅ Task shader dispatch per face
- ✅ Procedural control cage construction
- ✅ B-spline surface evaluation
- ✅ Procedural noise displacement
- ✅ LOD and fragment shading

Pebbles now render as smooth, rounded, rock-like surfaces with organic variation!

## Next Epic

Next epic: **[Epic 7 - Control Maps and Polish](../../07/epic-07-control-maps.md)**
