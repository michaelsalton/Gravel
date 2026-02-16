# Feature 2.3: GPU Buffer Upload (SSBOs)

**Epic**: [Epic 2 - Mesh Loading and GPU Upload](../../epic-02-mesh-loading.md)
**Estimated Time**: 2-3 hours
**Prerequisites**: [Feature 2.2 - Half-Edge Conversion](feature-02-halfedge-conversion.md), Epic 1 Feature 1.9

## Goal

Upload all half-edge Structure of Arrays (SoA) data to the GPU as Storage Buffer Objects (SSBOs) and bind them to descriptor set 1 (HESet). This makes the mesh topology accessible from shaders.

## What You'll Build

- StorageBuffer helper class for SSBO management
- GPU upload of 17 separate SoA arrays
- Descriptor set 1 bindings for half-edge data
- MeshInfo UBO with mesh counts

## Files to Create/Modify

### Create/Modify
- `src/vkHelper.hpp` (add StorageBuffer class)

### Modify
- `src/renderer.hpp`
- `src/renderer.cpp`
- `src/main.cpp`

## Implementation Steps

### Step 1: Add StorageBuffer to src/vkHelper.hpp

```cpp
#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>

// Add to existing vkHelper.hpp or create new file

class StorageBuffer {
public:
    StorageBuffer() = default;
    ~StorageBuffer() { destroy(); }

    // Disable copy
    StorageBuffer(const StorageBuffer&) = delete;
    StorageBuffer& operator=(const StorageBuffer&) = delete;

    void create(VkDevice device, VkPhysicalDevice physicalDevice,
                size_t size, const void* data = nullptr);

    void update(VkDevice device, const void* data, size_t size);

    void destroy();

    VkBuffer getBuffer() const { return buffer; }
    VkDeviceMemory getMemory() const { return memory; }
    size_t getSize() const { return size; }

private:
    uint32_t findMemoryType(VkPhysicalDevice physicalDevice,
                           uint32_t typeFilter,
                           VkMemoryPropertyFlags properties);

    VkDevice device = VK_NULL_HANDLE;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    size_t size = 0;
};
```

### Step 2: Implement StorageBuffer (add to renderer.cpp or separate file)

```cpp
#include "vkHelper.hpp"
#include <stdexcept>
#include <cstring>

void StorageBuffer::create(VkDevice device, VkPhysicalDevice physicalDevice,
                          size_t size, const void* data) {
    this->device = device;
    this->size = size;

    // Create buffer
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create storage buffer!");
    }

    // Allocate memory
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(
        physicalDevice,
        memRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate storage buffer memory!");
    }

    vkBindBufferMemory(device, buffer, memory, 0);

    // Upload data if provided
    if (data != nullptr) {
        update(device, data, size);
    }
}

void StorageBuffer::update(VkDevice device, const void* data, size_t size) {
    void* mapped;
    vkMapMemory(device, memory, 0, size, 0, &mapped);
    memcpy(mapped, data, size);
    vkUnmapMemory(device, memory);
}

void StorageBuffer::destroy() {
    if (device != VK_NULL_HANDLE) {
        if (buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, buffer, nullptr);
            buffer = VK_NULL_HANDLE;
        }
        if (memory != VK_NULL_HANDLE) {
            vkFreeMemory(device, memory, nullptr);
            memory = VK_NULL_HANDLE;
        }
    }
}

uint32_t StorageBuffer::findMemoryType(VkPhysicalDevice physicalDevice,
                                      uint32_t typeFilter,
                                      VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type!");
}
```

### Step 3: Add to Renderer class (src/renderer.hpp)

```cpp
class Renderer {
public:
    // ... existing methods

    void uploadHalfEdgeMesh(const HalfEdgeMesh& mesh);

private:
    // ... existing members

    // Half-edge mesh buffers
    std::vector<StorageBuffer> heVec4Buffers;  // Size 5
    std::vector<StorageBuffer> heVec2Buffers;  // Size 1
    std::vector<StorageBuffer> heIntBuffers;   // Size 10
    std::vector<StorageBuffer> heFloatBuffers; // Size 1

    VkDescriptorSet heDescriptorSet = VK_NULL_HANDLE;

    struct MeshInfoUBO {
        uint32_t nbVertices;
        uint32_t nbFaces;
        uint32_t nbHalfEdges;
        uint32_t padding;
    } meshInfo;

    VkBuffer meshInfoBuffer = VK_NULL_HANDLE;
    VkDeviceMemory meshInfoMemory = VK_NULL_HANDLE;
};
```

### Step 4: Implement uploadHalfEdgeMesh (src/renderer.cpp)

```cpp
void Renderer::uploadHalfEdgeMesh(const HalfEdgeMesh& mesh) {
    std::cout << "Uploading half-edge mesh to GPU..." << std::endl;

    // Allocate buffer arrays
    heVec4Buffers.resize(5);
    heVec2Buffers.resize(1);
    heIntBuffers.resize(10);
    heFloatBuffers.resize(1);

    // Upload vec4 buffers
    heVec4Buffers[0].create(device, physicalDevice,
        mesh.vertexPositions.size() * sizeof(glm::vec4),
        mesh.vertexPositions.data());

    heVec4Buffers[1].create(device, physicalDevice,
        mesh.vertexColors.size() * sizeof(glm::vec4),
        mesh.vertexColors.data());

    heVec4Buffers[2].create(device, physicalDevice,
        mesh.vertexNormals.size() * sizeof(glm::vec4),
        mesh.vertexNormals.data());

    heVec4Buffers[3].create(device, physicalDevice,
        mesh.faceNormals.size() * sizeof(glm::vec4),
        mesh.faceNormals.data());

    heVec4Buffers[4].create(device, physicalDevice,
        mesh.faceCenters.size() * sizeof(glm::vec4),
        mesh.faceCenters.data());

    // Upload vec2 buffers
    heVec2Buffers[0].create(device, physicalDevice,
        mesh.vertexTexCoords.size() * sizeof(glm::vec2),
        mesh.vertexTexCoords.data());

    // Upload int buffers
    heIntBuffers[0].create(device, physicalDevice,
        mesh.vertexEdges.size() * sizeof(int),
        mesh.vertexEdges.data());

    heIntBuffers[1].create(device, physicalDevice,
        mesh.faceEdges.size() * sizeof(int),
        mesh.faceEdges.data());

    heIntBuffers[2].create(device, physicalDevice,
        mesh.faceVertCounts.size() * sizeof(int),
        mesh.faceVertCounts.data());

    heIntBuffers[3].create(device, physicalDevice,
        mesh.faceOffsets.size() * sizeof(int),
        mesh.faceOffsets.data());

    heIntBuffers[4].create(device, physicalDevice,
        mesh.heVertex.size() * sizeof(int),
        mesh.heVertex.data());

    heIntBuffers[5].create(device, physicalDevice,
        mesh.heFace.size() * sizeof(int),
        mesh.heFace.data());

    heIntBuffers[6].create(device, physicalDevice,
        mesh.heNext.size() * sizeof(int),
        mesh.heNext.data());

    heIntBuffers[7].create(device, physicalDevice,
        mesh.hePrev.size() * sizeof(int),
        mesh.hePrev.data());

    heIntBuffers[8].create(device, physicalDevice,
        mesh.heTwin.size() * sizeof(int),
        mesh.heTwin.data());

    heIntBuffers[9].create(device, physicalDevice,
        mesh.vertexFaceIndices.size() * sizeof(int),
        mesh.vertexFaceIndices.data());

    // Upload float buffers
    heFloatBuffers[0].create(device, physicalDevice,
        mesh.faceAreas.size() * sizeof(float),
        mesh.faceAreas.data());

    // Create mesh info UBO
    meshInfo.nbVertices = mesh.nbVertices;
    meshInfo.nbFaces = mesh.nbFaces;
    meshInfo.nbHalfEdges = mesh.nbHalfEdges;
    meshInfo.padding = 0;

    // Create UBO (similar to StorageBuffer but with UNIFORM_BUFFER usage)
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = sizeof(MeshInfoUBO);
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vkCreateBuffer(device, &bufferInfo, nullptr, &meshInfoBuffer);

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, meshInfoBuffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = heVec4Buffers[0].findMemoryType(
        physicalDevice,
        memRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    vkAllocateMemory(device, &allocInfo, nullptr, &meshInfoMemory);
    vkBindBufferMemory(device, meshInfoBuffer, meshInfoMemory, 0);

    // Upload mesh info
    void* data;
    vkMapMemory(device, meshInfoMemory, 0, sizeof(MeshInfoUBO), 0, &data);
    memcpy(data, &meshInfo, sizeof(MeshInfoUBO));
    vkUnmapMemory(device, meshInfoMemory);

    // Update descriptor set 1 (HESet)
    updateHEDescriptorSet();

    std::cout << "✓ Half-edge mesh uploaded to GPU" << std::endl;
    std::cout << "  Total VRAM: " << calculateVRAM() / 1024.0f << " KB" << std::endl;
}

void Renderer::updateHEDescriptorSet() {
    std::vector<VkWriteDescriptorSet> writes;

    // Binding 0: vec4 buffers[5]
    std::vector<VkDescriptorBufferInfo> vec4BufferInfos(5);
    for (int i = 0; i < 5; ++i) {
        vec4BufferInfos[i].buffer = heVec4Buffers[i].getBuffer();
        vec4BufferInfos[i].offset = 0;
        vec4BufferInfos[i].range = heVec4Buffers[i].getSize();
    }

    VkWriteDescriptorSet vec4Write{};
    vec4Write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    vec4Write.dstSet = heDescriptorSet;
    vec4Write.dstBinding = 0;
    vec4Write.dstArrayElement = 0;
    vec4Write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    vec4Write.descriptorCount = 5;
    vec4Write.pBufferInfo = vec4BufferInfos.data();
    writes.push_back(vec4Write);

    // Binding 1: vec2 buffers[1]
    VkDescriptorBufferInfo vec2BufferInfo{};
    vec2BufferInfo.buffer = heVec2Buffers[0].getBuffer();
    vec2BufferInfo.offset = 0;
    vec2BufferInfo.range = heVec2Buffers[0].getSize();

    VkWriteDescriptorSet vec2Write{};
    vec2Write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    vec2Write.dstSet = heDescriptorSet;
    vec2Write.dstBinding = 1;
    vec2Write.dstArrayElement = 0;
    vec2Write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    vec2Write.descriptorCount = 1;
    vec2Write.pBufferInfo = &vec2BufferInfo;
    writes.push_back(vec2Write);

    // Binding 2: int buffers[10]
    std::vector<VkDescriptorBufferInfo> intBufferInfos(10);
    for (int i = 0; i < 10; ++i) {
        intBufferInfos[i].buffer = heIntBuffers[i].getBuffer();
        intBufferInfos[i].offset = 0;
        intBufferInfos[i].range = heIntBuffers[i].getSize();
    }

    VkWriteDescriptorSet intWrite{};
    intWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    intWrite.dstSet = heDescriptorSet;
    intWrite.dstBinding = 2;
    intWrite.dstArrayElement = 0;
    intWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    intWrite.descriptorCount = 10;
    intWrite.pBufferInfo = intBufferInfos.data();
    writes.push_back(intWrite);

    // Binding 3: float buffers[1]
    VkDescriptorBufferInfo floatBufferInfo{};
    floatBufferInfo.buffer = heFloatBuffers[0].getBuffer();
    floatBufferInfo.offset = 0;
    floatBufferInfo.range = heFloatBuffers[0].getSize();

    VkWriteDescriptorSet floatWrite{};
    floatWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    floatWrite.dstSet = heDescriptorSet;
    floatWrite.dstBinding = 3;
    floatWrite.dstArrayElement = 0;
    floatWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    floatWrite.descriptorCount = 1;
    floatWrite.pBufferInfo = &floatBufferInfo;
    writes.push_back(floatWrite);

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()),
                          writes.data(), 0, nullptr);
}

size_t Renderer::calculateVRAM() const {
    size_t total = 0;
    for (const auto& buf : heVec4Buffers) total += buf.getSize();
    for (const auto& buf : heVec2Buffers) total += buf.getSize();
    for (const auto& buf : heIntBuffers) total += buf.getSize();
    for (const auto& buf : heFloatBuffers) total += buf.getSize();
    total += sizeof(MeshInfoUBO);
    return total;
}
```

### Step 5: Test in main.cpp

```cpp
// After building half-edge mesh:
HalfEdgeMesh heMesh = HalfEdgeBuilder::build(ngonMesh);

// Upload to GPU
renderer.uploadHalfEdgeMesh(heMesh);
```

### Step 6: Build and Test

```bash
cd build
cmake ..
cmake --build .
./Gravel
```

## Acceptance Criteria

- [ ] All 17 SoA arrays uploaded successfully
- [ ] No Vulkan validation errors
- [ ] Descriptor set 1 updated with buffer handles
- [ ] MeshInfo UBO created and uploaded
- [ ] VRAM usage calculated and displayed
- [ ] No crashes or memory leaks
- [ ] Cleanup happens properly on shutdown

## Expected Output

```
Loading OBJ: assets/cube.obj
✓ Loaded OBJ: 8 vertices, 6 faces

Building half-edge structure...
  Vertices: 8
  Faces: 6
  Half-edges: 24
  Building twin edges...
  Boundary edges: 0
  Validating topology...
  ✓ Topology validation passed
✓ Half-edge structure built successfully

Uploading half-edge mesh to GPU...
✓ Half-edge mesh uploaded to GPU
  Total VRAM: 1.2 KB
```

## Troubleshooting

### Validation Errors on Descriptor Update
- Ensure descriptor set layout matches (from Epic 1 Feature 1.9)
- Check binding numbers are correct (0, 1, 2, 3)
- Verify array sizes match (vec4[5], vec2[1], int[10], float[1])

### Buffer Creation Failed
- Check device memory availability
- Verify memory type is available on GPU
- Ensure buffer sizes are > 0

### Large VRAM Usage
For cube: ~1.2 KB is expected
For icosphere (42 verts, 80 faces): ~5-10 KB expected

If much larger, check for:
- Duplicate data uploads
- Oversized buffers
- Memory leaks

## Common Pitfalls

1. **Buffer Array Indexing**: Ensure correct buffer goes to correct array index
2. **Memory Type**: Use HOST_VISIBLE | HOST_COHERENT for CPU→GPU transfer
3. **Descriptor Set Allocation**: heDescriptorSet must be allocated before update
4. **Buffer Size**: Ensure size > 0 before creating buffer
5. **Cleanup Order**: Destroy buffers before device

## Memory Layout

For cube mesh (8 vertices, 6 faces, 24 half-edges):

| Buffer | Type | Count | Size |
|--------|------|-------|------|
| heVec4Buffer[0] | positions | 8 | 128 B |
| heVec4Buffer[1] | colors | 8 | 128 B |
| heVec4Buffer[2] | normals | 8 | 128 B |
| heVec4Buffer[3] | face normals | 6 | 96 B |
| heVec4Buffer[4] | face centers | 6 | 96 B |
| heVec2Buffer[0] | texcoords | 8 | 64 B |
| heIntBuffer[0-9] | topology | varies | ~400 B |
| heFloatBuffer[0] | face areas | 6 | 24 B |
| **Total** | | | **~1.2 KB** |

## Performance Notes

- Upload time for cube: < 1ms
- Upload time for icosphere: ~1-2ms
- VRAM usage scales linearly with mesh complexity
- No performance impact on render loop (upload once)

## Next Feature

[Feature 2.4: Shared Shader Interface](feature-04-shader-interface.md)
