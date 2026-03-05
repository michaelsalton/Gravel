#include "renderer/MeshExport.h"
#include <stdexcept>
#include <iostream>

void MeshExportBuffers::allocate(VkDevice device, VkPhysicalDevice physDevice,
                                  uint32_t numVerts, uint32_t numTris,
                                  const std::vector<ExportElementOffset>& elementOffsets) {
    totalVertices = numVerts;
    totalTriangles = numTris;

    if (numVerts == 0 || numTris == 0) {
        throw std::runtime_error("MeshExportBuffers: zero vertices or triangles");
    }

    positions.create(device, physDevice, numVerts * sizeof(glm::vec4));
    normals.create(device, physDevice, numVerts * sizeof(glm::vec4));
    uvs.create(device, physDevice, numVerts * sizeof(glm::vec2));
    indices.create(device, physDevice, numTris * 3 * sizeof(uint32_t));
    offsets.create(device, physDevice,
                   elementOffsets.size() * sizeof(ExportElementOffset),
                   elementOffsets.data());

    std::cout << "Export buffers allocated: " << numVerts << " verts, "
              << numTris << " tris ("
              << (numVerts * (sizeof(glm::vec4) * 2 + sizeof(glm::vec2))
                  + numTris * 3 * sizeof(uint32_t)) / (1024 * 1024)
              << " MB)" << std::endl;
}

void MeshExportBuffers::destroy() {
    positions.destroy();
    normals.destroy();
    uvs.destroy();
    indices.destroy();
    offsets.destroy();
    totalVertices = 0;
    totalTriangles = 0;
}
