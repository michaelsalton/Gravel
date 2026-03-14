# Feature 17.2: Shader Interface Extensions

**Epic**: [Epic 17 - Dragon Scale](epic-17-dragon-scale.md)

## Goal

Extend `shaderInterface.h` with the new binding constant, LUT fields in `ResurfacingUBO`, and the SSBO accessor. Add `parametricDragonScale()` to `parametricSurfaces.glsl`. Add per-element orientation twist in `parametric.task`.

## Files Modified

- `shaders/include/shaderInterface.h`
- `shaders/parametric/parametricSurfaces.glsl`
- `shaders/parametric/parametric.task`

---

## shaderInterface.h Changes

### New Binding Constant

Add after `BINDING_RESURFACING_UBO`:

```glsl
#define BINDING_SCALE_LUT       6
```

### New Element Type Constant

Add after `ELEMENT_HEMISPHERE`:

```glsl
#define ELEMENT_DRAGON_SCALE    5u
```

### Extend ResurfacingUBO Struct

Append to the existing `ResurfacingUBO` block (after `hasMaskTexture` at offset 60):

```glsl
// Dragon scale LUT — std140 offsets 64–111
uint  Nx;                  // LUT grid width  (offset 64)
uint  Ny;                  // LUT grid height (offset 68)
float normalPerturbation;  // per-element random twist [0,1] (offset 72)
float pad_lut;             // padding (offset 76)
vec4  minLutExtent;        // LUT AABB min (offset 80)
vec4  maxLutExtent;        // LUT AABB max (offset 96)
```

### ScaleLutBuffer SSBO Declaration

Add after the `ResurfacingUBO` block (inside the `#ifndef CPP_SHADER_INTERFACE` guard):

```glsl
layout(set = SET_PER_OBJECT, binding = BINDING_SCALE_LUT, std430)
    readonly buffer ScaleLutBuffer { vec4 lutVertices[]; };

vec3 getScaleLutPoint(uvec2 idx, uvec2 gridSize) {
    return lutVertices[idx.y * gridSize.x + idx.x].xyz;
}
```

---

## parametricSurfaces.glsl Changes

### Add Include

At top of file, after existing includes:

```glsl
#include "bspline.glsl"
```

### Add parametricDragonScale() Function

```glsl
void parametricDragonScale(vec2 uv, out vec3 pos, out vec3 normal) {
    uvec2 gridSize   = uvec2(resurfacingUBO.Nx, resurfacingUBO.Ny);
    uint numPatchesU = (gridSize.x - 1u) / 3u;
    uint numPatchesV = (gridSize.y - 1u) / 3u;

    float pU = uv.x * float(numPatchesU);
    float pV = uv.y * float(numPatchesV);
    uint  pu = min(uint(pU), numPatchesU - 1u);
    uint  pv = min(uint(pV), numPatchesV - 1u);
    vec2  localUV = vec2(pU - float(pu), pV - float(pv));

    // Fetch 4×4 control points
    vec3 P[4][4];
    for (int j = 0; j < 4; j++)
        for (int i = 0; i < 4; i++)
            P[i][j] = getScaleLutPoint(uvec2(pu * 3u + uint(i), pv * 3u + uint(j)), gridSize);

    pos = evaluateBSplinePatch(localUV, P);

    // Normalise into unit space using precomputed LUT bounding box
    vec3 extentMin = resurfacingUBO.minLutExtent.xyz;
    vec3 extentMax = resurfacingUBO.maxLutExtent.xyz;
    vec3 center    = (extentMin + extentMax) * 0.5;
    float scale    = max(max(extentMax.x - extentMin.x,
                             extentMax.y - extentMin.y),
                             extentMax.z - extentMin.z) * 0.5;
    pos = (pos - center) / max(scale, 0.0001);

    normal = computeFiniteDifferenceBspline(localUV, P);
}
```

### Add Case to evaluateParametricSurface()

Inside the existing `switch (elementType)`:

```glsl
case ELEMENT_DRAGON_SCALE:
    parametricDragonScale(uv, pos, normal);
    break;
```

---

## parametric.task Changes

After the skinning block (post-skinning `payload.normal` and `payload.edgeTangent` are set), insert the normalPerturbation twist for dragon scale elements:

```glsl
// Per-element orientation twist for dragon scale
if (payload.elementType == ELEMENT_DRAGON_SCALE && resurfacingUBO.normalPerturbation > 0.0) {
    float angle = fract(float(globalId) * 0.618033988749895)
                  * 2.0 * PI * resurfacingUBO.normalPerturbation;
    float c = cos(angle), s = sin(angle);
    vec3 n = payload.normal;
    payload.edgeTangent = payload.edgeTangent * c
        + cross(n, payload.edgeTangent) * s
        + n * dot(n, payload.edgeTangent) * (1.0 - c);
}
```

This rotates `edgeTangent` around `normal` using Rodrigues' rotation formula. The golden-ratio hash ensures visually uniform pseudorandom distribution with no two adjacent elements having the same angle.

## Notes

- `PI` is already defined in `shaderInterface.h`; no new define needed
- The twist is applied before `payload.edgeTangent` is consumed by `alignRotationToVector()` in the mesh shader, so it correctly propagates to the final orientation
- Inserting dragon scale into the existing switch requires no changes to other element types
