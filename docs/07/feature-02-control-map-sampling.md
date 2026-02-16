# Feature 7.2: Control Map Sampling and Element Type Selection

**Epic**: [Epic 7 - Control Maps and Polish](../../07/epic-07-control-maps.md)
**Estimated Time**: 2-3 hours
**Prerequisites**: [Feature 7.1 - Control Map Loading](feature-01-control-map-loading.md)

## Goal

Sample the control map texture using face UV coordinates to determine which parametric surface type to use for each face element.

## Implementation

Update `shaders/parametric/parametric.task`:

```glsl
// Helper: Get average UV for face
vec2 getFaceAverageUV(uint faceId) {
    int faceEdge = readFaceEdge(faceId);
    if (faceEdge < 0) return vec2(0.5);

    vec2 uvSum = vec2(0);
    uint vertCount = 0;
    int currentEdge = faceEdge;

    do {
        int vertId = getHalfEdgeVertex(uint(currentEdge));
        vec2 vertUV = getVertexUV(uint(vertId));  // From half-edge data
        uvSum += vertUV;
        vertCount++;
        currentEdge = getHalfEdgeNext(uint(currentEdge));
    } while (currentEdge != faceEdge && vertCount < 16);

    return uvSum / float(vertCount);
}

// Color to element type mapping
uint colorToElementType(vec4 color) {
    vec3 rgb = color.rgb;

    // Blue → Cone (6)
    if (rgb.b > 0.7 && rgb.r < 0.3 && rgb.g < 0.3) return 6;

    // Violet → Sphere (1)
    if (rgb.b > 0.5 && rgb.r > 0.5 && rgb.g < 0.3) return 1;

    // Red → Torus (0)
    if (rgb.r > 0.7 && rgb.g < 0.3 && rgb.b < 0.3) return 0;

    // Yellow → Pebble (special flag: 255)
    if (rgb.r > 0.7 && rgb.g > 0.7 && rgb.b < 0.3) return 255;

    // Green → Empty (skip rendering: 254)
    if (rgb.g > 0.7 && rgb.r < 0.3 && rgb.b < 0.3) return 254;

    // Default: use global element type
    return 999;  // Signal to use global
}

void main() {
    // ... (existing code)

    uint elementType = push.elementType;

    // Override with control map if enabled
    if (push.useControlMap != 0) {
        vec2 faceUV = getFaceAverageUV(faceId);
        vec4 controlColor = texture(controlMapTexture, faceUV);
        uint mappedType = colorToElementType(controlColor);

        if (mappedType == 254) {
            // Green = skip rendering
            EmitMeshTasksEXT(0, 0, 0);
            return;
        } else if (mappedType == 255) {
            // Yellow = pebble (handled differently)
            // For now, treat as torus
            elementType = 0;
        } else if (mappedType != 999) {
            elementType = mappedType;
        }
    }

    payload.elementType = elementType;

    // ... (rest of task shader)
}
```

Add to push constants:

```cpp
struct PushConstants {
    // ... existing fields
    uint32_t useControlMap;  // 0 = disabled, 1 = enabled
};
```

## Acceptance Criteria

- [ ] Different mesh regions use different element types
- [ ] Blue regions → cones
- [ ] Red regions → tori
- [ ] Violet regions → spheres
- [ ] Green regions → skip (invisible)
- [ ] Control map checkbox toggles feature

## Next Steps

Next feature: **[Feature 7.3 - Base Mesh Rendering](feature-03-base-mesh-rendering.md)**
