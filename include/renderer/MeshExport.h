#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <cstdint>

#include "vulkan/vkHelper.h"

struct ExportElementOffset {
    uint32_t vertexOffset;
    uint32_t triangleOffset;
    uint32_t faceId;
    uint32_t isVertex;
};

struct MeshExportBuffers {
    StorageBuffer positions;   // vec4[totalVertices]
    StorageBuffer normals;     // vec4[totalVertices]
    StorageBuffer uvs;         // vec2[totalVertices]
    StorageBuffer indices;     // uint[totalTriangles * 3]
    StorageBuffer offsets;     // ExportElementOffset[numElements]

    uint32_t totalVertices = 0;
    uint32_t totalTriangles = 0;

    void allocate(VkDevice device, VkPhysicalDevice physDevice,
                  uint32_t numVerts, uint32_t numTris,
                  const std::vector<ExportElementOffset>& elementOffsets);
    void destroy();
};
