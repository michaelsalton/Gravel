#pragma once

#include <glm/glm.hpp>
#include <string>
#include <cstdint>

class ObjWriter {
public:
    static void write(const std::string& filepath,
                      const glm::vec4* positions,
                      const glm::vec4* normals,
                      const glm::vec2* uvs,
                      const uint32_t* indices,
                      uint32_t numVertices,
                      uint32_t numTriangles);
};
