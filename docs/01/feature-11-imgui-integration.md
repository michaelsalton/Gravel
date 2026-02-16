# Feature 1.11: ImGui Integration

**Epic**: [Epic 1 - Vulkan Infrastructure](epic-01-vulkan-infrastructure.md)
**Estimated Time**: 2-3 hours
**Prerequisites**: [Feature 1.10 - Test Mesh Shader Pipeline](feature-10-mesh-shader-pipeline.md)

## Goal

Integrate Dear ImGui with GLFW and Vulkan backends to provide a runtime UI for parameter control, performance monitoring, and debug visualization. The UI should display FPS, camera controls, and placeholder sections for future resurfacing parameters.

## What You'll Build

- ImGui initialization with GLFW and Vulkan backends
- Dedicated descriptor pool for ImGui
- ImGui render pass (rendered after main scene)
- Basic UI window with FPS counter
- Camera position/rotation controls
- Placeholder controls for future features

## Files to Create/Modify

### Modify
- `include/renderer.h`
- `src/renderer.cpp`
- `src/main.cpp`
- `CMakeLists.txt` (add ImGui sources and include paths)

## Implementation Steps

### Step 1: Update CMakeLists.txt

Add ImGui source files and include directories:

```cmake
# Update SOURCES to include ImGui
set(SOURCES
    src/main.cpp
    src/window.cpp
    src/renderer.cpp
    ${IMGUI_SOURCES}
)

# Add ImGui include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${Vulkan_INCLUDE_DIRS}
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)
```

### Step 2: Add ImGui Members to include/renderer.h

```cpp
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

// Add to Renderer class private section:

    void initImGui();
    void cleanupImGui();
    void renderImGui(VkCommandBuffer cmd);

    // ImGui
    VkDescriptorPool imguiDescriptorPool = VK_NULL_HANDLE;
```

### Step 3: Create Dedicated ImGui Descriptor Pool

ImGui needs its own descriptor pool to avoid conflicts with the main application:

```cpp
void Renderer::initImGui() {
    // Create dedicated descriptor pool for ImGui
    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1000;
    poolInfo.poolSizeCount = static_cast<uint32_t>(std::size(poolSizes));
    poolInfo.pPoolSizes = poolSizes;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr,
                                &imguiDescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create ImGui descriptor pool!");
    }

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Set style
    ImGui::StyleColorsDark();

    // Initialize GLFW backend
    ImGui_ImplGlfw_InitForVulkan(window.getHandle(), true);

    // Initialize Vulkan backend
    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = instance;
    initInfo.PhysicalDevice = physicalDevice;
    initInfo.Device = device;
    initInfo.QueueFamily = queueFamilyIndices.graphicsFamily.value();
    initInfo.Queue = graphicsQueue;
    initInfo.DescriptorPool = imguiDescriptorPool;
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = static_cast<uint32_t>(swapChainImages.size());
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.RenderPass = renderPass;
    initInfo.Subpass = 0;

    ImGui_ImplVulkan_Init(&initInfo);

    std::cout << "ImGui initialized" << std::endl;
}
```

### Step 4: ImGui Cleanup

```cpp
void Renderer::cleanupImGui() {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    vkDestroyDescriptorPool(device, imguiDescriptorPool, nullptr);
}
```

### Step 5: Render ImGui

```cpp
void Renderer::renderImGui(VkCommandBuffer cmd) {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // === Main UI Window ===
    ImGui::Begin("Gravel Controls");

    // FPS Counter
    ImGui::Text("FPS: %.1f (%.3f ms/frame)",
                ImGui::GetIO().Framerate,
                1000.0f / ImGui::GetIO().Framerate);
    ImGui::Separator();

    // Camera controls (placeholder values)
    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        static float cameraPos[3] = {0.0f, 0.0f, 3.0f};
        static float cameraRot[2] = {0.0f, 0.0f};
        ImGui::DragFloat3("Position", cameraPos, 0.1f);
        ImGui::DragFloat2("Rotation", cameraRot, 1.0f);
    }

    // Resurfacing controls (placeholder)
    if (ImGui::CollapsingHeader("Resurfacing")) {
        ImGui::Text("(Available after Epic 3)");
        static int gridResolution = 8;
        ImGui::SliderInt("Grid Resolution", &gridResolution, 2, 32);
        static int surfaceType = 0;
        const char* surfaceTypes[] = {"Torus", "Sphere", "B-Spline"};
        ImGui::Combo("Surface Type", &surfaceType, surfaceTypes, 3);
    }

    // Rendering controls (placeholder)
    if (ImGui::CollapsingHeader("Rendering")) {
        static bool wireframe = false;
        ImGui::Checkbox("Wireframe", &wireframe);
        static bool showNormals = false;
        ImGui::Checkbox("Show Normals", &showNormals);
    }

    ImGui::End();

    // Render ImGui
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}
```

### Step 6: Integrate ImGui into Render Loop

Update `recordCommandBuffer()` to call ImGui rendering after the main scene:

```cpp
void Renderer::recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &beginInfo);

    // Begin render pass
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapChainExtent;

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{0.392f, 0.584f, 0.929f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Set viewport and scissor
    VkViewport viewport{};
    viewport.width = static_cast<float>(swapChainExtent.width);
    viewport.height = static_cast<float>(swapChainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.extent = swapChainExtent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // Draw scene (mesh shader triangle from Feature 1.10)
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                             pipelineLayout, 0, 1,
                             &sceneDescriptorSets[currentFrame], 0, nullptr);

    glm::mat4 model = glm::mat4(1.0f);
    vkCmdPushConstants(cmd, pipelineLayout,
                        VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT,
                        0, sizeof(glm::mat4), &model);
    pfnCmdDrawMeshTasksEXT(cmd, 1, 1, 1);

    // Draw ImGui on top
    renderImGui(cmd);

    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);
}
```

### Step 7: Update Constructor and Destructor

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
    initImGui();
}

Renderer::~Renderer() {
    vkDeviceWaitIdle(device);
    cleanupImGui();
    vkDestroyPipeline(device, graphicsPipeline, nullptr);
    // ... rest of cleanup
}
```

### Step 8: Build and Test

```bash
cd build
cmake ..
cmake --build .
./bin/Gravel
```

## Acceptance Criteria

- [ ] ImGui initializes without errors
- [ ] "Gravel Controls" window displays in the application
- [ ] FPS counter shows real-time frame rate
- [ ] Camera position/rotation sliders are interactive
- [ ] Placeholder sections for Resurfacing and Rendering are visible
- [ ] ImGui renders on top of the mesh shader triangle
- [ ] Mouse and keyboard input work in ImGui panels
- [ ] Window resizing does not crash ImGui
- [ ] No validation errors in console
- [ ] Clean shutdown

## Expected Visual Result

The colored triangle from Feature 1.10 with a dark-themed ImGui window overlaid showing:
- FPS counter at the top
- Collapsible "Camera" section with position/rotation controls
- Collapsible "Resurfacing" section with grid resolution and surface type
- Collapsible "Rendering" section with wireframe toggle

## Expected Output

```
=== Gravel - GPU Mesh Shader Resurfacing ===

Window created: 1280x720
...
Graphics pipeline created (task + mesh + fragment)
ImGui initialized

Initialization complete
Entering main loop (press ESC to exit)

Application closed successfully
```

## Troubleshooting

### ImGui Font Not Rendering
- ImGui_ImplVulkan_Init handles font upload automatically in recent versions
- If using an older version, you may need to manually upload fonts via a one-time command buffer

### ImGui Not Receiving Input
- Ensure `ImGui_ImplGlfw_InitForVulkan(window, true)` passes `true` to install GLFW callbacks
- If you have custom GLFW callbacks, chain them with ImGui's callbacks

### Validation Error: Descriptor Pool Exhausted
- ImGui's descriptor pool must have `VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT`
- Use a generous pool size (1000 per type is standard for ImGui)

### Crash on Window Resize
- ImGui needs to know about the new swap chain after recreation
- Call `ImGui_ImplVulkan_SetMinImageCount` if image count changes
- The render pass reference in ImGui init must still be valid

### ImGui Draws Over Everything
- ImGui's `RenderDrawData` should be called after all scene rendering
- It renders in the same render pass, using the existing color attachment

## Epic 1 Complete!

With ImGui integration, Epic 1 is complete. You now have:
- Vulkan 1.3 with mesh shader support
- Swap chain with depth buffer and render pass
- Mesh shader pipeline rendering a test triangle
- Descriptor infrastructure for Scene, HalfEdge, and PerObject data
- ImGui for runtime parameter control

**Next**: [Epic 2 - Mesh Loading and GPU Upload](../02/epic-02-mesh-loading.md)
