# Feature 1.10: Test Mesh Shader Pipeline

**Epic**: [Epic 1 - Vulkan Infrastructure](epic-01-vulkan-infrastructure.md)
**Estimated Time**: 3-4 hours
**Prerequisites**: [Feature 1.9 - Descriptor Set Layouts](feature-09-descriptor-sets.md)

## Goal

Create a complete mesh shader graphics pipeline and render a hardcoded triangle using task + mesh + fragment shaders. This validates the entire mesh shader pipeline from shader compilation through draw call.

## What You'll Build

- Test task shader (`test.task`) that emits 1 mesh shader workgroup
- Test mesh shader (`test.mesh`) that outputs a hardcoded triangle
- Test fragment shader (`test.frag`) that outputs a solid color
- Shader module loading utility
- Mesh shader graphics pipeline creation
- `vkCmdDrawMeshTasksEXT` function pointer loading and invocation

## Files to Create/Modify

### Create
- `shaders/test.task`
- `shaders/test.mesh`
- `shaders/test.frag`

### Modify
- `include/renderer.h`
- `src/renderer.cpp`
- `CMakeLists.txt` (enable shader compilation)

## Implementation Steps

### Step 1: Create shaders/test.task

```glsl
#version 460
#extension GL_EXT_mesh_shader : require

layout(local_size_x = 1) in;

// Task payload passed to mesh shader
struct TaskPayload {
    uint meshletIndex;
};

taskPayloadSharedEXT TaskPayload payload;

void main() {
    // Emit one mesh shader workgroup
    payload.meshletIndex = gl_WorkGroupID.x;
    EmitMeshTasksEXT(1, 1, 1);
}
```

### Step 2: Create shaders/test.mesh

```glsl
#version 460
#extension GL_EXT_mesh_shader : require

layout(local_size_x = 1) in;
layout(triangles, max_vertices = 3, max_primitives = 1) out;

// Output to fragment shader
layout(location = 0) out vec3 fragColor[];

// Task payload from task shader
struct TaskPayload {
    uint meshletIndex;
};

taskPayloadSharedEXT TaskPayload payload;

// Push constants
layout(push_constant) uniform PushConstants {
    mat4 model;
} push;

// Scene UBO
layout(set = 0, binding = 0) uniform ViewUBO {
    mat4 view;
    mat4 projection;
    vec4 cameraPosition;
    float nearPlane;
    float farPlane;
} viewUBO;

void main() {
    // Set output counts
    SetMeshOutputsEXT(3, 1);

    // Hardcoded triangle vertices (NDC)
    vec3 positions[3] = vec3[](
        vec3( 0.0, -0.5, 0.0),
        vec3( 0.5,  0.5, 0.0),
        vec3(-0.5,  0.5, 0.0)
    );

    vec3 colors[3] = vec3[](
        vec3(1.0, 0.0, 0.0),  // Red
        vec3(0.0, 1.0, 0.0),  // Green
        vec3(0.0, 0.0, 1.0)   // Blue
    );

    // Transform and output vertices
    mat4 mvp = viewUBO.projection * viewUBO.view * push.model;

    for (uint i = 0; i < 3; i++) {
        gl_MeshVerticesEXT[i].gl_Position = mvp * vec4(positions[i], 1.0);
        fragColor[i] = colors[i];
    }

    // Output triangle primitive
    gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0, 1, 2);
}
```

### Step 3: Create shaders/test.frag

```glsl
#version 460

layout(location = 0) in vec3 fragColor;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(fragColor, 1.0);
}
```

### Step 4: Enable Shader Compilation in CMakeLists.txt

Uncomment the shader compilation section:

```cmake
# Shader compilation targets
file(GLOB_RECURSE SHADER_SOURCES
    ${CMAKE_SOURCE_DIR}/shaders/*.vert
    ${CMAKE_SOURCE_DIR}/shaders/*.frag
    ${CMAKE_SOURCE_DIR}/shaders/*.task
    ${CMAKE_SOURCE_DIR}/shaders/*.mesh
)

foreach(SHADER ${SHADER_SOURCES})
    compile_shader(${PROJECT_NAME} ${SHADER})
endforeach()

add_custom_target(shaders ALL DEPENDS ${SHADER_OUTPUTS})
add_dependencies(${PROJECT_NAME} shaders)
```

### Step 5: Add Pipeline Members to include/renderer.h

```cpp
// Add to Renderer class private section:

    void createGraphicsPipeline();
    VkShaderModule createShaderModule(const std::vector<char>& code);
    static std::vector<char> readFile(const std::string& filename);

    // Load mesh shader draw function pointer
    void loadMeshShaderFunctions();

    // Graphics pipeline
    VkPipeline graphicsPipeline = VK_NULL_HANDLE;

    // Mesh shader function pointer
    PFN_vkCmdDrawMeshTasksEXT pfnCmdDrawMeshTasksEXT = nullptr;
```

### Step 6: Implement Shader Loading

```cpp
std::vector<char> Renderer::readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filename);
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

VkShaderModule Renderer::createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module!");
    }

    return shaderModule;
}
```

### Step 7: Load Mesh Shader Function Pointer

```cpp
void Renderer::loadMeshShaderFunctions() {
    pfnCmdDrawMeshTasksEXT = (PFN_vkCmdDrawMeshTasksEXT)
        vkGetDeviceProcAddr(device, "vkCmdDrawMeshTasksEXT");

    if (pfnCmdDrawMeshTasksEXT == nullptr) {
        throw std::runtime_error("Failed to load vkCmdDrawMeshTasksEXT!");
    }

    std::cout << "Mesh shader draw function loaded" << std::endl;
}
```

### Step 8: Create Graphics Pipeline

```cpp
void Renderer::createGraphicsPipeline() {
    // Load shader modules
    auto taskCode = readFile("shaders/test.task.spv");
    auto meshCode = readFile("shaders/test.mesh.spv");
    auto fragCode = readFile("shaders/test.frag.spv");

    VkShaderModule taskModule = createShaderModule(taskCode);
    VkShaderModule meshModule = createShaderModule(meshCode);
    VkShaderModule fragModule = createShaderModule(fragCode);

    // Shader stages
    VkPipelineShaderStageCreateInfo taskStageInfo{};
    taskStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    taskStageInfo.stage = VK_SHADER_STAGE_TASK_BIT_EXT;
    taskStageInfo.module = taskModule;
    taskStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo meshStageInfo{};
    meshStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    meshStageInfo.stage = VK_SHADER_STAGE_MESH_BIT_EXT;
    meshStageInfo.module = meshModule;
    meshStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragStageInfo{};
    fragStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStageInfo.module = fragModule;
    fragStageInfo.pName = "main";

    std::array<VkPipelineShaderStageCreateInfo, 3> shaderStages = {
        taskStageInfo, meshStageInfo, fragStageInfo
    };

    // NOTE: pVertexInputState and pInputAssemblyState MUST be nullptr
    // for mesh shader pipelines

    // Viewport state (dynamic)
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Depth stencil
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    // Color blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // Dynamic state
    std::array<VkDynamicState, 2> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    // Create graphics pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineInfo.pStages = shaderStages.data();

    // CRITICAL: These must be nullptr for mesh shader pipelines
    pipelineInfo.pVertexInputState = nullptr;
    pipelineInfo.pInputAssemblyState = nullptr;

    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                   nullptr, &graphicsPipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics pipeline!");
    }

    // Cleanup shader modules (no longer needed after pipeline creation)
    vkDestroyShaderModule(device, fragModule, nullptr);
    vkDestroyShaderModule(device, meshModule, nullptr);
    vkDestroyShaderModule(device, taskModule, nullptr);

    std::cout << "Graphics pipeline created (task + mesh + fragment)" << std::endl;
}
```

### Step 9: Update recordCommandBuffer() to Draw

```cpp
void Renderer::recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex) {
    // ... existing begin/render pass code ...

    // Bind pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

    // Bind scene descriptor set
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                             pipelineLayout, 0, 1,
                             &sceneDescriptorSets[currentFrame],
                             0, nullptr);

    // Push model matrix (identity for test)
    glm::mat4 model = glm::mat4(1.0f);
    vkCmdPushConstants(cmd, pipelineLayout,
                        VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT,
                        0, sizeof(glm::mat4), &model);

    // Draw using mesh shaders: 1 task workgroup
    pfnCmdDrawMeshTasksEXT(cmd, 1, 1, 1);

    // ... existing end render pass code ...
}
```

### Step 10: Update Constructor and Destructor

```cpp
Renderer::Renderer(Window& window) : window(window) {
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    loadMeshShaderFunctions();
    createCommandPool();
    createSwapChain();
    createImageViews();
    createDepthResources();
    createRenderPass();
    createFramebuffers();
    createCommandBuffers();
    createSyncObjects();
    createDescriptorSetLayouts();
    createPipelineLayout();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
    createGraphicsPipeline();
}

Renderer::~Renderer() {
    vkDestroyPipeline(device, graphicsPipeline, nullptr);
    // ... rest of cleanup
}
```

### Step 11: Build and Test

```bash
cd build
cmake ..
cmake --build .
./bin/Gravel
```

## Acceptance Criteria

- [ ] Shaders compile to SPIR-V without errors
- [ ] Shader modules load correctly
- [ ] `vkCmdDrawMeshTasksEXT` function pointer loaded
- [ ] Graphics pipeline created with task + mesh + fragment stages
- [ ] `pVertexInputState` and `pInputAssemblyState` are `nullptr`
- [ ] Colorful triangle renders on cornflower blue background
- [ ] Scene descriptor set bound without validation errors
- [ ] Push constants sent without errors
- [ ] No validation errors in console
- [ ] Clean shutdown

## Expected Visual Result

A colored triangle (red/green/blue vertices) centered on a cornflower blue background. The triangle should be visible immediately when the application starts.

## Expected Output

```
=== Gravel - GPU Mesh Shader Resurfacing ===

Window created: 1280x720
...
Mesh shader draw function loaded
...
Graphics pipeline created (task + mesh + fragment)
Compiling shader test.task
Compiling shader test.mesh
Compiling shader test.frag

Initialization complete
Entering main loop (press ESC to exit)

Application closed successfully
```

## Troubleshooting

### Shader Compilation Fails
```bash
# Test manual compilation:
glslangValidator --target-env vulkan1.3 -V shaders/test.mesh -o test.mesh.spv

# Common issues:
# - Missing #extension GL_EXT_mesh_shader : require
# - Using old NV_mesh_shader syntax instead of EXT
# - Wrong GLSL version (must be 460+)
```

### Pipeline Creation Fails
- Ensure `pVertexInputState = nullptr` and `pInputAssemblyState = nullptr`
- Verify shader stages use correct `VK_SHADER_STAGE_TASK_BIT_EXT` and `VK_SHADER_STAGE_MESH_BIT_EXT`
- Check that render pass is compatible with pipeline

### Triangle Not Visible
- Verify the MVP matrix transforms correctly (identity model should work for NDC coordinates)
- Check that `SetMeshOutputsEXT(3, 1)` is called in mesh shader
- Ensure `gl_PrimitiveTriangleIndicesEXT` is set
- Verify cull mode — try `VK_CULL_MODE_NONE` to rule out winding issues

### vkCmdDrawMeshTasksEXT is NULL
- Ensure `VK_EXT_MESH_SHADER_EXTENSION_NAME` is in device extensions
- Load via `vkGetDeviceProcAddr` (not `vkGetInstanceProcAddr`)

## Next Feature

[Feature 1.11: ImGui Integration](feature-11-imgui-integration.md)
