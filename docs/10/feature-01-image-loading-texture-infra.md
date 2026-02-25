# Feature 10.1: Image Loading & Vulkan Texture Infrastructure

**Epic**: [Epic 10 - Textures, Skeleton & Bone Skinning](epic-10-textures-skeleton-skinning.md)
**Prerequisites**: None (foundation feature)

## Goal

Add the ability to load images from disk and create Vulkan images, image views, and samplers. This is the foundation for all texture features (AO, element type maps).

## What You'll Build

- stb_image integration for PNG/JPG loading
- `ImageLoader` class that returns RGBA pixel data
- `VulkanTexture` class wrapping VkImage + VkDeviceMemory + VkImageView
- Helper functions for image layout transitions and staging buffer uploads
- Linear and nearest VkSamplers on the Renderer

## Files to Create

- `libs/stb/stb_image.h` — Copy from `D:\Development\Research\resurfacing\libs\stb\stb_image.h`
- `include/loaders/ImageLoader.h` — ImageData struct + load function
- `src/loaders/ImageLoader.cpp` — stb_image implementation

## Files to Modify

- `include/vulkan/vkHelper.h` — Add VulkanTexture class
- `src/vulkan/vkHelper.cpp` — Implement texture creation, layout transitions, staging upload
- `include/renderer/renderer.h` — Add `VkSampler linearSampler, nearestSampler`, `createSamplers()` method
- `src/renderer/renderer_init.cpp` — Implement `createSamplers()`
- `src/renderer/renderer.cpp` — Call `createSamplers()` in init, destroy in destructor
- `CMakeLists.txt` — Add `src/loaders/ImageLoader.cpp` to SOURCES, `libs/stb` to include dirs

## Implementation Steps

### Step 1: Copy stb_image

Copy `stb_image.h` from the reference project to `libs/stb/`.

### Step 2: Create ImageLoader

```cpp
// include/loaders/ImageLoader.h
struct ImageData {
    std::vector<uint8_t> pixels;  // RGBA, 4 bytes per pixel
    uint32_t width = 0;
    uint32_t height = 0;
};

class ImageLoader {
public:
    static ImageData load(const std::string& filepath);
};
```

In `src/loaders/ImageLoader.cpp`, `#define STB_IMAGE_IMPLEMENTATION` before including `stb_image.h`. Use `stbi_load()` with `STBI_rgb_alpha` to always get 4-channel output.

### Step 3: Add VulkanTexture to vkHelper

```cpp
class VulkanTexture {
public:
    void create(VkDevice device, VkPhysicalDevice physicalDevice,
                uint32_t width, uint32_t height, VkFormat format,
                VkImageUsageFlags usage);
    void destroy();

    VkImage getImage() const;
    VkImageView getImageView() const;
    VkFormat getFormat() const;

private:
    VkDevice device = VK_NULL_HANDLE;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
};
```

Add standalone helper functions:
- `transitionImageLayout(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout)` — Pipeline barrier with correct access masks and stage flags
- `copyBufferToImage(VkCommandBuffer cmd, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)` — VkBufferImageCopy for RGBA data

### Step 4: Create samplers

In `renderer_init.cpp`, implement `Renderer::createSamplers()`:
- Linear sampler: `VK_FILTER_LINEAR`, `VK_SAMPLER_MIPMAP_MODE_LINEAR`, `VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE`
- Nearest sampler: `VK_FILTER_NEAREST`, `VK_SAMPLER_MIPMAP_MODE_NEAREST`, `VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE`

### Step 5: Update CMakeLists.txt

Add `src/loaders/ImageLoader.cpp` to SOURCES list. Add `${CMAKE_SOURCE_DIR}/libs/stb` to `target_include_directories`.

## Acceptance Criteria

- [ ] stb_image.h present in `libs/stb/`
- [ ] `ImageLoader::load()` successfully loads a PNG and returns pixel data
- [ ] `VulkanTexture::create()` creates a device-local VkImage with image view
- [ ] `transitionImageLayout()` handles UNDEFINED→TRANSFER_DST and TRANSFER_DST→SHADER_READ_ONLY transitions
- [ ] Both samplers created without validation errors
- [ ] Build and run with no visual change (samplers exist but aren't bound yet)
