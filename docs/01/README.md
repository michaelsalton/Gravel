# Epic 1 Features: Vulkan Infrastructure

Complete list of bite-sized features for Epic 1. Each feature should take 1-3 hours to implement.

## Feature List

### ✅ [Feature 1.1: CMake and Vulkan SDK Setup](feature-01-cmake-vulkan-setup.md)
- Configure CMake to find Vulkan SDK
- Verify Vulkan 1.3+ is available
- Print Vulkan version and extensions
- **Time**: 1-2 hours
- **Prerequisites**: None

### ✅ [Feature 1.2: GLFW Window Creation](feature-02-glfw-window.md)
- Integrate GLFW library
- Create window class wrapper
- Implement basic event handling (ESC to close, resize)
- **Time**: 1-2 hours
- **Prerequisites**: Feature 1.1

### ✅ [Feature 1.3: Vulkan Instance and Surface](feature-03-vulkan-instance.md)
- Create Vulkan instance (API 1.3)
- Set up debug messenger and validation layers
- Create window surface
- **Time**: 2-3 hours
- **Prerequisites**: Feature 1.2

### Feature 1.4: Physical and Logical Device
- Enumerate physical devices
- Check for mesh shader support (VK_EXT_mesh_shader)
- Select suitable GPU
- Create logical device with mesh shader features
- Print device properties and limits
- **Time**: 2-3 hours
- **Prerequisites**: Feature 1.3
- **Key Code**: `VkPhysicalDeviceMeshShaderFeaturesEXT`

### Feature 1.5: Swap Chain
- Create swap chain with optimal format/present mode
- Create image views for swap chain images
- Handle window resize (recreate swap chain)
- **Time**: 2-3 hours
- **Prerequisites**: Feature 1.4
- **Key Code**: `VkSwapchainCreateInfoKHR`

### Feature 1.6: Depth Buffer and Render Pass
- Create depth image and image view (D32_SFLOAT)
- Set up dynamic rendering or traditional render pass
- Configure color/depth attachments
- **Time**: 2 hours
- **Prerequisites**: Feature 1.5
- **Key Code**: `VK_KHR_dynamic_rendering` or `VkRenderPass`

### Feature 1.7: Command Buffers and Synchronization
- Create command pool
- Allocate command buffers (double/triple buffering)
- Set up fences and semaphores for frame synchronization
- Implement frame-in-flight management
- **Time**: 2-3 hours
- **Prerequisites**: Feature 1.6
- **Key Code**: `VkCommandBuffer`, `VkFence`, `VkSemaphore`

### Feature 1.8: Basic Render Loop
- Implement beginFrame() / endFrame()
- Begin/end rendering (dynamic rendering or render pass)
- Clear screen to solid color
- Submit commands to queue
- Present to swap chain
- **Time**: 2 hours
- **Prerequisites**: Feature 1.7
- **Key Code**: `vkQueueSubmit`, `vkQueuePresentKHR`

### Feature 1.9: Descriptor Set Layouts
- Create Set 0 (SceneSet) layout: ViewUBO, ShadingUBO
- Create Set 1 (HESet) layout: Mesh data SSBOs (arrays)
- Create Set 2 (PerObjectSet) layout: Config UBO, textures
- Create descriptor pool
- **Time**: 2-3 hours
- **Prerequisites**: Feature 1.4
- **Key Code**: `VkDescriptorSetLayout`, `VkDescriptorPool`

### Feature 1.10: Test Mesh Shader Pipeline
- Write minimal test.task (emits 1 mesh invocation)
- Write minimal test.mesh (outputs hardcoded triangle)
- Write minimal test.frag (solid color)
- Compile shaders to SPIR-V
- Create graphics pipeline (task+mesh+frag, NO vertex input)
- Load vkCmdDrawMeshTasksEXT
- Draw triangle on screen
- **Time**: 3-4 hours
- **Prerequisites**: Feature 1.9
- **Key Code**: `vkCmdDrawMeshTasksEXT`, `pVertexInputState = nullptr`

### Feature 1.11: ImGui Integration
- Add ImGui sources to CMake
- Initialize ImGui with GLFW and Vulkan backends
- Create basic UI window with FPS counter
- Add camera controls (placeholders)
- Render ImGui in separate pass
- **Time**: 2-3 hours
- **Prerequisites**: Feature 1.8
- **Key Code**: `ImGui_ImplVulkan_Init`, `ImGui_ImplGlfw_Init`

## Total Features: 11
**Estimated Total Time**: 22-30 hours

## Implementation Order

Follow features in numerical order. Each feature builds on the previous one and can be implemented in a single coding session.

## Testing After Each Feature

After implementing each feature, verify:
1. Code compiles without warnings
2. Application runs without crashes
3. No Vulkan validation errors
4. Feature-specific acceptance criteria met

## Next Epic

Once all features are complete, move to:
[Epic 2: Mesh Loading and GPU Upload](../../epic-02-mesh-loading.md)
