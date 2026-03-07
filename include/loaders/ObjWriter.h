#pragma once

#include <glm/glm.hpp>
#include <string>
#include <cstdint>

struct NGonMesh;

class ObjWriter {
public:
    static void write(const std::string& filepath,
                      const glm::vec4* positions,
                      const glm::vec4* normals,
                      const glm::vec2* uvs,
                      const uint32_t* indices,
                      uint32_t numVertices,
                      uint32_t numTriangles);

    // Append a triangulated NGonMesh to an existing OBJ file.
    // vertexOffset is the 1-based index offset for face indices.
    static void appendMesh(const std::string& filepath,
                           const NGonMesh& mesh,
                           uint32_t vertexOffset);
};
