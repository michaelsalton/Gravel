#pragma once                                     // Include this header only once per translation unit

#include <glm/glm.hpp>                           // glm vector types (vec2, vec3, vec4)
#include <vector>                                // std::vector for the mesh arrays
#include <string>                                // std::string for the file path
#include <cstdint>                               // Fixed-width integer types (uint32_t)

// =============================================================================
// ObjLoader.h — Wavefront OBJ loading into an n-gon mesh
//
// Loads .obj files into an NGonMesh: arbitrary polygons (not just triangles)
// with unified per-vertex position/normal/texcoord/color arrays. The loader
// splits vertices at UV/normal seams and precomputes per-face normal, centroid,
// and area. NGonMesh is the input to HalfEdgeBuilder. Also offers triangulation
// and (AI-generated) subdivision utilities.
// =============================================================================

// *** Michael Salton ***

struct NGonFace {                                // A single polygon face (3+ vertices)
    std::vector<uint32_t> vertexIndices;         // Position indices into NGonMesh.positions
    std::vector<uint32_t> normalIndices;         // Normal indices (only during parsing; cleared after remap)
    std::vector<uint32_t> texCoordIndices;       // Texcoord indices (only during parsing; cleared after remap)

    glm::vec4 normal;       // Computed face normal (w = 0)   // Geometric normal of the face
    glm::vec4 center;       // Computed face centroid (w = 1) // Average of the face's vertex positions
    float area;             // Computed face area              // Polygon area (fan-summed)
    uint32_t offset;        // Offset into flattened index array // Start of this face's indices in faceVertexIndices
    uint32_t count;         // Vertex count (3, 4, 5, ...)     // Number of sides
};

struct NGonMesh {                                // A loaded polygon mesh with unified per-vertex arrays
    std::vector<glm::vec3> positions;            // Per-vertex positions
    std::vector<glm::vec3> normals;              // Per-vertex normals
    std::vector<glm::vec2> texCoords;            // Per-vertex texture coordinates
    std::vector<glm::vec3> colors;               // Per-vertex colors (defaulted to white)
    std::vector<NGonFace> faces;                 // All polygon faces
    std::vector<uint32_t> faceVertexIndices;     // All faces' vertex indices concatenated (flat array)

    // Maps each (possibly split) vertex back to its original OBJ position index.
    // Used to remap per-vertex data from external tools (e.g. GRWM curvature).
    std::vector<uint32_t> originalVertexIndices; // unified vertex → original OBJ position index
    uint32_t originalVertexCount = 0;            // Number of distinct OBJ positions before seam-splitting

    uint32_t nbVertices = 0;                     // Final vertex count (after splitting)
    uint32_t nbFaces = 0;                        // Face count
};

class ObjLoader {                                // Stateless OBJ-loading and mesh-processing utility
public:
    static NGonMesh load(const std::string& filepath); // Parse an OBJ file into an NGonMesh
    static void triangulate(NGonMesh& mesh);     // Fan-triangulate all faces in place
    static void subdivide(NGonMesh& mesh, int levels = 1); // AI Generated
    static void subdivideFlat(NGonMesh& mesh, int levels = 1); // AI Generated

private:
    static glm::vec3 computeFaceNormal(          // Geometric normal from the first three vertices
        const std::vector<glm::vec3>& positions,
        const std::vector<uint32_t>& indices);

    static glm::vec3 computeFaceCentroid(        // Average of the face's vertex positions
        const std::vector<glm::vec3>& positions,
        const std::vector<uint32_t>& indices);

    static float computeFaceArea(                // Polygon area via a triangle fan
        const std::vector<glm::vec3>& positions,
        const std::vector<uint32_t>& indices);
};

// *** ************ ***
