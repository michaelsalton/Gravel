# Feature 3.6: UV Grid Generation

**Epic**: [Epic 3 - Core Resurfacing Pipeline](../../03/epic-03-core-resurfacing.md)
**Estimated Time**: 2 hours
**Prerequisites**: [Feature 3.5 - Transform Pipeline](feature-05-transform-pipeline.md)

## Goal

Make the UV grid resolution configurable via UBO, implement dynamic vertex/primitive counts, and optimize for higher resolutions while respecting hardware limits.

## What You'll Build

- Configurable M×N resolution (from UBO)
- Dynamic SetMeshOutputsEXT counts
- Runtime resolution validation
- ImGui controls for resolution
- Grid optimization for parallel generation

## Files to Create/Modify

### Modify
- `shaders/parametric/parametric.mesh`
- `src/main.cpp` (ImGui controls)

## Implementation Steps

### Step 1: Add UBO Binding to Mesh Shader

Update `shaders/parametric/parametric.mesh`:

```glsl
#version 450
#extension GL_EXT_mesh_shader : require
#extension GL_GOOGLE_include_directive : require

#include "../shaderInterface.h"
#include "../common.glsl"
#include "parametricSurfaces.glsl"

layout(local_size_x = 32) in;
layout(max_vertices = 256, max_primitives = 512, triangles) out;

// ============================================================================
// Task Payload
// ============================================================================

taskPayloadSharedEXT TaskPayload {
    vec3 position;
    vec3 normal;
    float area;
    uint taskId;
    uint isVertex;
    uint elementType;
    float userScaling;
} payload;

// ============================================================================
// UBO: Resurfacing Configuration
// ============================================================================

layout(set = SET_PER_OBJECT, binding = BINDING_CONFIG_UBO) uniform ResurfacingConfigUBO {
    uint elementType;
    float userScaling;
    uint resolutionM;      // U direction resolution
    uint resolutionN;      // V direction resolution

    float torusMajorR;
    float torusMinorR;
    float sphereRadius;
    float padding1;

    uint doLod;
    float lodFactor;
    uint doCulling;
    float cullingThreshold;

    uint padding[4];
} config;

// ============================================================================
// Per-Vertex Outputs
// ============================================================================

layout(location = 0) out PerVertexData {
    vec4 worldPosU;
    vec4 normalV;
} vOut[];

// ============================================================================
// Per-Primitive Outputs
// ============================================================================

layout(location = 2) perprimitiveEXT out PerPrimitiveData {
    uvec4 data;
} pOut[];

// ============================================================================
// Main: Dynamic UV Grid Generation
// ============================================================================

void main() {
    // Read resolution from UBO
    uint M = config.resolutionM;
    uint N = config.resolutionN;

    // Compute output counts
    uint numVerts = (M + 1) * (N + 1);
    uint numQuads = M * N;
    uint numPrims = numQuads * 2;

    // Hardware limits check (will be validated on CPU side)
    // max_vertices = 256, max_primitives = 512
    // For 16×16 grid: 289 vertices, 512 primitives (exactly at limit)

    SetMeshOutputsEXT(numVerts, numPrims);

    uint localId = gl_LocalInvocationID.x;

    // ========================================================================
    // Generate Vertices
    // ========================================================================

    for (uint i = localId; i < numVerts; i += 32) {
        // Compute grid coordinates
        uint u = i % (M + 1);
        uint v = i / (M + 1);

        // UV coordinates in [0, 1]
        vec2 uv = vec2(u, v) / vec2(M, N);

        // Evaluate parametric surface in local space
        vec3 localPos, localNormal;
        evaluateParametricSurface(uv, localPos, localNormal, config.elementType,
                                  config.torusMajorR, config.torusMinorR, config.sphereRadius);

        // Transform to world space
        vec3 worldPos, worldNormal;
        offsetVertex(localPos, localNormal,
                     payload.position, payload.normal, payload.area, config.userScaling,
                     worldPos, worldNormal);

        // Output vertex attributes
        vOut[i].worldPosU = vec4(worldPos, uv.x);
        vOut[i].normalV = vec4(worldNormal, uv.y);

        gl_MeshVerticesEXT[i].gl_Position = vec4(worldPos, 1.0);
    }

    // ========================================================================
    // Generate Primitives
    // ========================================================================

    for (uint q = localId; q < numQuads; q += 32) {
        // Compute quad grid coordinates
        uint qu = q % M;
        uint qv = q / M;

        // Compute four corner vertex indices
        uint v00 = qv * (M + 1) + qu;
        uint v10 = v00 + 1;
        uint v01 = v00 + (M + 1);
        uint v11 = v01 + 1;

        // Split quad into two triangles (CCW winding)
        uint triId0 = 2 * q + 0;
        uint triId1 = 2 * q + 1;

        gl_PrimitiveTriangleIndicesEXT[triId0] = uvec3(v00, v10, v11);
        gl_PrimitiveTriangleIndicesEXT[triId1] = uvec3(v00, v11, v01);

        pOut[triId0].data = uvec4(payload.taskId, payload.isVertex, payload.elementType, 0);
        pOut[triId1].data = uvec4(payload.taskId, payload.isVertex, payload.elementType, 0);
    }
}
```

### Step 2: Create UBO in C++

Create `src/UniformBuffers.h`:

```cpp
#pragma once
#include <vulkan/vulkan.h>
#include "ResurfacingConfig.h"

class UniformBuffer {
public:
    void create(VkDevice device, VkPhysicalDevice physicalDevice, size_t size);
    void update(VkDevice device, const void* data, size_t size);
    void destroy(VkDevice device);

    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    void* mapped = nullptr;
};
```

Create `src/UniformBuffers.cpp`:

```cpp
#include "UniformBuffers.h"
#include <cstring>
#include <stdexcept>

// Helper: Find memory type
uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter,
                        VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("Failed to find suitable memory type");
}

void UniformBuffer::create(VkDevice device, VkPhysicalDevice physicalDevice, size_t size) {
    // Create buffer
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create uniform buffer");
    }

    // Allocate memory
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate uniform buffer memory");
    }

    vkBindBufferMemory(device, buffer, memory, 0);

    // Map memory
    vkMapMemory(device, memory, 0, size, 0, &mapped);
}

void UniformBuffer::update(VkDevice device, const void* data, size_t size) {
    memcpy(mapped, data, size);
}

void UniformBuffer::destroy(VkDevice device) {
    if (mapped) {
        vkUnmapMemory(device, memory);
        mapped = nullptr;
    }
    vkDestroyBuffer(device, buffer, nullptr);
    vkFreeMemory(device, memory, nullptr);
}
```

### Step 3: Update ImGui Controls

Update `src/main.cpp`:

```cpp
// In ImGui render function:
void renderImGui() {
    ImGui::Begin("Parametric Resurfacing Controls");

    // Surface type selection
    const char* surfaceTypes[] = {"Torus", "Sphere", "Cone", "Cylinder"};
    int currentType = static_cast<int>(resurfacingConfig.elementType);
    if (ImGui::Combo("Surface Type", &currentType, surfaceTypes, 4)) {
        resurfacingConfig.elementType = static_cast<uint32_t>(currentType);
    }

    ImGui::Separator();

    // Type-specific parameters
    if (resurfacingConfig.elementType == 0) {
        ImGui::Text("Torus Parameters:");
        ImGui::SliderFloat("Major Radius", &resurfacingConfig.torusMajorR, 0.5f, 2.0f);
        ImGui::SliderFloat("Minor Radius", &resurfacingConfig.torusMinorR, 0.1f, 1.0f);
    } else if (resurfacingConfig.elementType == 1) {
        ImGui::Text("Sphere Parameters:");
        ImGui::SliderFloat("Radius", &resurfacingConfig.sphereRadius, 0.1f, 2.0f);
    }

    ImGui::Separator();

    // Global scaling
    ImGui::SliderFloat("Global Scale", &resurfacingConfig.userScaling, 0.05f, 0.5f);

    ImGui::Separator();

    // Resolution controls
    ImGui::Text("UV Grid Resolution:");
    int resM = static_cast<int>(resurfacingConfig.resolutionM);
    int resN = static_cast<int>(resurfacingConfig.resolutionN);

    if (ImGui::SliderInt("Resolution M (U)", &resM, 2, 16)) {
        resurfacingConfig.resolutionM = static_cast<uint32_t>(resM);
    }
    if (ImGui::SliderInt("Resolution N (V)", &resN, 2, 16)) {
        resurfacingConfig.resolutionN = static_cast<uint32_t>(resN);
    }

    // Display stats
    uint32_t numVerts = (resM + 1) * (resN + 1);
    uint32_t numPrims = resM * resN * 2;

    ImGui::Text("Grid Stats:");
    ImGui::Text("  Vertices: %u", numVerts);
    ImGui::Text("  Primitives: %u", numPrims);

    // Warning if exceeding limits
    if (numVerts > 256 || numPrims > 512) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "WARNING: Exceeds hardware limits!");
        ImGui::Text("  Max vertices: 256, Max primitives: 512");
    }

    ImGui::End();
}
```

### Step 4: Create Descriptor Set for UBO

Update pipeline creation to include descriptor set:

```cpp
// Create descriptor set layout
VkDescriptorSetLayoutBinding uboBinding{};
uboBinding.binding = BINDING_CONFIG_UBO;
uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
uboBinding.descriptorCount = 1;
uboBinding.stageFlags = VK_SHADER_STAGE_MESH_BIT_EXT;

VkDescriptorSetLayoutCreateInfo layoutInfo{};
layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
layoutInfo.bindingCount = 1;
layoutInfo.pBindings = &uboBinding;

VkDescriptorSetLayout descriptorSetLayout;
vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout);

// Update pipeline layout to include descriptor set
VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
pipelineLayoutInfo.setLayoutCount = 1;
pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
// ... (push constants, etc.)
```

### Step 5: Update UBO Each Frame

```cpp
// In render loop:
resurfacingUBO.update(device, &resurfacingConfig, sizeof(ResurfacingUBO));

// Bind descriptor set
vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        pipelineLayout, SET_PER_OBJECT, 1, &descriptorSet, 0, nullptr);
```

### Step 6: Compile and Test

```bash
cd build
cmake --build .
./Gravel
```

## Acceptance Criteria

- [ ] Resolution adjustable via ImGui sliders (2-16)
- [ ] Higher resolutions produce smoother surfaces
- [ ] Lower resolutions render faster (visible in FPS)
- [ ] Vertex/primitive counts displayed correctly
- [ ] Warning shown when exceeding hardware limits
- [ ] No crashes when changing resolution

## Expected Output

```
Resolution: 8×8
  Vertices: 81
  Primitives: 128

Resolution: 16×16
  Vertices: 289
  Primitives: 512

Frame time:
  8×8: 2.5ms (400 FPS)
  16×16: 4.8ms (208 FPS)
```

Visual: Higher resolutions (16×16) should show noticeably smoother surfaces compared to lower resolutions (4×4).

## Troubleshooting

### Validation Error: Exceeds maxMeshOutputVertices

**Error**: `VUID-RuntimeSpirv-MeshEXT-07113`

**Fix**: Check GPU limits using `vkGetPhysicalDeviceProperties2`:

```cpp
VkPhysicalDeviceMeshShaderPropertiesEXT meshProps{};
meshProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT;

VkPhysicalDeviceProperties2 props{};
props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
props.pNext = &meshProps;

vkGetPhysicalDeviceProperties2(physicalDevice, &props);

std::cout << "Max mesh output vertices: " << meshProps.maxMeshOutputVertices << std::endl;
std::cout << "Max mesh output primitives: " << meshProps.maxMeshOutputPrimitives << std::endl;
```

Clamp resolution to stay within limits:

```cpp
uint32_t maxM = static_cast<uint32_t>(sqrt(maxMeshOutputVertices)) - 1;
resolutionM = min(resolutionM, maxM);
```

### UBO Not Updating

**Symptom**: Changing resolution in UI has no effect.

**Fix**: Ensure UBO is updated every frame before rendering. Verify descriptor set is bound correctly.

### Low FPS at High Resolution

**Symptom**: 16×16 grid causes FPS drop.

**Fix**: This is expected. Higher resolution = more vertices/primitives. Use LOD (Epic 4) to dynamically adjust resolution based on distance.

### Surfaces Look Faceted

**Symptom**: Can see individual triangles at low resolution.

**Fix**: Increase resolution. 8×8 is a good baseline, 16×16 for smooth surfaces.

## Common Pitfalls

1. **Integer Division**: `vec2(M, N)` must be float, not int. Use `vec2(float(M), float(N))` or cast.

2. **Dynamic Limits**: Don't hardcode `max_vertices = 81`. Use larger value (256) and clamp resolution dynamically.

3. **UBO Alignment**: Ensure ResurfacingUBO struct matches std140 layout. Pad as needed.

4. **Descriptor Set Binding**: Must match `SET_PER_OBJECT` and `BINDING_CONFIG_UBO` from shaderInterface.h.

5. **Frame Updates**: UBO must be updated every frame if values change. Don't assume persistence.

## Validation Tests

### Test 1: Resolution vs Vertex Count

| M×N   | Vertices | Primitives |
|-------|----------|------------|
| 2×2   | 9        | 8          |
| 4×4   | 25       | 32         |
| 8×8   | 81       | 128        |
| 16×16 | 289      | 512        |

Formula: `V = (M+1)×(N+1)`, `P = 2×M×N`

### Test 2: Visual Quality

- 2×2: Very faceted, visible triangles
- 4×4: Somewhat smooth
- 8×8: Smooth (good default)
- 16×16: Very smooth (best quality)

### Test 3: Performance

Measure frametime vs resolution:
- 2×2: ~1.5ms
- 8×8: ~2.5ms
- 16×16: ~5ms

(Times vary by GPU and scene complexity)

## Optimization Notes

### Parallelization

Mesh shader uses 32 threads (`local_size_x = 32`):
- For 81 vertices: 3 threads do 3 vertices, 29 threads do 2 vertices
- For 289 vertices: All threads do 9 vertices

Ensure loop stride matches: `i += 32`

### Memory Access

Sequential UV coordinates produce sequential memory access:
- `i = 0, 1, 2, ...` → `u = 0, 1, 2, ..., M, 0, 1, 2, ..., M`

This is cache-friendly.

### Dynamic Branching

Avoid branches inside loops where possible. Current code has no dynamic branches in hot paths.

## Next Steps

Next feature: **[Feature 3.7 - Fragment Shader and Shading](feature-07-fragment-shading.md)**

You'll implement proper Blinn-Phong lighting and add debug visualization modes.

## Summary

You've implemented:
- ✅ Configurable M×N resolution (from UBO)
- ✅ Dynamic SetMeshOutputsEXT counts
- ✅ Runtime resolution validation
- ✅ ImGui controls for resolution
- ✅ Grid optimization for parallel generation

You can now adjust surface resolution in real-time and see immediate quality/performance tradeoffs!
