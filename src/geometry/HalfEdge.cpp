#include "geometry/HalfEdge.h"                   // HalfEdgeMesh, HalfEdgeBuilder, computeFace2Coloring declarations
#include "loaders/ObjLoader.h"                   // NGonMesh definition (the build() input)
#include <map>                                    // std::map for the directed-edge → half-edge lookup
#include <queue>                                  // std::queue for the BFS in face 2-coloring
#include <iostream>                               // Console logging
#include <stdexcept>                              // std::runtime_error for topology validation failures

// =============================================================================
// HalfEdge.cpp — Half-edge mesh builder, 2-coloring, and topology validation
//
// Implements HalfEdgeBuilder::build (NGonMesh → HalfEdgeMesh): copies vertex/
// face data into the SoA layout, creates the half-edges face by face, links
// next/prev within each face loop, and pairs twins via a directed-edge map.
// computeFace2Coloring runs a BFS over the dual graph to assign an alternating
// 0/1 color per face. validateTopology checks loop closure, twin symmetry, and
// vertex-edge consistency.
// =============================================================================

// *** Michael Salton ***

HalfEdgeMesh HalfEdgeBuilder::build(const NGonMesh& ngonMesh) { // Convert a loaded polygon mesh into a half-edge mesh
    HalfEdgeMesh mesh;                           // Output structure

    std::cout << "Building half-edge structure..." << std::endl; // Progress log

    mesh.nbVertices = ngonMesh.nbVertices;       // Copy vertex count
    mesh.nbFaces = ngonMesh.nbFaces;             // Copy face count

    // Total half-edges = sum of all face vertex counts
    mesh.nbHalfEdges = 0;                        // Accumulate the half-edge count
    for (const auto& face : ngonMesh.faces) {    // Each face contributes one half-edge per side
        mesh.nbHalfEdges += face.count;
    }

    std::cout << "  Vertices: " << mesh.nbVertices << std::endl;   // Log counts
    std::cout << "  Faces: " << mesh.nbFaces << std::endl;
    std::cout << "  Half-edges: " << mesh.nbHalfEdges << std::endl;

    // Allocate vertex arrays
    mesh.vertexPositions.resize(mesh.nbVertices); // Reserve per-vertex SoA arrays
    mesh.vertexColors.resize(mesh.nbVertices);
    mesh.vertexNormals.resize(mesh.nbVertices);
    mesh.vertexTexCoords.resize(mesh.nbVertices);
    mesh.vertexEdges.resize(mesh.nbVertices, -1); // -1 = vertex has no outgoing edge yet

    // Allocate face arrays
    mesh.faceEdges.resize(mesh.nbFaces);          // Reserve per-face SoA arrays
    mesh.faceVertCounts.resize(mesh.nbFaces);
    mesh.faceOffsets.resize(mesh.nbFaces);
    mesh.faceNormals.resize(mesh.nbFaces);
    mesh.faceCenters.resize(mesh.nbFaces);
    mesh.faceAreas.resize(mesh.nbFaces);

    // Allocate half-edge arrays
    mesh.heVertex.resize(mesh.nbHalfEdges);       // Reserve per-half-edge SoA arrays
    mesh.heFace.resize(mesh.nbHalfEdges);
    mesh.heNext.resize(mesh.nbHalfEdges);
    mesh.hePrev.resize(mesh.nbHalfEdges);
    mesh.heTwin.resize(mesh.nbHalfEdges, -1);     // -1 = boundary (no twin) until paired

    // Copy vertex data (convert to vec4 for GPU SoA layout)
    for (uint32_t i = 0; i < mesh.nbVertices; ++i) { // Promote vec3 attributes to vec4 for GPU alignment
        mesh.vertexPositions[i] = glm::vec4(ngonMesh.positions[i], 1.0f); // w=1 (point)
        mesh.vertexColors[i] = glm::vec4(ngonMesh.colors[i], 1.0f);       // opaque color
        mesh.vertexNormals[i] = glm::vec4(ngonMesh.normals[i], 0.0f);     // w=0 (direction)
        mesh.vertexTexCoords[i] = ngonMesh.texCoords[i];                  // uv as-is
    }

    // Copy face data
    for (uint32_t i = 0; i < mesh.nbFaces; ++i) { // Copy precomputed per-face attributes
        mesh.faceVertCounts[i] = static_cast<int>(ngonMesh.faces[i].count);  // Side count
        mesh.faceOffsets[i] = static_cast<int>(ngonMesh.faces[i].offset);    // Index offset
        mesh.faceNormals[i] = ngonMesh.faces[i].normal;                       // Normal
        mesh.faceCenters[i] = ngonMesh.faces[i].center;                       // Centroid
        mesh.faceAreas[i] = ngonMesh.faces[i].area;                           // Area
    }

    // Copy flattened face vertex indices
    mesh.vertexFaceIndices.assign(               // Copy the concatenated per-face index list
        ngonMesh.faceVertexIndices.begin(),
        ngonMesh.faceVertexIndices.end()
    );

    // Build half-edges face by face
    std::map<std::pair<int, int>, int> edgeMap;  // Maps directed edge (v0,v1) → half-edge id, for twin lookup
    int currentHE = 0;                           // Running half-edge index

    for (uint32_t faceId = 0; faceId < mesh.nbFaces; ++faceId) { // Walk every face
        const auto& face = ngonMesh.faces[faceId]; // Source face
        int firstHE = currentHE;                 // Index of this face's first half-edge (for loop wrap-around)

        for (uint32_t i = 0; i < face.count; ++i) { // One half-edge per side of the polygon
            int heId = currentHE;                // This half-edge's index
            int v0 = static_cast<int>(face.vertexIndices[i]); // Origin vertex of the edge
            int v1 = static_cast<int>(face.vertexIndices[(i + 1) % face.count]); // Destination vertex (wraps)

            mesh.heVertex[heId] = v0;            // Record the origin vertex
            mesh.heFace[heId] = static_cast<int>(faceId); // Record the bounding face

            // Next/prev within face loop
            mesh.heNext[heId] = (i == face.count - 1) ? firstHE : currentHE + 1; // Last edge loops back to first
            mesh.hePrev[heId] = (i == 0) ? firstHE + static_cast<int>(face.count) - 1 : currentHE - 1; // First edge's prev is last

            // Store one outgoing edge per vertex
            if (mesh.vertexEdges[v0] == -1) {    // First time we see this vertex...
                mesh.vertexEdges[v0] = heId;     // ...record an outgoing half-edge for it
            }

            // Register directed edge for twin lookup
            edgeMap[{v0, v1}] = heId;            // Remember this directed edge → half-edge

            currentHE++;                         // Advance to the next half-edge slot
        }

        mesh.faceEdges[faceId] = firstHE;        // Store the face's entry half-edge
    }

    // Build twin relationships
    int boundaryEdges = 0;                       // Count edges with no opposite (mesh boundary)
    for (uint32_t heId = 0; heId < mesh.nbHalfEdges; ++heId) { // For each half-edge...
        int v0 = mesh.heVertex[heId];            // Its origin vertex
        int v1 = mesh.heVertex[mesh.heNext[heId]]; // Its destination (origin of the next half-edge)

        auto it = edgeMap.find({v1, v0});        // Look for the oppositely-directed edge (v1→v0)
        if (it != edgeMap.end()) {               // If a matching half-edge exists...
            mesh.heTwin[heId] = it->second;      // ...pair them as twins
        } else {                                 // Otherwise...
            boundaryEdges++;                     // ...this is a boundary edge
        }
    }

    std::cout << "  Boundary edges: " << boundaryEdges << std::endl; // Log boundary count

    validateTopology(mesh);                      // Verify the built connectivity is consistent

    std::cout << "Half-edge structure built successfully" << std::endl;

    return mesh;                                 // Return the completed half-edge mesh
}

void computeFace2Coloring(HalfEdgeMesh& mesh) {  // Assign each face a 0/1 color so neighbors differ (checkerboard)
    std::vector<int> color(mesh.nbFaces, -1);    // -1 = uncolored
    int conflicts = 0;                           // Count of same-color adjacencies (non-bipartite meshes)

    for (uint32_t startFace = 0; startFace < mesh.nbFaces; ++startFace) { // Handle every connected component
        if (color[startFace] != -1) continue;    // Skip faces already colored by an earlier BFS

        color[startFace] = 0;                    // Seed this component with color 0
        std::queue<uint32_t> q;                  // BFS frontier
        q.push(startFace);

        while (!q.empty()) {                     // Standard BFS over adjacent faces
            uint32_t face = q.front();           // Current face
            q.pop();

            int edge = mesh.faceEdges[face];     // Start walking this face's half-edge loop
            int start = edge;                    // Remember the start to detect loop completion
            do {
                int twin = mesh.heTwin[edge];    // The half-edge across this edge
                if (twin != -1) {                // If there's a neighboring face...
                    uint32_t neighbor = static_cast<uint32_t>(mesh.heFace[twin]); // The adjacent face
                    if (color[neighbor] == -1) { // Uncolored neighbor...
                        color[neighbor] = 1 - color[face]; // ...give it the opposite color
                        q.push(neighbor);        // ...and enqueue it
                    } else if (color[neighbor] == color[face]) { // Already colored the same...
                        conflicts++;             // ...a 2-coloring conflict (odd cycle)
                    }
                }
                edge = mesh.heNext[edge];        // Advance around the face
            } while (edge != start);             // Until the loop closes
        }
    }

    for (uint32_t i = 0; i < mesh.nbFaces; ++i) { // Store the color in the unused w of each face normal
        mesh.faceNormals[i].w = static_cast<float>(color[i]);
    }

    std::cout << "  Face 2-coloring: " << mesh.nbFaces << " faces, " // Log result
              << conflicts << " conflicts" << std::endl;
}

void HalfEdgeBuilder::validateTopology(const HalfEdgeMesh& mesh) { // Sanity-check the built connectivity
    std::cout << "  Validating topology..." << std::endl;

    // Test 1: Next/prev loops close correctly
    for (uint32_t faceId = 0; faceId < mesh.nbFaces; ++faceId) { // For each face...
        int edge = mesh.faceEdges[faceId];       // Start of its half-edge loop
        int start = edge;                        // Loop-start marker
        int count = 0;                           // Edges visited in this loop

        do {
            int next = mesh.heNext[edge];        // Next half-edge in the loop
            if (mesh.hePrev[next] != edge) {     // prev(next(e)) must equal e (consistent links)
                throw std::runtime_error(
                    "Invalid topology: prev(next(e)) != e at face " +
                    std::to_string(faceId));
            }
            edge = next;                         // Advance
            count++;                             // Count this edge
            if (count > static_cast<int>(mesh.nbHalfEdges)) { // Guard against a broken (infinite) loop
                throw std::runtime_error(
                    "Invalid topology: infinite loop in face " +
                    std::to_string(faceId));
            }
        } while (edge != start);                 // Until back to the start

        if (count != mesh.faceVertCounts[faceId]) { // Loop length must match the face's side count
            throw std::runtime_error(
                "Invalid topology: face " + std::to_string(faceId) +
                " loop count " + std::to_string(count) +
                " != expected " + std::to_string(mesh.faceVertCounts[faceId]));
        }
    }

    // Test 2: Twin symmetry
    int twinErrors = 0;                          // Count of inconsistent twin pairings
    for (uint32_t heId = 0; heId < mesh.nbHalfEdges; ++heId) { // For each half-edge...
        int twin = mesh.heTwin[heId];            // Its twin (if any)
        if (twin != -1) {                        // Only check paired edges
            if (mesh.heTwin[twin] != static_cast<int>(heId)) { // Twin's twin must point back
                twinErrors++;
            }
            // Verify reversed vertices
            int v0 = mesh.heVertex[heId];        // This edge's origin...
            int v1 = mesh.heVertex[mesh.heNext[heId]]; // ...and destination
            int tv0 = mesh.heVertex[twin];       // Twin's origin...
            int tv1 = mesh.heVertex[mesh.heNext[twin]]; // ...and destination
            if (v0 != tv1 || v1 != tv0) {        // Twin must run in the opposite direction
                twinErrors++;
            }
        }
    }
    if (twinErrors > 0) {                        // Non-fatal: warn about non-manifold edges
        std::cout << "  Warning: " << twinErrors << " twin errors (non-manifold edges)" << std::endl;
    }

    // Test 3: Every referenced vertex has a valid outgoing edge
    int isolatedVertices = 0;                    // Count of vertices not used by any face
    for (uint32_t v = 0; v < mesh.nbVertices; ++v) { // For each vertex...
        int edge = mesh.vertexEdges[v];          // Its stored outgoing half-edge
        if (edge == -1) {                        // No edge → unreferenced vertex
            isolatedVertices++;
            continue;
        }
        if (mesh.heVertex[edge] != static_cast<int>(v)) { // The edge must actually originate at this vertex
            throw std::runtime_error(
                "Invalid topology: vertex " + std::to_string(v) +
                " edge points to wrong vertex");
        }
    }
    if (isolatedVertices > 0) {                  // Non-fatal: warn about isolated vertices
        std::cout << "  Warning: " << isolatedVertices << " isolated vertices (not referenced by any face)" << std::endl;
    }

    std::cout << "  Topology validation passed" << std::endl; // All checks done
}

// *** ************ ***
