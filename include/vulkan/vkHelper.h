#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>
#include <cstddef>
#include <vector>

class StorageBuffer {
public:
    StorageBuffer() = default;
    ~StorageBuffer() { destroy(); }

    StorageBuffer(const StorageBuffer&) = delete;
    StorageBuffer& operator=(const StorageBuffer&) = delete;

    StorageBuffer(StorageBuffer&& other) noexcept;
    StorageBuffer& operator=(StorageBuffer&& other) noexcept;

    void create(VkDevice device, VkPhysicalDevice physicalDevice,
                size_t size, const void* data = nullptr);
    void update(const void* data, size_t size);
    void destroy();

    VkBuffer getBuffer() const { return buffer; }
    VkDeviceMemory getMemory() const { return memory; }
    size_t getSize() const { return bufferSize; }

private:
    static uint32_t findMemoryType(VkPhysicalDevice physicalDevice,
                                   uint32_t typeFilter,
                                   VkMemoryPropertyFlags properties);

    VkDevice device = VK_NULL_HANDLE;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    size_t bufferSize = 0;
};

class VulkanTexture {
public:
    VulkanTexture() = default;
    ~VulkanTexture() { destroy(); }

    VulkanTexture(const VulkanTexture&) = delete;
    VulkanTexture& operator=(const VulkanTexture&) = delete;

    VulkanTexture(VulkanTexture&& other) noexcept;
    VulkanTexture& operator=(VulkanTexture&& other) noexcept;

    void create(VkDevice device, VkPhysicalDevice physicalDevice,
                uint32_t width, uint32_t height, VkFormat format,
                VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    void uploadData(VkCommandPool commandPool, VkQueue queue,
                    VkPhysicalDevice physicalDevice,
                    const void* pixels, size_t dataSize);

    void destroy();

    VkImage getImage() const { return image; }
    VkImageView getImageView() const { return imageView; }
    VkDeviceMemory getMemory() const { return memory; }
    VkFormat getFormat() const { return format; }
    uint32_t getWidth() const { return width; }
    uint32_t getHeight() const { return height; }
    size_t getMemorySize() const { return memorySize; }

private:
    VkDevice device = VK_NULL_HANDLE;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
    uint32_t width = 0;
    uint32_t height = 0;
    size_t memorySize = 0;
};

// Standalone Vulkan helper functions
void transitionImageLayout(VkCommandBuffer cmd, VkImage image,
                           VkImageLayout oldLayout, VkImageLayout newLayout);

void copyBufferToImage(VkCommandBuffer cmd, VkBuffer buffer, VkImage image,
                       uint32_t width, uint32_t height);

uint32_t findMemoryType(VkPhysicalDevice physicalDevice,
                        uint32_t typeFilter, VkMemoryPropertyFlags properties);
