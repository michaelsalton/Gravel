# Feature 13.1: PebbleUBO & Separate Pipeline

**Epic**: [Epic 13 - Pebble Generation](epic-13-pebble-generation.md)
**Estimated Time**: 3-4 hours
**Prerequisites**: Epic 3 (Core Resurfacing Pipeline)

## Goal

Create the PebbleUBO structure, a separate Vulkan graphics pipeline for pebble rendering, and pipeline switching infrastructure in the renderer. After this task, the application can toggle between parametric and pebble modes without crashing.

## What You'll Build

- `PebbleUBO` struct in shaderInterface.h (shared CPU/GPU definition)
- C++ mirror struct in renderer.h
- Separate Vulkan graphics pipeline loading pebble task/mesh/fragment shaders
- Pebble-specific descriptor set with PebbleUBO bound at BINDING_CONFIG_UBO
- Pipeline switching in the draw path
- Placeholder pebble shaders that compile and produce no output

## Files Modified

- `shaders/include/shaderInterface.h` -- Add PebbleUBO struct (conditional on `PEBBLE_PIPELINE` define)
- `include/renderer/renderer.h` -- Add pebble pipeline handle, UBO buffer, descriptor set, state vars
- `src/renderer/renderer_init.cpp` -- Create pebble pipeline in `createGraphicsPipeline()`
- `src/renderer/renderer.cpp` -- Add pebble draw path in `recordCommandBuffer()`, UBO update per frame

## Files Created

- `shaders/pebbles/pebble.task` -- Placeholder task shader
- `shaders/pebbles/pebble.mesh` -- Placeholder mesh shader
- `shaders/pebbles/pebble.frag` -- Placeholder fragment shader

## Implementation Details

### Step 1: PebbleUBO in shaderInterface.h

Add a conditional block after the existing ResurfacingUBO GLSL declaration. The PebbleUBO shares the same binding slot (`BINDING_CONFIG_UBO`) but is used by a different pipeline:

```glsl
#ifdef PEBBLE_PIPELINE
LAYOUT_STD140(SET_PER_OBJECT, BINDING_CONFIG_UBO) uniform PebbleUBOBlock {
    uint  subdivisionLevel;
    uint  subdivOffset;
    float extrusionAmount;
    float extrusionVariation;
    float roundness;
    uint  normalCalculationMethod;
    float fillradius;
    float ringoffset;
    uint  useLod;
    float lodFactor;
    uint  allowLowLod;
    uint  boundingBoxType;
    uint  useCulling;
    float cullingThreshold;
    float time;
    uint  enableRotation;
    float rotationSpeed;
    float scalingThreshold;
    uint  doNoise;
    float noiseAmplitude;
    float noiseFrequency;
    float normalOffset;
    float padding0;
} pebbleUbo;
#endif
```

### Step 2: C++ PebbleUBO in renderer.h

```cpp
struct PebbleUBO {
    uint32_t subdivisionLevel = 3;
    uint32_t subdivOffset = 0;
    float    extrusionAmount = 0.1f;
    float    extrusionVariation = 0.5f;
    float    roundness = 2.0f;
    uint32_t normalCalculationMethod = 1;
    float    fillradius = 0.0f;
    float    ringoffset = 0.3f;
    uint32_t useLod = 0;
    float    lodFactor = 1.0f;
    uint32_t allowLowLod = 0;
    uint32_t boundingBoxType = 0;
    uint32_t useCulling = 0;
    float    cullingThreshold = 0.1f;
    float    time = 0.0f;
    uint32_t enableRotation = 0;
    float    rotationSpeed = 0.1f;
    float    scalingThreshold = 0.1f;
    uint32_t doNoise = 0;
    float    noiseAmplitude = 0.01f;
    float    noiseFrequency = 50.0f;
    float    normalOffset = 0.2f;
    float    padding0 = 0.0f;
};
```

Add to the Renderer class:
```cpp
// Pebble pipeline
VkPipeline pebblePipeline = VK_NULL_HANDLE;
VkBuffer pebbleUBOBuffer = VK_NULL_HANDLE;
VkDeviceMemory pebbleUBOMemory = VK_NULL_HANDLE;
void* pebbleUBOMapped = nullptr;
VkDescriptorSet pebblePerObjectDescriptorSet = VK_NULL_HANDLE;
bool renderPebbles = false;
PebbleUBO pebbleUBO;
```

### Step 3: Pipeline Creation

In `createGraphicsPipeline()`, after the existing parametric and base mesh pipelines:

```cpp
// --- Pebble pipeline ---
auto pebbleTaskCode = readFile(std::string(SHADER_DIR) + "pebble.task.spv");
auto pebbleMeshCode = readFile(std::string(SHADER_DIR) + "pebble.mesh.spv");
auto pebbleFragCode = readFile(std::string(SHADER_DIR) + "pebble.frag.spv");

// Create shader modules, stages, and pipeline using the same pattern
// as the parametric pipeline (reuse pipelineLayout, renderPass, etc.)
```

The pebble pipeline reuses the same pipeline layout (same descriptor set layouts and push constant range) as the parametric pipeline.

### Step 4: Pebble Descriptor Set

Create a separate descriptor set for the pebble pipeline's per-object data. This uses the same layout as `perObjectDescriptorSet` but writes the PebbleUBO buffer to BINDING_CONFIG_UBO instead of ResurfacingUBO.

### Step 5: Draw Path

In `recordCommandBuffer()`:
```cpp
if (renderPebbles) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pebblePipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                             pipelineLayout, SET_PER_OBJECT, 1,
                             &pebblePerObjectDescriptorSet, 0, nullptr);
    pfnCmdDrawMeshTasksEXT(cmd, heNbFaces, 1, 1);  // faces only
} else if (renderResurfacing) {
    // existing parametric draw path
}
```

### Step 6: Placeholder Shaders

Create minimal shaders in `shaders/pebbles/` that compile:
- `pebble.task`: emit 0 mesh tasks
- `pebble.mesh`: empty with correct layout declarations
- `pebble.frag`: output solid red

## Acceptance Criteria

- [ ] PebbleUBO struct defined in both GLSL and C++
- [ ] Pebble pipeline created without Vulkan errors
- [ ] Pebble descriptor set correctly binds PebbleUBO
- [ ] Can toggle between parametric and pebble modes
- [ ] Pebble mode doesn't crash (stub shaders emit nothing)
- [ ] All placeholder shaders compile to .spv

## Next Steps

**[Feature 13.2 - Pebble Helper & Task Shader](feature-02-pebble-helper-task-shader.md)**
