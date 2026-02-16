# Feature 3.7: Fragment Shader and Shading

**Epic**: [Epic 3 - Core Resurfacing Pipeline](../../03/epic-03-core-resurfacing.md)
**Estimated Time**: 2 hours
**Prerequisites**: [Feature 3.6 - UV Grid Generation](feature-06-uv-grid-generation.md)

## Goal

Implement proper Blinn-Phong shading with configurable lighting parameters, add debug visualization modes, and create a complete lighting pipeline with ambient, diffuse, and specular components.

## What You'll Build

- Blinn-Phong shading model
- GlobalShadingUBO for light parameters
- Per-element color variation
- Debug visualization modes (normals, UV, taskID)
- ImGui lighting controls

## Files to Create/Modify

### Create
- `shaders/shading.glsl` (shared lighting functions)

### Modify
- `shaders/parametric/parametric.frag`
- `src/main.cpp` (ImGui controls)
- `shaders/shaderInterface.h` (already has GlobalShadingUBO)

## Implementation Steps

### Step 1: Create shaders/shading.glsl

```glsl
#ifndef SHADING_GLSL
#define SHADING_GLSL

#include "shaderInterface.h"

// ============================================================================
// Color Utilities
// ============================================================================

/**
 * Convert HSV to RGB
 * Used for generating per-element debug colors
 *
 * @param hsv Hue [0,1], Saturation [0,1], Value [0,1]
 * @return RGB color [0,1]
 */
vec3 hsv2rgb(vec3 hsv) {
    vec3 rgb = clamp(abs(mod(hsv.x * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    return hsv.z * mix(vec3(1.0), rgb, hsv.y);
}

/**
 * Generate a unique color for a given ID
 * Uses golden ratio for good color distribution
 *
 * @param id Element ID
 * @return RGB color
 */
vec3 getDebugColor(uint id) {
    float hue = fract(float(id) * 0.618033988749895);  // Golden ratio
    return hsv2rgb(vec3(hue, 0.7, 0.9));
}

// ============================================================================
// Blinn-Phong Shading
// ============================================================================

/**
 * Compute Blinn-Phong shading
 *
 * Components:
 *   - Ambient: constant base lighting
 *   - Diffuse: lambertian reflectance (N·L)
 *   - Specular: glossy highlights (N·H)^shininess
 *
 * @param worldPos Fragment world position
 * @param normal Surface normal (unit length)
 * @param lightPos Light position in world space
 * @param cameraPos Camera position in world space
 * @param ambient Ambient color and intensity
 * @param diffuseIntensity Diffuse reflection coefficient
 * @param specularIntensity Specular reflection coefficient
 * @param shininess Specular exponent (higher = tighter highlight)
 * @return Final RGB color
 */
vec3 blinnPhong(vec3 worldPos, vec3 normal, vec3 lightPos, vec3 cameraPos,
                vec4 ambient, float diffuseIntensity, float specularIntensity, float shininess) {
    // Normalize inputs
    vec3 N = normalize(normal);
    vec3 L = normalize(lightPos - worldPos);
    vec3 V = normalize(cameraPos - worldPos);
    vec3 H = normalize(L + V);  // Half vector

    // Ambient component
    vec3 ambientColor = ambient.rgb * ambient.a;

    // Diffuse component (Lambertian)
    float NdotL = max(dot(N, L), 0.0);
    vec3 diffuseColor = vec3(diffuseIntensity) * NdotL;

    // Specular component (Blinn-Phong)
    float NdotH = max(dot(N, H), 0.0);
    float specular = pow(NdotH, shininess);
    vec3 specularColor = vec3(specularIntensity) * specular;

    // Combine components
    return ambientColor + diffuseColor + specularColor;
}

/**
 * Simplified Blinn-Phong using UBO parameters
 *
 * @param worldPos Fragment world position
 * @param normal Surface normal (unit length)
 * @param shading GlobalShadingUBO struct
 * @param cameraPos Camera position
 * @return Final RGB color
 */
vec3 blinnPhongUBO(vec3 worldPos, vec3 normal, GlobalShadingUBO shading, vec3 cameraPos) {
    return blinnPhong(worldPos, normal, shading.lightPosition.xyz, cameraPos,
                     shading.ambient, shading.diffuse, shading.specular, shading.shininess);
}

#endif // SHADING_GLSL
```

### Step 2: Update parametric.frag with Blinn-Phong

```glsl
#version 450
#extension GL_GOOGLE_include_directive : require

#include "../shaderInterface.h"
#include "../shading.glsl"

// ============================================================================
// Inputs
// ============================================================================

layout(location = 0) in PerVertexData {
    vec4 worldPosU;
    vec4 normalV;
} vIn;

layout(location = 2) perprimitiveEXT in PerPrimitiveData {
    uvec4 data;  // x = taskId, y = isVertex, z = elementType, w = unused
} pIn;

// ============================================================================
// Uniform Buffers
// ============================================================================

layout(set = SET_SCENE, binding = BINDING_VIEW_UBO) uniform ViewUBO {
    mat4 view;
    mat4 projection;
    vec4 cameraPosition;
    float nearPlane;
    float farPlane;
    float padding[2];
} viewUBO;

layout(set = SET_SCENE, binding = BINDING_SHADING_UBO) uniform GlobalShadingUBO {
    vec4 lightPosition;
    vec4 ambient;
    float diffuse;
    float specular;
    float shininess;
    float padding;
} shadingUBO;

// ============================================================================
// Push Constants (for debug modes)
// ============================================================================

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    uint nbFaces;
    uint nbVertices;
    uint elementType;
    float userScaling;
    uint debugMode;  // 0 = shading, 1 = normals, 2 = UV, 3 = taskID
} push;

// ============================================================================
// Outputs
// ============================================================================

layout(location = 0) out vec4 outColor;

// ============================================================================
// Main: Fragment Shading
// ============================================================================

void main() {
    vec3 worldPos = vIn.worldPosU.xyz;
    vec3 normal = normalize(vIn.normalV.xyz);
    vec2 uv = vec2(vIn.worldPosU.w, vIn.normalV.w);

    uint taskId = pIn.data.x;
    uint isVertex = pIn.data.y;
    uint elementType = pIn.data.z;

    vec3 color;

    // Debug mode switching
    switch (push.debugMode) {
        case 0: {
            // Standard Blinn-Phong shading
            color = blinnPhongUBO(worldPos, normal, shadingUBO, viewUBO.cameraPosition.xyz);

            // Add per-element color variation (subtle tint)
            vec3 elementColor = getDebugColor(taskId);
            color = mix(color, elementColor, 0.2);  // 20% tint
            break;
        }

        case 1: {
            // Visualize normals as RGB
            color = normal * 0.5 + 0.5;
            break;
        }

        case 2: {
            // Visualize UV coordinates
            color = vec3(uv, 0.5);
            break;
        }

        case 3: {
            // Visualize task ID (per-element colors)
            color = getDebugColor(taskId);
            break;
        }

        case 4: {
            // Visualize element type (face vs vertex)
            color = isVertex == 1 ? vec3(1, 0, 0) : vec3(0, 0, 1);
            break;
        }

        default: {
            // Fallback to shading
            color = blinnPhongUBO(worldPos, normal, shadingUBO, viewUBO.cameraPosition.xyz);
            break;
        }
    }

    outColor = vec4(color, 1.0);
}
```

### Step 3: Create GlobalShadingUBO in C++

Create `include/ShadingConfig.h`:

```cpp
#pragma once
#include <glm/glm.h>

// Must match shaderInterface.h GlobalShadingUBO
struct GlobalShadingUBO {
    glm::vec4 lightPosition;   // xyz = position, w = unused
    glm::vec4 ambient;         // rgb = ambient color, a = intensity
    float diffuse;
    float specular;
    float shininess;
    float padding;
};

// Default values
inline GlobalShadingUBO createDefaultShading() {
    return GlobalShadingUBO{
        .lightPosition = glm::vec4(5.0f, 5.0f, 5.0f, 0.0f),
        .ambient = glm::vec4(0.2f, 0.2f, 0.25f, 1.0f),  // Slight blue tint
        .diffuse = 0.7f,
        .specular = 0.5f,
        .shininess = 32.0f,
        .padding = 0.0f
    };
}
```

### Step 4: Add ImGui Lighting Controls

Update `src/main.cpp`:

```cpp
#include "ShadingConfig.h"

// Global shading config
GlobalShadingUBO shadingConfig = createDefaultShading();

void renderImGui() {
    ImGui::Begin("Parametric Resurfacing Controls");

    // ... (existing surface controls)

    ImGui::Separator();

    // Lighting controls
    if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat3("Light Position", &shadingConfig.lightPosition.x, -10.0f, 10.0f);

        ImGui::ColorEdit3("Ambient Color", &shadingConfig.ambient.r);
        ImGui::SliderFloat("Ambient Intensity", &shadingConfig.ambient.a, 0.0f, 1.0f);

        ImGui::SliderFloat("Diffuse", &shadingConfig.diffuse, 0.0f, 1.0f);
        ImGui::SliderFloat("Specular", &shadingConfig.specular, 0.0f, 1.0f);
        ImGui::SliderFloat("Shininess", &shadingConfig.shininess, 1.0f, 128.0f);
    }

    ImGui::Separator();

    // Debug modes
    if (ImGui::CollapsingHeader("Debug Visualization")) {
        const char* debugModes[] = {
            "Shading (Blinn-Phong)",
            "Normals (RGB)",
            "UV Coordinates",
            "Task ID (Per-Element)",
            "Element Type (Face/Vertex)"
        };
        int debugMode = 0;  // Store in renderer state
        ImGui::Combo("Debug Mode", &debugMode, debugModes, 5);
    }

    ImGui::End();
}
```

### Step 5: Update Descriptor Sets for Shading UBO

```cpp
// Create descriptor set layout bindings
VkDescriptorSetLayoutBinding bindings[2];

// Binding 0: ViewUBO
bindings[0].binding = BINDING_VIEW_UBO;
bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
bindings[0].descriptorCount = 1;
bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

// Binding 1: GlobalShadingUBO
bindings[1].binding = BINDING_SHADING_UBO;
bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
bindings[1].descriptorCount = 1;
bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

VkDescriptorSetLayoutCreateInfo layoutInfo{};
layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
layoutInfo.bindingCount = 2;
layoutInfo.pBindings = bindings;

VkDescriptorSetLayout sceneSetLayout;
vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &sceneSetLayout);
```

### Step 6: Update UBOs Each Frame

```cpp
// In render loop:
viewUBO.update(device, &viewData, sizeof(ViewUBO));
shadingUBO.update(device, &shadingConfig, sizeof(GlobalShadingUBO));

// Bind descriptor set
vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        pipelineLayout, SET_SCENE, 1, &sceneDescriptorSet, 0, nullptr);
```

### Step 7: Compile and Test

```bash
cd build
cmake --build .
./Gravel
```

## Acceptance Criteria

- [ ] Blinn-Phong shading renders correctly with highlights
- [ ] Light position adjustable via ImGui
- [ ] Ambient, diffuse, specular intensities adjustable
- [ ] Shininess affects highlight size
- [ ] Debug mode switches work (normals, UV, taskID)
- [ ] Per-element color variation visible
- [ ] Moving light causes highlights to move

## Expected Output

```
✓ Shading pipeline active

Lighting model: Blinn-Phong
  Ambient: (0.2, 0.2, 0.25) × 1.0
  Diffuse: 0.7
  Specular: 0.5
  Shininess: 32.0
  Light position: (5, 5, 5)

Debug modes available:
  0 - Shading (Blinn-Phong)
  1 - Normals (RGB)
  2 - UV Coordinates
  3 - Task ID (Per-Element)
  4 - Element Type (Face/Vertex)
```

Visual: Surfaces should have proper lighting with bright highlights where light hits directly, subtle ambient lighting in shadows, and smooth shading across surfaces.

## Troubleshooting

### Surfaces Are Too Dark

**Symptom**: Everything looks black or very dim.

**Fix**: Increase ambient intensity or diffuse coefficient. Check that light position is not inside geometry.

### No Specular Highlights

**Symptom**: Surfaces look flat, no glossy highlights.

**Fix**: Increase specular intensity. Ensure shininess is reasonable (32-64). Check that camera position is set correctly.

### Highlights in Wrong Places

**Symptom**: Specular highlights don't follow light movement.

**Fix**: Verify light position is updated each frame. Check that camera position is correct. Ensure normals are unit length (`normalize()`).

### Debug Modes Don't Switch

**Symptom**: Fragment shader always shows same visualization.

**Fix**: Ensure `push.debugMode` is updated in push constants. Verify push constant range includes debugMode field.

### Normals Look Inverted

**Symptom**: Dark side is lit, bright side is dark.

**Fix**: Check normal direction. Should point outward from surface. May need to flip: `normal = -normal` in parametric surface evaluation.

## Common Pitfalls

1. **Non-Normalized Normals**: Always `normalize()` normals in fragment shader. Interpolated normals are not unit length.

2. **Camera Position**: Must be in world space, not view space. Don't multiply by view matrix.

3. **Light Position**: Same as camera - world space coordinates.

4. **Shininess Range**: Values < 1.0 create very broad highlights. Values > 128 create tiny pinpoints. 16-64 is typical.

5. **Ambient Intensity**: `ambient.a` is the intensity multiplier. `ambient.rgb` is the color. Don't confuse the two.

## Validation Tests

### Test 1: Lighting Components

Turn off each component individually:
- Ambient = 0: Completely black in shadows
- Diffuse = 0: No surface shading, only highlights
- Specular = 0: No glossy highlights, matte appearance

### Test 2: Shininess Range

| Shininess | Highlight Size |
|-----------|----------------|
| 1         | Very broad     |
| 8         | Broad          |
| 32        | Medium (good default) |
| 64        | Tight          |
| 128       | Very tight     |

### Test 3: Light Distance

Move light closer/farther:
- Close (2, 2, 2): Very bright, sharp falloff
- Medium (5, 5, 5): Balanced
- Far (20, 20, 20): Dim, even lighting

### Test 4: Debug Modes

- Mode 0 (Shading): Realistic lighting
- Mode 1 (Normals): RGB gradients, no black
- Mode 2 (UV): Red-green gradient
- Mode 3 (TaskID): Distinct colors per element
- Mode 4 (Element Type): Red (vertex) or blue (face)

## Advanced Enhancements (Optional)

### 1. Multiple Lights

Add support for multiple light sources:

```glsl
vec3 color = vec3(0);
for (int i = 0; i < numLights; i++) {
    color += blinnPhong(worldPos, normal, lights[i].position, ...);
}
```

### 2. Attenuation

Add distance-based light falloff:

```glsl
float distance = length(lightPos - worldPos);
float attenuation = 1.0 / (1.0 + 0.1 * distance + 0.01 * distance * distance);
color *= attenuation;
```

### 3. Normal Mapping

Add detail normals from a texture (Epic 7).

### 4. PBR Shading

Replace Blinn-Phong with physically-based rendering (future epic).

## Next Steps

**Epic 3 Complete!**

Next epic: **[Epic 4 - Amplification and LOD](../../04/epic-04-amplification-lod.md)**

You'll implement task shader amplification for large grids, frustum/backface culling, and screen-space LOD.

## Summary

You've implemented:
- ✅ Blinn-Phong shading model (ambient + diffuse + specular)
- ✅ GlobalShadingUBO for configurable lighting
- ✅ Per-element color variation for visual debugging
- ✅ Debug visualization modes (normals, UV, taskID, element type)
- ✅ ImGui controls for all lighting parameters

The core resurfacing pipeline is now complete with full lighting and shading!

## Epic 3 Completion

Congratulations! You've completed all 7 features of Epic 3:

1. ✅ Task Shader Mapping Function (F)
2. ✅ Mesh Shader Hardcoded Quad
3. ✅ Parametric Torus Evaluation
4. ✅ Multiple Parametric Surfaces
5. ✅ Transform Pipeline (offsetVertex)
6. ✅ UV Grid Generation
7. ✅ Fragment Shader and Shading

Your application can now:
- Dispatch task shaders for each mesh element
- Generate parametric surfaces (torus, sphere, cone, cylinder)
- Transform surfaces to align with base mesh
- Render with proper Blinn-Phong lighting
- Adjust parameters in real-time via ImGui

The base mesh now has a beautiful "chain mail" or "scales" appearance with realistic lighting!
