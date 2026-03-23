# Feature 20.1: MSAA Render Pipeline

**Epic**: [Epic 20 - MSAA and Alpha-to-Coverage](epic-20-msaa-coverage-fade.md)

## Goal

Add 4x multisampled rendering to the Vulkan pipeline. This is a prerequisite for alpha-to-coverage, which requires MSAA to convert fractional alpha into partial sample coverage.

## Files Modified

- `include/renderer/renderer.h`
- `src/renderer/renderer_init.cpp`
- `src/renderer/renderer.cpp`

## Changes

### 1. Renderer Members (renderer.h)

Add MSAA resources alongside existing depth buffer members:

```cpp
VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_4_BIT;
VkImage msaaColorImage = VK_NULL_HANDLE;
VkDeviceMemory msaaColorMemory = VK_NULL_HANDLE;
VkImageView msaaColorImageView = VK_NULL_HANDLE;
```

### 2. Create MSAA Color Image (renderer_init.cpp)

New function `createMsaaColorResources()` called after `createDepthResources()`:
- Create a multisampled color image matching swap chain format and extent
- `usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT`
- `samples = msaaSamples` (4x)
- Memory: `VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT` (lazily allocated if supported)

### 3. Update Depth Buffer (renderer_init.cpp)

In `createDepthResources()`, change:
```cpp
imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
```
to:
```cpp
imageInfo.samples = msaaSamples;
```

### 4. Update Render Pass (renderer_init.cpp)

Change from 2 attachments (color + depth) to 3 attachments (MSAA color + depth + resolve):

- **Attachment 0**: MSAA color — `samples = msaaSamples`, `storeOp = DONT_CARE`, `finalLayout = COLOR_ATTACHMENT_OPTIMAL`
- **Attachment 1**: Depth — `samples = msaaSamples` (matching)
- **Attachment 2**: Resolve — `samples = 1_BIT`, `storeOp = STORE`, `finalLayout = PRESENT_SRC_KHR`

Subpass gets a `pResolveAttachments` pointing to attachment 2.

### 5. Update Framebuffers (renderer_init.cpp)

Each framebuffer now has 3 image views: `[msaaColorImageView, depthImageView, swapChainImageViews[i]]`.

### 6. Cleanup on Swap Chain Recreation (renderer.cpp)

`cleanupSwapChain()` must destroy MSAA color image/view/memory alongside depth resources. `recreateSwapChain()` must recreate them.

## Verification

- Build compiles without errors
- No Vulkan validation errors on startup
- Rendering looks identical to pre-MSAA (slightly smoother edges from 4x MSAA)
- Swap chain resize works correctly
