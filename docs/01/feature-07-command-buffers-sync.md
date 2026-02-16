# Feature 1.7: Command Buffers and Synchronization

**Epic**: [Epic 1 - Vulkan Infrastructure](epic-01-vulkan-infrastructure.md)
**Estimated Time**: 2-3 hours
**Prerequisites**: [Feature 1.6 - Depth Buffer and Render Pass](feature-06-depth-buffer-render-pass.md)

## Goal

Allocate command buffers from the command pool and create synchronization primitives (semaphores and fences) to coordinate frame rendering with swap chain image acquisition and presentation.

## What You'll Build

- Command buffer allocation (one per frame in flight)
- Semaphores for image acquisition and render completion
- Fences for CPU-GPU frame synchronization
- Double buffering with MAX_FRAMES_IN_FLIGHT = 2

## Files to Create/Modify

### Modify
- `include/renderer.h`
- `src/renderer.cpp`

## Implementation Steps

### Step 1: Add Synchronization Members to include/renderer.h

```cpp
// Add to Renderer class private section:

    void createCommandBuffers();
    void createSyncObjects();

    // Command buffers (one per frame in flight)
    std::vector<VkCommandBuffer> commandBuffers;

    // Synchronization primitives
    static const int MAX_FRAMES_IN_FLIGHT = 2;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;

    // Frame tracking
    uint32_t currentFrame = 0;
```

### Step 2: Allocate Command Buffers

```cpp
void Renderer::createCommandBuffers() {
    commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

    if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers!");
    }

    std::cout << "Command buffers allocated: " << commandBuffers.size() << std::endl;
}
```

### Step 3: Create Synchronization Objects

```cpp
void Renderer::createSyncObjects() {
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    // Create fences in signaled state so first frame doesn't deadlock
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr,
                              &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr,
                              &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr,
                          &inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create synchronization objects!");
        }
    }

    std::cout << "Synchronization objects created ("
              << MAX_FRAMES_IN_FLIGHT << " frames in flight)" << std::endl;
}
```

### Step 4: Update Constructor and Destructor

```cpp
Renderer::Renderer(Window& window) : window(window) {
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createCommandPool();
    createSwapChain();
    createImageViews();
    createDepthResources();
    createRenderPass();
    createFramebuffers();
    createCommandBuffers();
    createSyncObjects();
}

Renderer::~Renderer() {
    // Destroy sync objects
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(device, inFlightFences[i], nullptr);
    }

    // Command buffers are freed when command pool is destroyed
    cleanupSwapChain();
    vkDestroyRenderPass(device, renderPass, nullptr);
    vkDestroyCommandPool(device, commandPool, nullptr);
    // ... rest of cleanup
}
```

### Step 5: Build and Test

```bash
cd build
cmake ..
cmake --build .
./bin/Gravel
```

## Acceptance Criteria

- [ ] Command buffers allocated (one per frame in flight)
- [ ] Image available semaphores created (one per frame in flight)
- [ ] Render finished semaphores created (one per frame in flight)
- [ ] Fences created with `VK_FENCE_CREATE_SIGNALED_BIT` (one per frame in flight)
- [ ] `MAX_FRAMES_IN_FLIGHT = 2` for double buffering
- [ ] No validation errors in console
- [ ] Clean shutdown with proper sync object destruction

## Synchronization Overview

```
Frame N:
  1. vkWaitForFences(inFlightFences[N])     -- CPU waits for GPU to finish frame N
  2. vkAcquireNextImageKHR(imageAvailable)   -- Get next swap chain image
  3. vkResetFences(inFlightFences[N])        -- Reset fence for reuse
  4. Record command buffer
  5. vkQueueSubmit(wait: imageAvailable,     -- Submit: wait for image, signal when done
                   signal: renderFinished,
                   fence: inFlightFences[N])
  6. vkQueuePresentKHR(wait: renderFinished) -- Present: wait for render to finish
```

## Expected Output

```
=== Gravel - GPU Mesh Shader Resurfacing ===

Window created: 1280x720
...
Framebuffers created: 3
Command buffers allocated: 2
Synchronization objects created (2 frames in flight)

Initialization complete
Entering main loop (press ESC to exit)

Application closed successfully
```

## Troubleshooting

### Deadlock on First Frame
- Fences MUST be created with `VK_FENCE_CREATE_SIGNALED_BIT`. Otherwise `vkWaitForFences` on the first frame will block forever since no previous submit has signaled the fence.

### Command Buffer Recording Errors
- Command buffers must be in the "initial" state before recording. Use `VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT` (set in Feature 1.4) to allow individual reset.
- Call `vkResetCommandBuffer` before each recording.

### Frame Index Confusion
- `currentFrame` (0 or 1) indexes into sync objects and command buffers
- `imageIndex` (0, 1, or 2) indexes into swap chain images/framebuffers
- These are different values — don't mix them up

## Next Feature

[Feature 1.8: Basic Render Loop](feature-08-render-loop.md)
