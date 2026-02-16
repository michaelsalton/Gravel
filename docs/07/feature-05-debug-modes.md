# Feature 7.5: Debug Visualization Modes

**Epic**: [Epic 7 - Control Maps and Polish](../../07/epic-07-control-maps.md)
**Estimated Time**: 2 hours
**Prerequisites**: [Feature 7.4 - UI Organization](feature-04-ui-organization.md)

## Goal

Add comprehensive debug visualization modes to help troubleshoot and understand rendering behavior.

## Implementation

Update fragment shaders with debug modes:

```glsl
// In parametric.frag and pebble.frag

layout(push_constant) uniform PushConstants {
    // ... existing fields
    uint debugMode;  // 0=shading, 1=normals, 2=UV, 3=primID, 4=LOD
} push;

void main() {
    vec3 worldPos = vIn.worldPosU.xyz;
    vec3 normal = normalize(vIn.normalV.xyz);
    vec2 uv = vec2(vIn.worldPosU.w, vIn.normalV.w);

    vec3 color;

    switch (push.debugMode) {
        case 0: {
            // Standard Blinn-Phong shading
            color = blinnPhongUBO(worldPos, normal, shadingUBO, viewUBO.cameraPosition.xyz);
            break;
        }

        case 1: {
            // Normals as RGB
            color = normal * 0.5 + 0.5;
            break;
        }

        case 2: {
            // UV coordinates
            color = vec3(uv, 0.5);
            break;
        }

        case 3: {
            // Per-primitive ID
            uint primId = pIn.data.x;  // taskId or similar
            color = getDebugColor(primId);
            break;
        }

        case 4: {
            // LOD level visualization
            uint resolution = pIn.data.y;  // Pass resolution in per-primitive data
            if (resolution <= 4) {
                color = vec3(0, 0, 1);  // Blue = low
            } else if (resolution <= 8) {
                color = vec3(0, 1, 0);  // Green = medium
            } else if (resolution <= 16) {
                color = vec3(1, 1, 0);  // Yellow = medium-high
            } else {
                color = vec3(1, 0, 0);  // Red = high
            }
            break;
        }

        default:
            color = vec3(1, 0, 1);  // Magenta = error
            break;
    }

    outColor = vec4(color, 1.0);
}
```

Add ImGui dropdown:

```cpp
const char* debugModes[] = {
    "Standard Shading",
    "Normals (RGB)",
    "UV Coordinates",
    "Primitive ID",
    "LOD Level"
};

ImGui::Combo("Debug Mode", &debugMode, debugModes, 5);

if (debugMode > 0) {
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "Debug mode active");
}
```

## Acceptance Criteria

- [ ] Normal mode shows smooth RGB gradients
- [ ] UV mode shows red-green gradient
- [ ] Primitive ID shows per-element colors
- [ ] LOD mode shows blue (distant) to red (close)
- [ ] Dropdown switches modes correctly

## Expected Visuals

**Normal Mode**: Rainbow gradient following surface curvature
**UV Mode**: Red (u=0) to green (u=1) gradient
**Primitive ID**: Random distinct colors per surface
**LOD Mode**: Blue distant blobs, red close blobs

## Next Steps

Next feature: **[Feature 7.6 - Skeletal Animation (Optional)](feature-06-skeletal-animation.md)**

Or proceed to: **[Epic 8 - Performance Analysis](../../08/epic-08-performance-analysis.md)**
