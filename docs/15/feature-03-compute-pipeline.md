# Feature 15.3: Compute Pipeline Setup

**Epic**: [Epic 15 - Procedural Mesh Export](epic-15-procedural-mesh-export.md)
**Prerequisites**: Feature 15.2

## Goal

Create the Vulkan compute pipelines and descriptor infrastructure for mesh export. The compute shaders need to read the same half-edge and per-object data as the graphics pipeline, plus write to export output buffers.

## Files Modified

- `include/renderer/renderer.h` — add compute pipeline members
- `src/renderer/renderer_init.cpp` — pipeline creation function
- `src/renderer/renderer.cpp` — cleanup in destructor

## New Renderer Members

```cpp
// Export compute pipeline (private)
VkDescriptorSetLayout exportOutputSetLayout = VK_NULL_HANDLE;
VkPipelineLayout      computePipelineLayout = VK_NULL_HANDLE;
VkPipeline            parametricExportPipeline = VK_NULL_HANDLE;
VkPipeline            pebbleExportPipeline = VK_NULL_HANDLE;
```

## Descriptor Set 3 (Export Output)

| Binding | Type | Count | Access | Content |
|---------|------|-------|--------|---------|
| 0 | STORAGE_BUFFER | 1 | writeonly | Positions (vec4[]) |
| 1 | STORAGE_BUFFER | 1 | writeonly | Normals (vec4[]) |
| 2 | STORAGE_BUFFER | 1 | writeonly | UVs (vec2[]) |
| 3 | STORAGE_BUFFER | 1 | writeonly | Indices (uint[]) |
| 4 | STORAGE_BUFFER | 1 | readonly | Element offsets |

All bindings: `VK_SHADER_STAGE_COMPUTE_BIT`

## Compute Pipeline Layout

```cpp
// Reuses existing Sets 0-2, adds Set 3 for export
std::array<VkDescriptorSetLayout, 4> setLayouts = {
    sceneSetLayout,         // Set 0: ViewUBO, ShadingUBO
    halfEdgeSetLayout,      // Set 1: Half-edge mesh data
    perObjectSetLayout,     // Set 2: ResurfacingUBO/PebbleUBO + textures
    exportOutputSetLayout   // Set 3: Export output buffers
};

// Same push constant range as graphics pipeline
VkPushConstantRange pushRange{};
pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
pushRange.offset = 0;
pushRange.size = sizeof(PushConstants);
```

## Pipeline Creation

```cpp
void Renderer::createExportComputePipelines() {
    // 1. Create Set 3 layout
    // 2. Create compute pipeline layout with 4 descriptor sets
    // 3. Load parametric_export.comp.spv → create parametric compute pipeline
    // 4. Load pebble_export.comp.spv → create pebble compute pipeline
}
```

Follow existing pattern from `createGraphicsPipeline()` (renderer_init.cpp:1210):
- Read SPIR-V bytecode via `readFile()`
- Create shader module via `createShaderModule()`
- `VkComputePipelineCreateInfo` with single stage + layout
- `vkCreateComputePipelines()`
- Destroy shader module after pipeline creation

## Descriptor Pool & Set (On-Demand)

The export descriptor pool and set are created on-demand during export and destroyed after:

```cpp
// In exportProceduralMesh():
VkDescriptorPool exportPool;  // 5 SSBOs, 1 set
VkDescriptorSet exportSet;
// ... allocate, write, use, destroy ...
```

This avoids pre-allocating pool space that's rarely used.

## Acceptance Criteria

- [ ] Compute pipeline layout created with 4 descriptor set layouts
- [ ] Both compute pipelines compile and create without errors
- [ ] Descriptor set 3 can be allocated and written with export buffer handles
- [ ] No Vulkan validation layer errors
- [ ] Pipeline handles destroyed properly in Renderer destructor
