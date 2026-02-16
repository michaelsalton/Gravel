# Feature 6.1: Pebble Pipeline Setup

**Epic**: [Epic 6 - Pebble Generation](../../06/epic-06-pebble-generation.md)
**Estimated Time**: 2 hours
**Prerequisites**: [Epic 5 Complete](../../05/epic-05-bspline-cages.md)

## Goal

Create a separate graphics pipeline for pebble rendering with its own UBO structure and UI controls for switching between parametric and pebble modes.

## What You'll Build

- PebbleUBO structure (subdivision, extrusion, roundness, noise)
- Separate pebble graphics pipeline
- Shared descriptor sets with parametric pipeline
- Mode switching UI (parametric vs pebble)
- Separate draw call for faces only

## Files to Create/Modify

### Create
- `include/PebbleConfig.h`

### Modify
- `src/renderer.cpp` (add pebble pipeline)
- `src/main.cpp` (ImGui controls)
- `shaders/shaderInterface.h` (PebbleUBO)

## Implementation Steps

### Step 1: Create PebbleUBO Structure

Update `shaders/shaderInterface.h`:

```glsl
// Pebble generation parameters
struct PebbleUBO {
    uint subdivisionLevel;    // 0-8: determines resolution (2^level)
    float extrusionAmount;    // Scale factor for extrusion (0.1-0.5)
    float roundness;          // 1.0=linear, 2.0=quadratic projection
    uint doNoise;             // 0=disable, 1=enable noise

    float noiseAmplitude;     // Displacement amplitude (0.01-0.2)
    float noiseFrequency;     // Noise frequency (1.0-20.0)
    uint noiseType;           // 0=Perlin, 1=Gabor
    float padding1;

    uint padding[4];
};
```

Create `include/PebbleConfig.h`:

```cpp
#pragma once
#include <cstdint>

struct PebbleUBO {
    uint32_t subdivisionLevel = 3;  // 8×8 default
    float extrusionAmount = 0.15f;
    float roundness = 2.0f;         // Quadratic
    uint32_t doNoise = 0;           // Off by default

    float noiseAmplitude = 0.08f;
    float noiseFrequency = 5.0f;
    uint32_t noiseType = 0;         // Perlin
    float padding1 = 0.0f;

    uint32_t padding[4] = {0, 0, 0, 0};
};
```

### Step 2: Create Pebble Pipeline

In renderer code:

```cpp
class PebblePipeline {
public:
    void create(VkDevice device, VkRenderPass renderPass,
                VkDescriptorSetLayout sceneLayout,
                VkDescriptorSetLayout halfEdgeLayout,
                VkDescriptorSetLayout perObjectLayout);
    void destroy(VkDevice device);

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
};

void PebblePipeline::create(VkDevice device, VkRenderPass renderPass,
                             VkDescriptorSetLayout sceneLayout,
                             VkDescriptorSetLayout halfEdgeLayout,
                             VkDescriptorSetLayout perObjectLayout) {
    // Load shaders (will create in next features)
    // For now, use placeholder shaders
    VkShaderModule taskShader = loadShaderModule(device, "build/shaders/pebbles/pebble.task.spv");
    VkShaderModule meshShader = loadShaderModule(device, "build/shaders/pebbles/pebble.mesh.spv");
    VkShaderModule fragShader = loadShaderModule(device, "build/shaders/pebbles/pebble.frag.spv");

    // Shader stages
    VkPipelineShaderStageCreateInfo stages[3];
    stages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stages[0].stage = VK_SHADER_STAGE_TASK_BIT_EXT;
    stages[0].module = taskShader;
    stages[0].pName = "main";

    stages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stages[1].stage = VK_SHADER_STAGE_MESH_BIT_EXT;
    stages[1].module = meshShader;
    stages[1].pName = "main";

    stages[2] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stages[2].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[2].module = fragShader;
    stages[2].pName = "main";

    // Pipeline layout (share descriptor sets with parametric)
    VkDescriptorSetLayout setLayouts[] = {sceneLayout, halfEdgeLayout, perObjectLayout};

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 3;
    layoutInfo.pSetLayouts = setLayouts;

    vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout);

    // Graphics pipeline (same setup as parametric)
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 3;
    pipelineInfo.pStages = stages;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;

    // ... (vertex input, rasterization, depth, etc. - same as parametric)

    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);

    vkDestroyShaderModule(device, taskShader, nullptr);
    vkDestroyShaderModule(device, meshShader, nullptr);
    vkDestroyShaderModule(device, fragShader, nullptr);

    std::cout << "✓ Pebble pipeline created" << std::endl;
}
```

### Step 3: Add Mode Switching

Update `src/main.cpp`:

```cpp
enum RenderMode {
    RENDER_MODE_PARAMETRIC = 0,
    RENDER_MODE_PEBBLES = 1
};

RenderMode renderMode = RENDER_MODE_PARAMETRIC;
PebbleUBO pebbleConfig;

void renderImGui() {
    ImGui::Begin("Gravel - Resurfacing Controls");

    // Mode selection
    const char* modes[] = {"Parametric Surfaces", "Pebble Generation"};
    int mode = static_cast<int>(renderMode);
    if (ImGui::Combo("Render Mode", &mode, modes, 2)) {
        renderMode = static_cast<RenderMode>(mode);
    }

    ImGui::Separator();

    if (renderMode == RENDER_MODE_PARAMETRIC) {
        // Existing parametric controls
        // ...
    } else {
        // Pebble controls
        if (ImGui::CollapsingHeader("Pebble Generation", ImGuiTreeNodeFlags_DefaultOpen)) {
            int subdivLevel = static_cast<int>(pebbleConfig.subdivisionLevel);
            ImGui::SliderInt("Subdivision Level", &subdivLevel, 0, 6);
            pebbleConfig.subdivisionLevel = static_cast<uint32_t>(subdivLevel);

            uint32_t resolution = 1 << subdivLevel;  // 2^level
            ImGui::Text("  Resolution: %u×%u", resolution, resolution);

            ImGui::SliderFloat("Extrusion", &pebbleConfig.extrusionAmount, 0.05f, 0.5f);
            ImGui::SliderFloat("Roundness", &pebbleConfig.roundness, 1.0f, 3.0f);

            ImGui::Text("Noise (not yet implemented)");
        }
    }

    ImGui::End();
}
```

### Step 4: Render with Pebble Pipeline

In render loop:

```cpp
if (renderMode == RENDER_MODE_PARAMETRIC) {
    // Existing parametric rendering
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, parametricPipeline.pipeline);
    // ...
    uint32_t totalTasks = nbFaces + nbVertices;
    vkCmdDrawMeshTasksEXT(cmd, totalTasks, 1, 1);

} else {  // RENDER_MODE_PEBBLES
    // Pebble rendering
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pebblePipeline.pipeline);

    // Bind same descriptor sets
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                           pebblePipeline.pipelineLayout, 0, 3, descriptorSets, 0, nullptr);

    // Draw: one task per FACE only (not vertices)
    vkCmdDrawMeshTasksEXT(cmd, nbFaces, 1, 1);
}
```

### Step 5: Create Placeholder Shaders

Create minimal pebble shaders that compile (we'll implement them in next features):

`shaders/pebbles/pebble.task`:
```glsl
#version 450
#extension GL_EXT_mesh_shader : require

layout(local_size_x = 1) in;

void main() {
    // Placeholder: emit nothing for now
    EmitMeshTasksEXT(0, 0, 0);
}
```

`shaders/pebbles/pebble.mesh`:
```glsl
#version 450
#extension GL_EXT_mesh_shader : require

layout(local_size_x = 1) in;
layout(max_vertices = 3, max_primitives = 1, triangles) out;

void main() {
    // Placeholder: no output
}
```

`shaders/pebbles/pebble.frag`:
```glsl
#version 450

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(1, 0, 0, 1);  // Red placeholder
}
```

## Acceptance Criteria

- [ ] Pebble pipeline created without errors
- [ ] Can switch between parametric and pebble modes
- [ ] Pebble mode doesn't crash (even with placeholder shaders)
- [ ] ImGui shows pebble controls
- [ ] Subdivision slider works (updates config)

## Expected Output

```
✓ Pebble pipeline created

ImGui:
  Render Mode: [Parametric Surfaces ▼]
               [Pebble Generation  ]

When Pebble mode selected:
  Pebble Generation:
    Subdivision Level: 3
    Resolution: 8×8
    Extrusion: 0.15
    Roundness: 2.00
    Noise (not yet implemented)
```

## Next Steps

Next feature: **[Feature 6.2 - Pebble Task Shader](feature-02-pebble-task-shader.md)**
