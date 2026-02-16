#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

struct NGonMesh; // Forward declaration

struct HalfEdgeMesh {
    uint32_t nbVertices = 0, nbFaces = 0, nbHalfEdges = 0;

    // Vertex SoA (size: nbVertices)
    std::vector<glm::vec4> vertexPositions;  // xyz = position, w = 1.0
    std::vector<glm::vec4> vertexColors;     // rgba
    std::vector<glm::vec4> vertexNormals;    // xyz = normal, w = 0.0
    std::vector<glm::vec2> vertexTexCoords;  // uv
    std::vector<int> vertexEdges;            // one outgoing half-edge per vertex

    // Face SoA (size: nbFaces)
    std::vector<int> faceEdges;              // one half-edge per face
    std::vector<int> faceVertCounts;         // polygon vertex count (3, 4, 5, ...)
    std::vector<int> faceOffsets;            // offset into vertexFaceIndices
    std::vector<glm::vec4> faceNormals;      // xyz = normal, w = 0.0
    std::vector<glm::vec4> faceCenters;      // xyz = center, w = 1.0
    std::vector<float> faceAreas;            // face area

    // Half-edge SoA (size: nbHalfEdges)
    std::vector<int> heVertex;   // origin vertex of this half-edge
    std::vector<int> heFace;     // adjacent face
    std::vector<int> heNext;     // next half-edge in face loop
    std::vector<int> hePrev;     // previous half-edge in face loop
    std::vector<int> heTwin;     // opposite half-edge (-1 if boundary)

    // Flattened face vertex indices (size: sum of all face vertex counts)
    std::vector<int> vertexFaceIndices;
};

class HalfEdgeBuilder {
public:
    static HalfEdgeMesh build(const NGonMesh& ngonMesh);

private:
    static void validateTopology(const HalfEdgeMesh& mesh);
};
