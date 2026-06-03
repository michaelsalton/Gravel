#pragma once                                     // Include this header only once per translation unit

#include <glm/glm.hpp>                           // glm vector/matrix types (vec2, vec4)
#include <vector>                                // std::vector for the SoA arrays
#include <cstdint>                               // Fixed-width integer types (uint32_t)

// =============================================================================
// HalfEdge.h — Half-edge mesh data structure and builder
//
// Defines the GPU-friendly half-edge representation the resurfacing/pebble
// pipelines consume. The mesh is stored as struct-of-arrays (SoA) so each field
// uploads directly into a GPU storage buffer. HalfEdgeBuilder converts a loaded
// NGonMesh (arbitrary polygons) into this structure, wiring up the per-vertex,
// per-face, and per-half-edge connectivity and validating the topology.
// computeFace2Coloring adds a checkerboard color used for chainmail tilt.
// =============================================================================

// *** Michael Salton ***

struct NGonMesh; // Forward declaration             // Defined in the OBJ loader; only referenced by pointer/ref here

struct HalfEdgeMesh {                            // GPU-ready half-edge mesh (struct-of-arrays)
    uint32_t nbVertices = 0, nbFaces = 0, nbHalfEdges = 0; // Element counts for the three SoA groups

    // Vertex SoA (size: nbVertices)
    std::vector<glm::vec4> vertexPositions;  // xyz = position, w = 1.0   // Per-vertex position (w=1 for point transforms)
    std::vector<glm::vec4> vertexColors;     // rgba                      // Per-vertex color
    std::vector<glm::vec4> vertexNormals;    // xyz = normal, w = 0.0     // Per-vertex normal (w=0 for direction transforms)
    std::vector<glm::vec2> vertexTexCoords;  // uv                        // Per-vertex texture coordinates
    std::vector<int> vertexEdges;            // one outgoing half-edge per vertex // Entry point into the connectivity per vertex

    // Face SoA (size: nbFaces)
    std::vector<int> faceEdges;              // one half-edge per face    // Entry half-edge for each face's loop
    std::vector<int> faceVertCounts;         // polygon vertex count (3, 4, 5, ...) // Number of sides per face
    std::vector<int> faceOffsets;            // offset into vertexFaceIndices // Where each face's indices begin in the flat array
    std::vector<glm::vec4> faceNormals;      // xyz = normal, w = 0.0     // Face normal; w is reused for the 2-coloring value
    std::vector<glm::vec4> faceCenters;      // xyz = center, w = 1.0     // Face centroid
    std::vector<float> faceAreas;            // face area                 // Face area (drives element sizing/LOD)

    // Half-edge SoA (size: nbHalfEdges)
    std::vector<int> heVertex;   // origin vertex of this half-edge       // Vertex each half-edge starts from
    std::vector<int> heFace;     // adjacent face                         // Face each half-edge bounds
    std::vector<int> heNext;     // next half-edge in face loop           // Successor around the face
    std::vector<int> hePrev;     // previous half-edge in face loop       // Predecessor around the face
    std::vector<int> heTwin;     // opposite half-edge (-1 if boundary)   // Paired half-edge on the adjacent face

    // Flattened face vertex indices (size: sum of all face vertex counts)
    std::vector<int> vertexFaceIndices;      // All faces' vertex indices concatenated (indexed via faceOffsets)
};

class HalfEdgeBuilder {                          // Converts an NGonMesh into a HalfEdgeMesh
public:
    static HalfEdgeMesh build(const NGonMesh& ngonMesh); // Build and return the half-edge structure

private:
    static void validateTopology(const HalfEdgeMesh& mesh); // Sanity-check connectivity (loops, twins, vertex edges)
};

/// BFS-based face 2-coloring on the dual graph.
/// Stores color (0.0 or 1.0) in faceNormals[i].w.
/// For bipartite graphs (quad meshes), produces a perfect 2-coloring.
/// For non-bipartite graphs (triangle meshes), uses greedy BFS with conflict tolerance.
void computeFace2Coloring(HalfEdgeMesh& mesh);   // Assign each face a 0/1 checkerboard color (used for chainmail tilt)

// *** ************ ***
