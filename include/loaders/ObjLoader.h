#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <cstdint>

struct NGonFace {
    std::vector<uint32_t> vertexIndices;
    std::vector<uint32_t> normalIndices;
    std::vector<uint32_t> texCoordIndices;

    glm::vec4 normal;       // Computed face normal (w = 0)
    glm::vec4 center;       // Computed face centroid (w = 1)
    float area;             // Computed face area
    uint32_t offset;        // Offset into flattened index array
    uint32_t count;         // Vertex count (3, 4, 5, ...)
};

struct NGonMesh {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texCoords;
    std::vector<glm::vec3> colors;
    std::vector<NGonFace> faces;
    std::vector<uint32_t> faceVertexIndices;

    uint32_t nbVertices = 0;
    uint32_t nbFaces = 0;
};

class ObjLoader {
public:
    static NGonMesh load(const std::string& filepath);

private:
    static glm::vec3 computeFaceNormal(
        const std::vector<glm::vec3>& positions,
        const std::vector<uint32_t>& indices);

    static glm::vec3 computeFaceCentroid(
        const std::vector<glm::vec3>& positions,
        const std::vector<uint32_t>& indices);

    static float computeFaceArea(
        const std::vector<glm::vec3>& positions,
        const std::vector<uint32_t>& indices);
};
