#include "HalfEdge.h"
#include "loaders/ObjLoader.h"
#include <map>
#include <iostream>
#include <stdexcept>

HalfEdgeMesh HalfEdgeBuilder::build(const NGonMesh& ngonMesh) {
    HalfEdgeMesh mesh;

    std::cout << "Building half-edge structure..." << std::endl;

    mesh.nbVertices = ngonMesh.nbVertices;
    mesh.nbFaces = ngonMesh.nbFaces;

    // Total half-edges = sum of all face vertex counts
    mesh.nbHalfEdges = 0;
    for (const auto& face : ngonMesh.faces) {
        mesh.nbHalfEdges += face.count;
    }

    std::cout << "  Vertices: " << mesh.nbVertices << std::endl;
    std::cout << "  Faces: " << mesh.nbFaces << std::endl;
    std::cout << "  Half-edges: " << mesh.nbHalfEdges << std::endl;

    // Allocate vertex arrays
    mesh.vertexPositions.resize(mesh.nbVertices);
    mesh.vertexColors.resize(mesh.nbVertices);
    mesh.vertexNormals.resize(mesh.nbVertices);
    mesh.vertexTexCoords.resize(mesh.nbVertices);
    mesh.vertexEdges.resize(mesh.nbVertices, -1);

    // Allocate face arrays
    mesh.faceEdges.resize(mesh.nbFaces);
    mesh.faceVertCounts.resize(mesh.nbFaces);
    mesh.faceOffsets.resize(mesh.nbFaces);
    mesh.faceNormals.resize(mesh.nbFaces);
    mesh.faceCenters.resize(mesh.nbFaces);
    mesh.faceAreas.resize(mesh.nbFaces);

    // Allocate half-edge arrays
    mesh.heVertex.resize(mesh.nbHalfEdges);
    mesh.heFace.resize(mesh.nbHalfEdges);
    mesh.heNext.resize(mesh.nbHalfEdges);
    mesh.hePrev.resize(mesh.nbHalfEdges);
    mesh.heTwin.resize(mesh.nbHalfEdges, -1);

    // Copy vertex data (convert to vec4 for GPU SoA layout)
    for (uint32_t i = 0; i < mesh.nbVertices; ++i) {
        mesh.vertexPositions[i] = glm::vec4(ngonMesh.positions[i], 1.0f);
        mesh.vertexColors[i] = glm::vec4(ngonMesh.colors[i], 1.0f);
        mesh.vertexNormals[i] = glm::vec4(ngonMesh.normals[i], 0.0f);
        mesh.vertexTexCoords[i] = ngonMesh.texCoords[i];
    }

    // Copy face data
    for (uint32_t i = 0; i < mesh.nbFaces; ++i) {
        mesh.faceVertCounts[i] = static_cast<int>(ngonMesh.faces[i].count);
        mesh.faceOffsets[i] = static_cast<int>(ngonMesh.faces[i].offset);
        mesh.faceNormals[i] = ngonMesh.faces[i].normal;
        mesh.faceCenters[i] = ngonMesh.faces[i].center;
        mesh.faceAreas[i] = ngonMesh.faces[i].area;
    }

    // Copy flattened face vertex indices
    mesh.vertexFaceIndices.assign(
        ngonMesh.faceVertexIndices.begin(),
        ngonMesh.faceVertexIndices.end()
    );

    // Build half-edges face by face
    std::map<std::pair<int, int>, int> edgeMap;
    int currentHE = 0;

    for (uint32_t faceId = 0; faceId < mesh.nbFaces; ++faceId) {
        const auto& face = ngonMesh.faces[faceId];
        int firstHE = currentHE;

        for (uint32_t i = 0; i < face.count; ++i) {
            int heId = currentHE;
            int v0 = static_cast<int>(face.vertexIndices[i]);
            int v1 = static_cast<int>(face.vertexIndices[(i + 1) % face.count]);

            mesh.heVertex[heId] = v0;
            mesh.heFace[heId] = static_cast<int>(faceId);

            // Next/prev within face loop
            mesh.heNext[heId] = (i == face.count - 1) ? firstHE : currentHE + 1;
            mesh.hePrev[heId] = (i == 0) ? firstHE + static_cast<int>(face.count) - 1 : currentHE - 1;

            // Store one outgoing edge per vertex
            if (mesh.vertexEdges[v0] == -1) {
                mesh.vertexEdges[v0] = heId;
            }

            // Register directed edge for twin lookup
            edgeMap[{v0, v1}] = heId;

            currentHE++;
        }

        mesh.faceEdges[faceId] = firstHE;
    }

    // Build twin relationships
    int boundaryEdges = 0;
    for (uint32_t heId = 0; heId < mesh.nbHalfEdges; ++heId) {
        int v0 = mesh.heVertex[heId];
        int v1 = mesh.heVertex[mesh.heNext[heId]];

        auto it = edgeMap.find({v1, v0});
        if (it != edgeMap.end()) {
            mesh.heTwin[heId] = it->second;
        } else {
            boundaryEdges++;
        }
    }

    std::cout << "  Boundary edges: " << boundaryEdges << std::endl;

    validateTopology(mesh);

    std::cout << "Half-edge structure built successfully" << std::endl;

    return mesh;
}

void HalfEdgeBuilder::validateTopology(const HalfEdgeMesh& mesh) {
    std::cout << "  Validating topology..." << std::endl;

    // Test 1: Next/prev loops close correctly
    for (uint32_t faceId = 0; faceId < mesh.nbFaces; ++faceId) {
        int edge = mesh.faceEdges[faceId];
        int start = edge;
        int count = 0;

        do {
            int next = mesh.heNext[edge];
            if (mesh.hePrev[next] != edge) {
                throw std::runtime_error(
                    "Invalid topology: prev(next(e)) != e at face " +
                    std::to_string(faceId));
            }
            edge = next;
            count++;
            if (count > static_cast<int>(mesh.nbHalfEdges)) {
                throw std::runtime_error(
                    "Invalid topology: infinite loop in face " +
                    std::to_string(faceId));
            }
        } while (edge != start);

        if (count != mesh.faceVertCounts[faceId]) {
            throw std::runtime_error(
                "Invalid topology: face " + std::to_string(faceId) +
                " loop count " + std::to_string(count) +
                " != expected " + std::to_string(mesh.faceVertCounts[faceId]));
        }
    }

    // Test 2: Twin symmetry
    int twinErrors = 0;
    for (uint32_t heId = 0; heId < mesh.nbHalfEdges; ++heId) {
        int twin = mesh.heTwin[heId];
        if (twin != -1) {
            if (mesh.heTwin[twin] != static_cast<int>(heId)) {
                twinErrors++;
            }
            // Verify reversed vertices
            int v0 = mesh.heVertex[heId];
            int v1 = mesh.heVertex[mesh.heNext[heId]];
            int tv0 = mesh.heVertex[twin];
            int tv1 = mesh.heVertex[mesh.heNext[twin]];
            if (v0 != tv1 || v1 != tv0) {
                twinErrors++;
            }
        }
    }
    if (twinErrors > 0) {
        throw std::runtime_error(
            "Invalid topology: " + std::to_string(twinErrors) + " twin errors");
    }

    // Test 3: Every vertex has a valid outgoing edge
    for (uint32_t v = 0; v < mesh.nbVertices; ++v) {
        int edge = mesh.vertexEdges[v];
        if (edge == -1) {
            throw std::runtime_error(
                "Invalid topology: vertex " + std::to_string(v) +
                " has no outgoing edge");
        }
        if (mesh.heVertex[edge] != static_cast<int>(v)) {
            throw std::runtime_error(
                "Invalid topology: vertex " + std::to_string(v) +
                " edge points to wrong vertex");
        }
    }

    std::cout << "  Topology validation passed" << std::endl;
}
