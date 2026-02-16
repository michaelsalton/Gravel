# Feature 7.1: Control Map Texture Loading

**Epic**: [Epic 7 - Control Maps and Polish](../../07/epic-07-control-maps.md)
**Estimated Time**: 2-3 hours
**Prerequisites**: [Epic 6 Complete](../../06/epic-06-pebble-generation.md)

## Goal

Add texture loading support to load control maps (PNG/JPG images) that define which parametric surface type to use for each region of the mesh.

## What You'll Build

- Texture class wrapper (VkImage, VkImageView, VkSampler)
- stb_image integration for PNG/JPG loading
- Descriptor set binding for textures
- ImGui file picker for control maps

## Implementation Steps

### Step 1: Add stb_image to Project

Add to CMakeLists.txt:

```cmake
# STB image (header-only)
add_library(stb_image INTERFACE)
target_include_directories(stb_image INTERFACE libs/stb)

# Add to main target
target_link_libraries(Gravel PRIVATE ... stb_image)
```

Download `stb_image.h` to `libs/stb/` folder.

### Step 2: Create Texture Class

Create `include/Texture.h`:

```cpp
#pragma once
#include <vulkan/vulkan.h>
#include <string>

class Texture {
public:
    void createFromFile(VkDevice device, VkPhysicalDevice physicalDevice,
                       VkCommandPool commandPool, VkQueue graphicsQueue,
                       const std::string& filepath);
    void destroy(VkDevice device);

    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;

    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t mipLevels = 1;
};
```

Create `src/Texture.cpp`:

```cpp
#include "Texture.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <stdexcept>
#include <iostream>

void Texture::createFromFile(VkDevice device, VkPhysicalDevice physicalDevice,
                              VkCommandPool commandPool, VkQueue graphicsQueue,
                              const std::string& filepath) {
    // Load image
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(filepath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

    if (!pixels) {
        throw std::runtime_error("Failed to load texture: " + filepath);
    }

    width = texWidth;
    height = texHeight;
    VkDeviceSize imageSize = width * height * 4;  // RGBA

    // Create staging buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    createBuffer(device, physicalDevice, imageSize,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                stagingBuffer, stagingMemory);

    void* data;
    vkMapMemory(device, stagingMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, imageSize);
    vkUnmapMemory(device, stagingMemory);

    stbi_image_free(pixels);

    // Create image
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vkCreateImage(device, &imageInfo, nullptr, &image);

    // Allocate memory
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits,
                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vkAllocateMemory(device, &allocInfo, nullptr, &memory);
    vkBindImageMemory(device, image, memory, 0);

    // Transition and copy
    transitionImageLayout(device, commandPool, graphicsQueue, image,
                         VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    copyBufferToImage(device, commandPool, graphicsQueue, stagingBuffer, image, width, height);

    transitionImageLayout(device, commandPool, graphicsQueue, image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // Cleanup staging
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);

    // Create image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    vkCreateImageView(device, &viewInfo, nullptr, &imageView);

    // Create sampler
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = 16.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    vkCreateSampler(device, &samplerInfo, nullptr, &sampler);

    std::cout << "✓ Texture loaded: " << filepath << " (" << width << "×" << height << ")" << std::endl;
}

void Texture::destroy(VkDevice device) {
    vkDestroySampler(device, sampler, nullptr);
    vkDestroyImageView(device, imageView, nullptr);
    vkDestroyImage(device, image, nullptr);
    vkFreeMemory(device, memory, nullptr);
}
```

### Step 3: Add to Descriptor Set

Update shaderInterface.h:

```glsl
#define BINDING_CONTROL_MAP 7

#ifndef __cplusplus
layout(set = SET_PER_OBJECT, binding = BINDING_CONTROL_MAP) uniform sampler2D controlMapTexture;
#endif
```

Update descriptor set layout:

```cpp
VkDescriptorSetLayoutBinding controlMapBinding{};
controlMapBinding.binding = BINDING_CONTROL_MAP;
controlMapBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
controlMapBinding.descriptorCount = 1;
controlMapBinding.stageFlags = VK_SHADER_STAGE_TASK_BIT_EXT;
```

### Step 4: Add ImGui Controls

```cpp
Texture controlMapTexture;
std::string controlMapFile = "None";
bool useControlMap = false;

ImGui::Separator();
ImGui::Text("Control Map");
ImGui::Text("  Current: %s", controlMapFile.c_str());

if (ImGui::Button("Load Control Map")) {
    // Simple hardcoded paths for now
    // Later: use file dialog library
    loadControlMap("assets/control_maps/dragon_map.png");
}

ImGui::Checkbox("Use Control Map", &useControlMap);

void loadControlMap(const std::string& filepath) {
    controlMapTexture.destroy(device);
    controlMapTexture.createFromFile(device, physicalDevice, commandPool, graphicsQueue, filepath);
    controlMapFile = filepath;

    // Update descriptor set
    updateControlMapDescriptor();
}
```

## Acceptance Criteria

- [ ] Can load PNG textures
- [ ] Texture uploaded to GPU
- [ ] Descriptor set updated
- [ ] ImGui shows loaded texture filename
- [ ] No validation errors

## Next Steps

Next feature: **[Feature 7.2 - Control Map Sampling](feature-02-control-map-sampling.md)**
