# Feature 10.2: Expand PerObject Descriptor Set

**Epic**: [Epic 10 - Textures, Skeleton & Bone Skinning](epic-10-textures-skeleton-skinning.md)
**Prerequisites**: [Feature 10.1](feature-01-image-loading-texture-infra.md)

## Goal

Grow Descriptor Set 2 (PerObject) from 1 binding to 6 bindings with partial binding support. This allows optional resources (textures, skeleton buffers) to remain unbound for non-dragon meshes without validation errors.

## What You'll Build

- Expanded descriptor set layout with bindings for UBO, skeleton SSBOs, samplers, and textures
- Partial binding support via `VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT`
- Updated shader interface with new binding defines and GLSL declarations
- Expanded ResurfacingUBO with feature flags

## New Set 2 Layout

| Binding | Type | Count | Stage | Purpose |
|---------|------|-------|-------|---------|
| 0 | UNIFORM_BUFFER | 1 | Task/Mesh/Frag | ResurfacingUBO (existing) |
| 1 | STORAGE_BUFFER | 1 | Task/Mesh | Joint indices (vec4 per vertex) |
| 2 | STORAGE_BUFFER | 1 | Task/Mesh | Joint weights (vec4 per vertex) |
| 3 | STORAGE_BUFFER | 1 | Task/Mesh | Bone matrices (mat4 per bone) |
| 4 | SAMPLER | 2 | Task/Mesh/Frag | [linear, nearest] |
| 5 | SAMPLED_IMAGE | 2 | Task/Mesh/Frag | [AO, element type map] |

## Files to Modify

- `shaders/include/shaderInterface.h` — New binding defines, GLSL declarations, ResurfacingUBO expansion
- `include/renderer/renderer.h` — CPU-side ResurfacingUBO update, new member variables
- `src/renderer/renderer_init.cpp` — Descriptor layout, pool, device features
- `src/renderer/renderer_mesh.cpp` — Descriptor set write (binding 0 only for now)
- `src/renderer/renderer.cpp` — ResurfacingUBO memcpy update

## Implementation Steps

### Step 1: Update shaderInterface.h

Add new binding defines:
```cpp
#define BINDING_SKIN_JOINTS    1
#define BINDING_SKIN_WEIGHTS   2
#define BINDING_BONE_MATRICES  3
#define BINDING_SAMPLERS       4
#define BINDING_TEXTURES       5
```

Add texture/sampler index defines:
```cpp
#define AO_TEXTURE             0
#define ELEMENT_TYPE_TEXTURE   1
#define LINEAR_SAMPLER         0
#define NEAREST_SAMPLER        1
#define SAMPLER_COUNT          2
#define TEXTURE_COUNT          2
```

Expand `ResurfacingUBO`:
```cpp
struct ResurfacingUBO {
    // ... existing 12 fields ...
    uint doSkinning;            // 0 = off, 1 = apply bone transforms
    uint hasElementTypeTexture; // 0 = use uniform elementType, 1 = sample texture
    uint hasAOTexture;          // 0 = no AO, 1 = sample AO texture
    uint padding1;
};
```

Add GLSL declarations (under `#ifndef __cplusplus`):
```glsl
// Skinning SSBOs
layout(std430, set = SET_PER_OBJECT, binding = BINDING_SKIN_JOINTS) readonly buffer SkinJointsBuffer {
    vec4 jointIndices[];
};
layout(std430, set = SET_PER_OBJECT, binding = BINDING_SKIN_WEIGHTS) readonly buffer SkinWeightsBuffer {
    vec4 jointWeights[];
};
layout(std430, set = SET_PER_OBJECT, binding = BINDING_BONE_MATRICES) readonly buffer BoneMatricesBuffer {
    mat4 boneMatrices[];
};

// Texture samplers
layout(set = SET_PER_OBJECT, binding = BINDING_SAMPLERS) uniform sampler samplers[SAMPLER_COUNT];
layout(set = SET_PER_OBJECT, binding = BINDING_TEXTURES) uniform texture2D textures[TEXTURE_COUNT];
```

Update the ResurfacingUBO GLSL block to include new fields.

### Step 2: Update CPU-side ResurfacingUBO

In `renderer.h`, add matching fields:
```cpp
struct ResurfacingUBO {
    // ... existing fields ...
    uint32_t doSkinning = 0;
    uint32_t hasElementTypeTexture = 0;
    uint32_t hasAOTexture = 0;
    uint32_t padding1 = 0;
};
```

Add member variables for textures and skeleton buffers:
```cpp
VulkanTexture aoTexture;
VulkanTexture elementTypeTexture;
bool aoTextureLoaded = false;
bool elementTypeTextureLoaded = false;

StorageBuffer jointIndicesBuffer;
StorageBuffer jointWeightsBuffer;
StorageBuffer boneMatricesBuffer;
bool skeletonLoaded = false;
```

### Step 3: Update descriptor set layout creation

In `renderer_init.cpp` `createDescriptorSetLayouts()`:
- Replace the single-binding Set 2 layout with a 6-binding layout
- Bindings 1-5 get `VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT` flags
- Add `VkDescriptorSetLayoutBindingFlagsCreateInfo` to the layout create info chain

### Step 4: Enable descriptor indexing features

In `createLogicalDevice()`:
- Chain `VkPhysicalDeviceDescriptorIndexingFeatures` into `VkDeviceCreateInfo`
- Set `descriptorBindingPartiallyBound = VK_TRUE`

### Step 5: Update descriptor pool

In `createDescriptorPool()`:
- Add pool sizes for `VK_DESCRIPTOR_TYPE_SAMPLER (2)` and `VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE (2)`
- Increase `STORAGE_BUFFER` count by 3
- Add `VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT` flag

### Step 6: Update ResurfacingUBO memcpy

In `renderer.cpp` where the UBO is updated per-frame, ensure the new fields are included.

## Acceptance Criteria

- [ ] Descriptor set layout has 6 bindings with correct types and stages
- [ ] Partial binding enabled — no validation errors when bindings 1-5 are unwritten
- [ ] ResurfacingUBO correctly sized with new flags (both CPU and GPU)
- [ ] All existing meshes render correctly with the expanded layout
- [ ] No Vulkan validation errors
