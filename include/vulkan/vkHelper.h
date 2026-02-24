#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>
#include <cstddef>

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
