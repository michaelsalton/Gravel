# Feature 1.8: Basic Render Loop

**Epic**: [Epic 1 - Vulkan Infrastructure](epic-01-vulkan-infrastructure.md)
**Estimated Time**: 2 hours
**Prerequisites**: [Feature 1.7 - Command Buffers and Synchronization](feature-07-command-buffers-sync.md)

## Goal

Implement the frame acquisition, command recording, and presentation loop that clears the screen to a solid color, handling window resize by recreating the swap chain.

## What You'll Build

- Frame begin: wait for fence, acquire next image, reset command buffer
- Command recording: begin render pass, set viewport/scissor, end render pass
- Frame end: submit command buffer, present image
- Clear color (cornflower blue)
- Swap chain recreation on window resize

## Files to Create/Modify

### Modify
- `include/renderer.h`
- `src/renderer.cpp`
- `src/main.cpp`

## Implementation Steps

### Step 1: Add Render Loop Methods to include/renderer.h

```cpp
// Add to Renderer class public section:

    // Render loop
    void beginFrame();
    void endFrame();
    void waitIdle();

    // Swap chain management
    void recreateSwapChain();

    bool isFrameStarted() const { return frameStarted; }
    VkDevice getDevice() const { return device; }

// Add to Renderer class private section:

    void recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex);

    uint32_t currentImageIndex = 0;
    bool frameStarted = false;
```

### Step 2: Implement beginFrame()

```cpp
void Renderer::beginFrame() {
    // Wait for the previous frame using this slot to finish
    vkWaitForFences(device, 1, &inFlightFences[currentFrame],
                    VK_TRUE, UINT64_MAX);

    // Acquire the next image from the swap chain
    VkResult result = vkAcquireNextImageKHR(
        device, swapChain, UINT64_MAX,
        imageAvailableSemaphores[currentFrame],
        VK_NULL_HANDLE, &currentImageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain();
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swap chain image!");
    }

    // Only reset the fence if we are submitting work
    vkResetFences(device, 1, &inFlightFences[currentFrame]);

    // Reset command buffer for this frame
    vkResetCommandBuffer(commandBuffers[currentFrame], 0);

    frameStarted = true;
}
```

### Step 3: Implement recordCommandBuffer()

```cpp
void Renderer::recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin recording command buffer!");
    }

    // Begin render pass
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapChainExtent;

    // Clear values: cornflower blue for color, 1.0 for depth
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{0.392f, 0.584f, 0.929f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Set dynamic viewport
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapChainExtent.width);
    viewport.height = static_cast<float>(swapChainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    // Set dynamic scissor
    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapChainExtent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // TODO: Bind pipeline and draw here (Feature 1.10)

    vkCmdEndRenderPass(cmd);

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer!");
    }
}
```

### Step 4: Implement endFrame()

```cpp
void Renderer::endFrame() {
    if (!frameStarted) return;

    // Record commands
    recordCommandBuffer(commandBuffers[currentFrame], currentImageIndex);

    // Submit command buffer
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
    VkPipelineStageFlags waitStages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

    VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo,
                      inFlightFences[currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit draw command buffer!");
    }

    // Present
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {swapChain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &currentImageIndex;

    VkResult result = vkQueuePresentKHR(presentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreateSwapChain();
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swap chain image!");
    }

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    frameStarted = false;
}
```

### Step 5: Implement recreateSwapChain()

```cpp
void Renderer::recreateSwapChain() {
    // Handle minimized window
    int width = 0, height = 0;
    glfwGetFramebufferSize(window.getHandle(), &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window.getHandle(), &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(device);

    cleanupSwapChain();

    createSwapChain();
    createImageViews();
    createDepthResources();
    createFramebuffers();

    std::cout << "Swap chain recreated: " << width << "x" << height << std::endl;
}

void Renderer::waitIdle() {
    vkDeviceWaitIdle(device);
}
```

### Step 6: Update src/main.cpp

```cpp
#include "window.h"
#include "renderer.h"
#include <iostream>
#include <stdexcept>

int main() {
    std::cout << "=== Gravel - GPU Mesh Shader Resurfacing ===" << std::endl;
    std::cout << std::endl;

    try {
        Window window(1280, 720, "Gravel - Mesh Shader Resurfacing");
        Renderer renderer(window);

        std::cout << "\nInitialization complete" << std::endl;
        std::cout << "Entering main loop (press ESC to exit)\n" << std::endl;

        while (!window.shouldClose()) {
            window.pollEvents();

            if (window.wasResized()) {
                window.resetResizedFlag();
                renderer.recreateSwapChain();
            }

            renderer.beginFrame();
            if (renderer.isFrameStarted()) {
                renderer.endFrame();
            }
        }

        renderer.waitIdle();
        std::cout << "\nApplication closed successfully" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
```

### Step 7: Build and Test

```bash
cd build
cmake ..
cmake --build .
./bin/Gravel
```

## Acceptance Criteria

- [ ] Screen clears to cornflower blue (0.392, 0.584, 0.929)
- [ ] Application runs at 60+ FPS without stuttering
- [ ] Window can be resized without crashes
- [ ] Minimizing and restoring the window works correctly
- [ ] No Vulkan validation errors in console
- [ ] Clean shutdown with vkDeviceWaitIdle before destruction
- [ ] Frame synchronization prevents rendering artifacts

## Expected Output

```
=== Gravel - GPU Mesh Shader Resurfacing ===

Window created: 1280x720
Vulkan instance created (API 1.3)
...
Synchronization objects created (2 frames in flight)

Initialization complete
Entering main loop (press ESC to exit)

Swap chain recreated: 800x600  (if window resized)

Application closed successfully
```

## Troubleshooting

### Screen Stays Black
- Verify the render pass `loadOp` is `VK_ATTACHMENT_LOAD_OP_CLEAR`
- Check that `clearValues` array matches the number of attachments (color + depth)
- Ensure `vkCmdBeginRenderPass` receives the correct `pClearValues`

### Crash on Resize
- Call `vkDeviceWaitIdle` before `cleanupSwapChain`
- Check that all swap chain-dependent resources are recreated
- Handle minimized window (0x0 framebuffer) by waiting for events

### Validation Error: Fence Not Signaled
- Fences must be created with `VK_FENCE_CREATE_SIGNALED_BIT`
- Only reset the fence after confirming the image was acquired successfully

### Tearing or Flickering
- Verify `MAX_FRAMES_IN_FLIGHT = 2` matches the number of sync objects
- Ensure the correct frame index is used for semaphores and fences
- Check that `currentFrame` advances modulo `MAX_FRAMES_IN_FLIGHT`

## Next Feature

[Feature 1.9: Descriptor Set Layouts](feature-09-descriptor-sets.md)
