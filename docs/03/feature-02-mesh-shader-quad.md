# Feature 3.2: Mesh Shader Hardcoded Quad

**Epic**: [Epic 3 - Core Resurfacing Pipeline](../../03/epic-03-core-resurfacing.md)
**Estimated Time**: 2-3 hours
**Prerequisites**: [Feature 3.1 - Task Shader Mapping](feature-01-task-shader-mapping.md)

## Goal

Create a mesh shader that receives the task payload and outputs a simple 2×2 quad at each element position. This validates the task→mesh data flow and establishes the geometry generation pipeline.

## What You'll Build

- Mesh shader with payload reception
- Per-vertex output structure (worldPos, normal, UV)
- Per-primitive output structure (taskId, debug info)
- 2×2 UV grid generation (9 vertices, 8 triangles)
- Proper triangle indexing and winding

## Files to Create/Modify

### Modify
- `shaders/parametric/parametric.mesh` (expand from placeholder)

## Implementation Steps

### Step 1: Update parametric.mesh

Replace the placeholder with a full quad generator:

```glsl
#version 450
#extension GL_EXT_mesh_shader : require
#extension GL_GOOGLE_include_directive : require

#include "../shaderInterface.h"
#include "../common.glsl"

// ============================================================================
// Mesh Shader Configuration
// ============================================================================

// Local work group size (32 threads for parallel vertex/primitive generation)
layout(local_size_x = 32) in;

// Output limits (for 2×2 quad: 9 vertices, 8 triangles)
layout(max_vertices = 81, max_primitives = 128, triangles) out;

// ============================================================================
// Task Payload (must match task shader)
// ============================================================================

taskPayloadSharedEXT TaskPayload {
    vec3 position;
    vec3 normal;
    float area;
    uint taskId;
    uint isVertex;
    uint elementType;
    uint padding;
} payload;

// ============================================================================
// Per-Vertex Outputs
// ============================================================================

layout(location = 0) out PerVertexData {
    vec4 worldPosU;  // xyz = world position, w = u coordinate
    vec4 normalV;    // xyz = world normal, w = v coordinate
} vOut[];

// ============================================================================
// Per-Primitive Outputs
// ============================================================================

layout(location = 2) perprimitiveEXT out PerPrimitiveData {
    uvec4 data;  // x = taskId, y = isVertex, z = elementType, w = unused
} pOut[];

// ============================================================================
// Main: Generate 2×2 Quad
// ============================================================================

void main() {
    // Hardcoded 2×2 grid for now (will be configurable in Feature 3.6)
    const uint M = 2;  // U resolution
    const uint N = 2;  // V resolution

    const uint numVerts = (M + 1) * (N + 1);  // 3 × 3 = 9 vertices
    const uint numQuads = M * N;              // 2 × 2 = 4 quads
    const uint numPrims = numQuads * 2;       // 8 triangles

    // Declare mesh output counts
    SetMeshOutputsEXT(numVerts, numPrims);

    uint localId = gl_LocalInvocationID.x;

    // ========================================================================
    // Generate Vertices
    // ========================================================================

    // Parallel vertex generation (each thread processes multiple vertices)
    for (uint i = localId; i < numVerts; i += 32) {
        // Compute grid coordinates
        uint u = i % (M + 1);
        uint v = i / (M + 1);

        // UV coordinates in [0, 1]
        vec2 uv = vec2(u, v) / vec2(M, N);

        // Generate simple quad in local XY plane
        // For now, just create a flat square (parametric surfaces in Feature 3.3)
        vec3 localPos = vec3(
            (uv.x - 0.5) * 2.0,  // x in [-1, 1]
            (uv.y - 0.5) * 2.0,  // y in [-1, 1]
            0.0                   // z = 0 (flat)
        );

        // Scale by a small factor (0.1) to make quads visible
        localPos *= 0.1;

        // Translate to payload position
        vec3 worldPos = payload.position + localPos;

        // Use payload normal as vertex normal (will be computed in Feature 3.5)
        vec3 worldNormal = payload.normal;

        // Output vertex attributes
        vOut[i].worldPosU = vec4(worldPos, uv.x);
        vOut[i].normalV = vec4(worldNormal, uv.y);

        // Set gl_Position (will use proper MVP in Feature 3.7)
        gl_MeshVerticesEXT[i].gl_Position = vec4(worldPos, 1.0);
    }

    // ========================================================================
    // Generate Primitives (Triangle Indices)
    // ========================================================================

    // Parallel primitive generation
    for (uint q = localId; q < numQuads; q += 32) {
        // Compute quad grid coordinates
        uint qu = q % M;
        uint qv = q / M;

        // Compute four corner vertex indices
        uint v00 = qv * (M + 1) + qu;      // Bottom-left
        uint v10 = v00 + 1;                // Bottom-right
        uint v01 = v00 + (M + 1);          // Top-left
        uint v11 = v01 + 1;                // Top-right

        // Split quad into two triangles (CCW winding)
        uint triId0 = 2 * q + 0;
        uint triId1 = 2 * q + 1;

        // Triangle 1: (v00, v10, v11)
        gl_PrimitiveTriangleIndicesEXT[triId0] = uvec3(v00, v10, v11);

        // Triangle 2: (v00, v11, v01)
        gl_PrimitiveTriangleIndicesEXT[triId1] = uvec3(v00, v11, v01);

        // Per-primitive data
        pOut[triId0].data = uvec4(payload.taskId, payload.isVertex, payload.elementType, 0);
        pOut[triId1].data = uvec4(payload.taskId, payload.isVertex, payload.elementType, 0);
    }
}
```

### Step 2: Update Fragment Shader

Update `shaders/parametric/parametric.frag` to read new vertex outputs:

```glsl
#version 450

// ============================================================================
// Inputs
// ============================================================================

layout(location = 0) in PerVertexData {
    vec4 worldPosU;
    vec4 normalV;
} vIn;

layout(location = 2) perprimitiveEXT in PerPrimitiveData {
    uvec4 data;
} pIn;

// ============================================================================
// Outputs
// ============================================================================

layout(location = 0) out vec4 outColor;

// ============================================================================
// Main: Simple UV Coloring
// ============================================================================

void main() {
    vec2 uv = vec2(vIn.worldPosU.w, vIn.normalV.w);

    // Color by UV coordinates for debugging
    vec3 color = vec3(uv, 0.5);

    outColor = vec4(color, 1.0);
}
```

### Step 3: Compile Shaders

```bash
cd build
cmake --build .

# Verify compilation
ls build/shaders/parametric/
# Should see: parametric.task.spv, parametric.mesh.spv, parametric.frag.spv
```

### Step 4: Create C++ Pipeline (Basic Setup)

Create `include/ParametricPipeline.h`:

```cpp
#pragma once
#include <vulkan/vulkan.h>

class ParametricPipeline {
public:
    void create(VkDevice device, VkRenderPass renderPass);
    void destroy(VkDevice device);

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

private:
    VkShaderModule loadShaderModule(VkDevice device, const std::string& filepath);
};
```

Create `src/ParametricPipeline.cpp`:

```cpp
#include "ParametricPipeline.h"
#include <fstream>
#include <vector>
#include <stdexcept>
#include <iostream>

VkShaderModule ParametricPipeline::loadShaderModule(VkDevice device, const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open shader file: " + filepath);
    }

    size_t fileSize = file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = buffer.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(buffer.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module");
    }

    return shaderModule;
}

void ParametricPipeline::create(VkDevice device, VkRenderPass renderPass) {
    // Load shader modules
    VkShaderModule taskShader = loadShaderModule(device, "build/shaders/parametric.task.spv");
    VkShaderModule meshShader = loadShaderModule(device, "build/shaders/parametric.mesh.spv");
    VkShaderModule fragShader = loadShaderModule(device, "build/shaders/parametric.frag.spv");

    // Shader stages
    VkPipelineShaderStageCreateInfo taskStage{};
    taskStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    taskStage.stage = VK_SHADER_STAGE_TASK_BIT_EXT;
    taskStage.module = taskShader;
    taskStage.pName = "main";

    VkPipelineShaderStageCreateInfo meshStage{};
    meshStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    meshStage.stage = VK_SHADER_STAGE_MESH_BIT_EXT;
    meshStage.module = meshShader;
    meshStage.pName = "main";

    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragShader;
    fragStage.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {taskStage, meshStage, fragStage};

    // Push constant range
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_TASK_BIT_EXT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(glm::mat4) + 4 * sizeof(uint32_t);  // mvp + 4 uints

    // Pipeline layout
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 0;  // Will add descriptor sets later
    layoutInfo.pSetLayouts = nullptr;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout");
    }

    // Graphics pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 3;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;

    // No vertex input (mesh shader generates vertices)
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    pipelineInfo.pVertexInputState = &vertexInputInfo;

    // Input assembly (ignored for mesh shaders, but required)
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    pipelineInfo.pInputAssemblyState = &inputAssembly;

    // Rasterization
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.lineWidth = 1.0f;
    pipelineInfo.pRasterizationState = &rasterizer;

    // Multisample
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    pipelineInfo.pMultisampleState = &multisampling;

    // Depth/stencil
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    pipelineInfo.pDepthStencilState = &depthStencil;

    // Color blend
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    pipelineInfo.pColorBlendState = &colorBlending;

    // Viewport (dynamic, will be set in command buffer)
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;
    pipelineInfo.pViewportState = &viewportState;

    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;
    pipelineInfo.pDynamicState = &dynamicState;

    // Create pipeline
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics pipeline");
    }

    // Cleanup shader modules
    vkDestroyShaderModule(device, taskShader, nullptr);
    vkDestroyShaderModule(device, meshShader, nullptr);
    vkDestroyShaderModule(device, fragShader, nullptr);

    std::cout << "✓ Parametric pipeline created" << std::endl;
}

void ParametricPipeline::destroy(VkDevice device) {
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
}
```

### Step 5: Render with Mesh Shader (Minimal Test)

In your render loop:

```cpp
// Bind pipeline
vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, parametricPipeline.pipeline);

// Set push constants
struct PushConstants {
    glm::mat4 mvp;
    uint32_t nbFaces;
    uint32_t nbVertices;
    uint32_t elementType;
    uint32_t padding;
} pushConstants;

pushConstants.mvp = projection * view * model;
pushConstants.nbFaces = halfEdgeMesh.nbFaces;
pushConstants.nbVertices = halfEdgeMesh.nbVertices;
pushConstants.elementType = 0;  // Torus (not used yet)

vkCmdPushConstants(commandBuffer, parametricPipeline.pipelineLayout,
                   VK_SHADER_STAGE_TASK_BIT_EXT, 0, sizeof(pushConstants), &pushConstants);

// Draw mesh shader
uint32_t totalTasks = pushConstants.nbFaces + pushConstants.nbVertices;
vkCmdDrawMeshTasksEXT(commandBuffer, totalTasks, 1, 1);
```

## Acceptance Criteria

- [ ] Mesh shader compiles without errors
- [ ] Small quads render at each face center and vertex position
- [ ] Quads are colored by UV (gradient from red to green)
- [ ] Total quad count = nbFaces + nbVertices
- [ ] No validation errors
- [ ] Mesh appears "fuzzy" with many small quads

## Expected Output

```
✓ Parametric pipeline created

Rendering:
  Total tasks: 14
  Total vertices: 126 (14 tasks × 9 vertices)
  Total primitives: 112 (14 tasks × 8 triangles)

Frame rendered successfully.
```

Visual: You should see many small colored quads scattered across the positions where the base mesh faces and vertices are located.

## Troubleshooting

### Nothing Renders

**Check 1**: Verify `vkCmdDrawMeshTasksEXT` is being called with correct count.

**Check 2**: Ensure task shader emits mesh tasks (`EmitMeshTasksEXT(1, 1, 1)`).

**Check 3**: Verify mesh shader calls `SetMeshOutputsEXT` before outputting vertices.

### Validation Errors

**Error**: `VUID-VkGraphicsPipelineCreateInfo-pStages-02095`

**Fix**: Ensure you're NOT including VK_SHADER_STAGE_VERTEX_BIT. Mesh shaders replace vertex shaders.

---

**Error**: `VUID-vkCmdDrawMeshTasksEXT-None-02690`

**Fix**: Ensure the pipeline is bound before calling `vkCmdDrawMeshTasksEXT`.

### Quads Are Too Small/Large

**Symptom**: Can't see quads, or they're huge.

**Fix**: Adjust the scaling factor in the mesh shader (`localPos *= 0.1;`). Try different values (0.05, 0.2, etc.).

### Quads in Wrong Positions

**Symptom**: Quads not centered on mesh elements.

**Fix**: Verify `payload.position` is correctly transferred from task shader. Print values in C++ to compare.

## Common Pitfalls

1. **Max Vertices/Primitives**: Ensure `max_vertices` and `max_primitives` are >= actual output counts. For 2×2 grid: 9 vertices, 8 triangles.

2. **Triangle Winding**: CCW is standard. Ensure vertices are ordered correctly: `(v00, v10, v11)` and `(v00, v11, v01)`.

3. **SetMeshOutputsEXT Placement**: Must be called exactly once, before any vertex/primitive output.

4. **Parallel Loop Stride**: Use `i += 32` to match `local_size_x = 32`. Otherwise, some threads won't do work.

5. **gl_Position vs vOut**: Both must be set. `gl_Position` is for rasterization, `vOut` is custom data passed to fragment shader.

## Validation Tests

### Test 1: Quad Count

For a cube (8 vertices, 6 faces):
- Total tasks: 14
- Total quads: 14
- Total vertices: 126 (14 × 9)
- Total triangles: 112 (14 × 8)

### Test 2: UV Coordinates

Fragment shader should show UV gradients:
- Bottom-left quad corner: red (u=0, v=0)
- Bottom-right: green (u=1, v=0)
- Top-left: red (u=0, v=1)
- Top-right: yellow (u=1, v=1)

## Next Steps

Next feature: **[Feature 3.3 - Parametric Torus Evaluation](feature-03-parametric-torus.md)**

You'll replace the flat quads with actual parametric torus geometry using trigonometric equations.

## Summary

You've implemented:
- ✅ Mesh shader with task payload reception
- ✅ Per-vertex and per-primitive output structures
- ✅ 2×2 UV grid generation (9 vertices, 8 triangles)
- ✅ Parallel vertex/primitive generation
- ✅ Proper triangle indexing (CCW winding)

Small quads now render at every face and vertex position, validating the complete task→mesh→fragment pipeline!
