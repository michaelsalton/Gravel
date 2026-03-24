#include "loaders/ObjLoader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <map>
#include <set>

NGonMesh ObjLoader::load(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open OBJ file: " + filepath);
    }

    NGonMesh mesh;
    std::string line;
    uint32_t faceVertexOffset = 0;

    std::cout << "Loading OBJ: " << filepath << std::endl;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        std::string prefix;
        iss >> prefix;

        if (prefix == "v") {
            glm::vec3 pos;
            iss >> pos.x >> pos.y >> pos.z;
            mesh.positions.push_back(pos);

        } else if (prefix == "vn") {
            glm::vec3 normal;
            iss >> normal.x >> normal.y >> normal.z;
            mesh.normals.push_back(glm::normalize(normal));

        } else if (prefix == "vt") {
            glm::vec2 texCoord;
            iss >> texCoord.x >> texCoord.y;
            mesh.texCoords.push_back(texCoord);

        } else if (prefix == "f") {
            NGonFace face;
            std::string token;

            while (iss >> token) {
                uint32_t vIdx = 0, vtIdx = 0, vnIdx = 0;

                // Parse face vertex: v, v/vt, v/vt/vn, or v//vn
                size_t firstSlash = token.find('/');
                if (firstSlash == std::string::npos) {
                    // Format: v
                    vIdx = static_cast<uint32_t>(std::stoi(token));
                } else {
                    vIdx = static_cast<uint32_t>(std::stoi(token.substr(0, firstSlash)));
                    size_t secondSlash = token.find('/', firstSlash + 1);

                    if (secondSlash == std::string::npos) {
                        // Format: v/vt
                        vtIdx = static_cast<uint32_t>(std::stoi(token.substr(firstSlash + 1)));
                    } else if (secondSlash == firstSlash + 1) {
                        // Format: v//vn
                        vnIdx = static_cast<uint32_t>(std::stoi(token.substr(secondSlash + 1)));
                    } else {
                        // Format: v/vt/vn
                        vtIdx = static_cast<uint32_t>(std::stoi(
                            token.substr(firstSlash + 1, secondSlash - firstSlash - 1)));
                        vnIdx = static_cast<uint32_t>(std::stoi(token.substr(secondSlash + 1)));
                    }
                }

                // OBJ is 1-based, convert to 0-based
                face.vertexIndices.push_back(vIdx - 1);
                if (vtIdx > 0) face.texCoordIndices.push_back(vtIdx - 1);
                if (vnIdx > 0) face.normalIndices.push_back(vnIdx - 1);
            }

            if (face.vertexIndices.size() < 3) continue;

            face.count = static_cast<uint32_t>(face.vertexIndices.size());
            face.offset = faceVertexOffset;
            face.normal = glm::vec4(computeFaceNormal(mesh.positions, face.vertexIndices), 0.0f);
            face.center = glm::vec4(computeFaceCentroid(mesh.positions, face.vertexIndices), 1.0f);
            face.area = computeFaceArea(mesh.positions, face.vertexIndices);

            for (uint32_t idx : face.vertexIndices) {
                mesh.faceVertexIndices.push_back(idx);
            }
            faceVertexOffset += face.count;

            mesh.faces.push_back(face);
        }
    }

    // Remap normals and texcoords from OBJ indices to per-vertex arrays
    // OBJ can have different counts for v/vn/vt, indexed independently per face
    std::vector<glm::vec3> rawNormals = std::move(mesh.normals);
    std::vector<glm::vec2> rawTexCoords = std::move(mesh.texCoords);
    mesh.normals.resize(mesh.positions.size(), glm::vec3(0.0f, 0.0f, 1.0f));
    mesh.texCoords.resize(mesh.positions.size(), glm::vec2(0.0f));

    for (const auto& face : mesh.faces) {
        for (uint32_t i = 0; i < face.vertexIndices.size(); ++i) {
            uint32_t vIdx = face.vertexIndices[i];
            if (i < face.normalIndices.size() && face.normalIndices[i] < rawNormals.size()) {
                mesh.normals[vIdx] = rawNormals[face.normalIndices[i]];
            }
            if (i < face.texCoordIndices.size() && face.texCoordIndices[i] < rawTexCoords.size()) {
                mesh.texCoords[vIdx] = rawTexCoords[face.texCoordIndices[i]];
            }
        }
    }
    mesh.colors.resize(mesh.positions.size(), glm::vec3(1.0f));

    mesh.nbVertices = static_cast<uint32_t>(mesh.positions.size());
    mesh.nbFaces = static_cast<uint32_t>(mesh.faces.size());

    // Print face type distribution
    int triCount = 0, quadCount = 0, ngonCount = 0;
    for (const auto& face : mesh.faces) {
        if (face.count == 3) triCount++;
        else if (face.count == 4) quadCount++;
        else ngonCount++;
    }

    std::cout << "Loaded OBJ: " << mesh.nbVertices << " vertices, "
              << mesh.nbFaces << " faces" << std::endl;
    std::cout << "  Triangles: " << triCount
              << ", Quads: " << quadCount
              << ", N-gons: " << ngonCount << std::endl;

    return mesh;
}

void ObjLoader::triangulate(NGonMesh& mesh) {
    std::vector<NGonFace> newFaces;
    std::vector<uint32_t> newFaceVertexIndices;
    uint32_t offset = 0;

    for (const auto& face : mesh.faces) {
        if (face.count <= 3) {
            // Already a triangle, keep as-is
            NGonFace tri = face;
            tri.offset = offset;
            for (uint32_t idx : tri.vertexIndices) {
                newFaceVertexIndices.push_back(idx);
            }
            offset += tri.count;
            newFaces.push_back(tri);
        } else {
            // Fan-triangulate: vertex 0, i+1, i+2
            for (uint32_t i = 0; i < face.count - 2; i++) {
                NGonFace tri;
                tri.vertexIndices = {
                    face.vertexIndices[0],
                    face.vertexIndices[i + 1],
                    face.vertexIndices[i + 2]
                };
                tri.count = 3;
                tri.offset = offset;
                tri.normal = glm::vec4(computeFaceNormal(mesh.positions, tri.vertexIndices), 0.0f);
                tri.center = glm::vec4(computeFaceCentroid(mesh.positions, tri.vertexIndices), 1.0f);
                tri.area = computeFaceArea(mesh.positions, tri.vertexIndices);
                for (uint32_t idx : tri.vertexIndices) {
                    newFaceVertexIndices.push_back(idx);
                }
                offset += 3;
                newFaces.push_back(tri);
            }
        }
    }

    mesh.faces = std::move(newFaces);
    mesh.faceVertexIndices = std::move(newFaceVertexIndices);
    mesh.nbFaces = static_cast<uint32_t>(mesh.faces.size());

    std::cout << "Triangulated: " << mesh.nbFaces << " triangles" << std::endl;
}

glm::vec3 ObjLoader::computeFaceNormal(
    const std::vector<glm::vec3>& positions,
    const std::vector<uint32_t>& indices) {

    if (indices.size() < 3) {
        return glm::vec3(0.0f, 0.0f, 1.0f);
    }

    glm::vec3 v0 = positions[indices[0]];
    glm::vec3 v1 = positions[indices[1]];
    glm::vec3 v2 = positions[indices[2]];

    glm::vec3 normal = glm::cross(v1 - v0, v2 - v0);
    float len = glm::length(normal);

    if (len > 0.0f) {
        return normal / len;
    }
    return glm::vec3(0.0f, 0.0f, 1.0f);
}

glm::vec3 ObjLoader::computeFaceCentroid(
    const std::vector<glm::vec3>& positions,
    const std::vector<uint32_t>& indices) {

    glm::vec3 centroid(0.0f);
    for (uint32_t idx : indices) {
        centroid += positions[idx];
    }
    return centroid / static_cast<float>(indices.size());
}

float ObjLoader::computeFaceArea(
    const std::vector<glm::vec3>& positions,
    const std::vector<uint32_t>& indices) {

    if (indices.size() < 3) return 0.0f;

    // Triangle fan from first vertex
    float totalArea = 0.0f;
    glm::vec3 v0 = positions[indices[0]];

    for (size_t i = 1; i < indices.size() - 1; ++i) {
        glm::vec3 edge1 = positions[indices[i]] - v0;
        glm::vec3 edge2 = positions[indices[i + 1]] - v0;
        totalArea += glm::length(glm::cross(edge1, edge2)) * 0.5f;
    }

    return totalArea;
}

void ObjLoader::subdivide(NGonMesh& mesh, int levels) {
    using Edge = std::pair<uint32_t, uint32_t>;
    auto makeEdge = [](uint32_t a, uint32_t b) -> Edge {
        return a < b ? Edge{a, b} : Edge{b, a};
    };

    for (int lvl = 0; lvl < levels; lvl++) {
        uint32_t nVerts = static_cast<uint32_t>(mesh.positions.size());
        uint32_t nFaces = static_cast<uint32_t>(mesh.faces.size());

        // Ensure arrays match
        mesh.normals.resize(nVerts, glm::vec3(0, 1, 0));
        mesh.texCoords.resize(nVerts, glm::vec2(0));
        mesh.colors.resize(nVerts, glm::vec3(1));

        // === Step 1: Build adjacency ===
        std::map<Edge, std::vector<uint32_t>> edgeToFaces;
        std::vector<std::vector<uint32_t>> vertexToFaces(nVerts);
        std::vector<std::set<uint32_t>> vertexNeighbors(nVerts);

        for (uint32_t fi = 0; fi < nFaces; fi++) {
            const auto& vi = mesh.faces[fi].vertexIndices;
            uint32_t n = mesh.faces[fi].count;
            for (uint32_t i = 0; i < n; i++) {
                uint32_t v0 = vi[i], v1 = vi[(i + 1) % n];
                vertexToFaces[v0].push_back(fi);
                vertexNeighbors[v0].insert(v1);
                vertexNeighbors[v1].insert(v0);
                edgeToFaces[makeEdge(v0, v1)].push_back(fi);
            }
        }

        // === Step 2: Compute face points ===
        std::vector<glm::vec3> facePoints(nFaces);
        std::vector<glm::vec2> facePointUVs(nFaces);
        std::vector<glm::vec3> facePointColors(nFaces);

        for (uint32_t fi = 0; fi < nFaces; fi++) {
            const auto& vi = mesh.faces[fi].vertexIndices;
            uint32_t n = mesh.faces[fi].count;
            glm::vec3 p(0), c(0); glm::vec2 uv(0);
            for (uint32_t i = 0; i < n; i++) {
                p += mesh.positions[vi[i]];
                uv += mesh.texCoords[vi[i]];
                c += mesh.colors[vi[i]];
            }
            float inv = 1.0f / float(n);
            facePoints[fi] = p * inv;
            facePointUVs[fi] = uv * inv;
            facePointColors[fi] = c * inv;
        }

        // === Step 3: Compute edge points ===
        std::map<Edge, glm::vec3> edgePointPos;
        std::map<Edge, glm::vec2> edgePointUV;
        std::map<Edge, glm::vec3> edgePointCol;
        std::map<Edge, bool> edgeIsBoundary;

        for (auto& [edge, faces] : edgeToFaces) {
            glm::vec3 v0 = mesh.positions[edge.first];
            glm::vec3 v1 = mesh.positions[edge.second];
            glm::vec2 uv0 = mesh.texCoords[edge.first];
            glm::vec2 uv1 = mesh.texCoords[edge.second];
            glm::vec3 c0 = mesh.colors[edge.first];
            glm::vec3 c1 = mesh.colors[edge.second];

            if (faces.size() == 2) {
                // Interior edge: average of endpoints + adjacent face points
                glm::vec3 fp = facePoints[faces[0]] + facePoints[faces[1]];
                edgePointPos[edge] = (v0 + v1 + fp) * 0.25f;
                edgePointUV[edge] = (uv0 + uv1 + facePointUVs[faces[0]] + facePointUVs[faces[1]]) * 0.25f;
                edgePointCol[edge] = (c0 + c1 + facePointColors[faces[0]] + facePointColors[faces[1]]) * 0.25f;
                edgeIsBoundary[edge] = false;
            } else {
                // Boundary edge: simple midpoint
                edgePointPos[edge] = (v0 + v1) * 0.5f;
                edgePointUV[edge] = (uv0 + uv1) * 0.5f;
                edgePointCol[edge] = (c0 + c1) * 0.5f;
                edgeIsBoundary[edge] = true;
            }
        }

        // === Step 4: Compute smoothed vertex positions ===
        std::vector<glm::vec3> smoothedPos(nVerts);
        std::vector<glm::vec2> smoothedUV(nVerts);
        std::vector<glm::vec3> smoothedCol(nVerts);

        for (uint32_t vi = 0; vi < nVerts; vi++) {
            uint32_t n = static_cast<uint32_t>(vertexNeighbors[vi].size());
            if (n == 0) {
                smoothedPos[vi] = mesh.positions[vi];
                smoothedUV[vi] = mesh.texCoords[vi];
                smoothedCol[vi] = mesh.colors[vi];
                continue;
            }

            // Check if boundary vertex
            bool isBoundary = false;
            std::vector<uint32_t> boundaryNeighbors;
            for (uint32_t nb : vertexNeighbors[vi]) {
                Edge e = makeEdge(vi, nb);
                if (edgeToFaces[e].size() == 1) {
                    isBoundary = true;
                    boundaryNeighbors.push_back(nb);
                }
            }

            if (isBoundary && boundaryNeighbors.size() == 2) {
                // Boundary vertex: (left + 6*V + right) / 8
                smoothedPos[vi] = (mesh.positions[boundaryNeighbors[0]] +
                                   6.0f * mesh.positions[vi] +
                                   mesh.positions[boundaryNeighbors[1]]) / 8.0f;
                smoothedUV[vi] = (mesh.texCoords[boundaryNeighbors[0]] +
                                  6.0f * mesh.texCoords[vi] +
                                  mesh.texCoords[boundaryNeighbors[1]]) / 8.0f;
                smoothedCol[vi] = (mesh.colors[boundaryNeighbors[0]] +
                                   6.0f * mesh.colors[vi] +
                                   mesh.colors[boundaryNeighbors[1]]) / 8.0f;
            } else {
                // Interior vertex: Q/n + 2R/n + (n-3)*V/n
                glm::vec3 Q(0); glm::vec2 Quv(0); glm::vec3 Qc(0);
                for (uint32_t fi : vertexToFaces[vi]) {
                    Q += facePoints[fi];
                    Quv += facePointUVs[fi];
                    Qc += facePointColors[fi];
                }
                float nf = float(vertexToFaces[vi].size());
                Q /= nf; Quv /= nf; Qc /= nf;

                glm::vec3 R(0); glm::vec2 Ruv(0); glm::vec3 Rc(0);
                for (uint32_t nb : vertexNeighbors[vi]) {
                    R += (mesh.positions[vi] + mesh.positions[nb]) * 0.5f;
                    Ruv += (mesh.texCoords[vi] + mesh.texCoords[nb]) * 0.5f;
                    Rc += (mesh.colors[vi] + mesh.colors[nb]) * 0.5f;
                }
                float ne = float(n);
                R /= ne; Ruv /= ne; Rc /= ne;

                smoothedPos[vi] = Q / ne + 2.0f * R / ne + (ne - 3.0f) * mesh.positions[vi] / ne;
                smoothedUV[vi] = Quv / ne + 2.0f * Ruv / ne + (ne - 3.0f) * mesh.texCoords[vi] / ne;
                smoothedCol[vi] = Qc / ne + 2.0f * Rc / ne + (ne - 3.0f) * mesh.colors[vi] / ne;
            }
        }

        // === Step 5: Build output mesh ===
        NGonMesh result;

        // Add smoothed original vertices
        for (uint32_t i = 0; i < nVerts; i++) {
            result.positions.push_back(smoothedPos[i]);
            result.normals.push_back(mesh.normals[i]);  // recomputed later
            result.texCoords.push_back(smoothedUV[i]);
            result.colors.push_back(smoothedCol[i]);
        }

        // Add face point vertices
        std::vector<uint32_t> facePointIdx(nFaces);
        for (uint32_t fi = 0; fi < nFaces; fi++) {
            facePointIdx[fi] = static_cast<uint32_t>(result.positions.size());
            result.positions.push_back(facePoints[fi]);
            result.normals.push_back(glm::vec3(0, 1, 0));
            result.texCoords.push_back(facePointUVs[fi]);
            result.colors.push_back(facePointColors[fi]);
        }

        // Add edge point vertices
        std::map<Edge, uint32_t> edgePointIdx;
        for (auto& [edge, pos] : edgePointPos) {
            edgePointIdx[edge] = static_cast<uint32_t>(result.positions.size());
            result.positions.push_back(pos);
            result.normals.push_back(glm::vec3(0, 1, 0));
            result.texCoords.push_back(edgePointUV[edge]);
            result.colors.push_back(edgePointCol[edge]);
        }

        // Create quads
        uint32_t offset = 0;
        for (uint32_t fi = 0; fi < nFaces; fi++) {
            const auto& vi = mesh.faces[fi].vertexIndices;
            uint32_t n = mesh.faces[fi].count;
            uint32_t fp = facePointIdx[fi];

            for (uint32_t i = 0; i < n; i++) {
                uint32_t corner = vi[i];
                uint32_t ep_next = edgePointIdx[makeEdge(vi[i], vi[(i + 1) % n])];
                uint32_t ep_prev = edgePointIdx[makeEdge(vi[(i + n - 1) % n], vi[i])];

                NGonFace quad;
                quad.vertexIndices = { corner, ep_next, fp, ep_prev };
                quad.count = 4;
                quad.offset = offset;
                quad.normal = glm::vec4(computeFaceNormal(result.positions, quad.vertexIndices), 0.0f);
                quad.center = glm::vec4(computeFaceCentroid(result.positions, quad.vertexIndices), 1.0f);
                quad.area   = computeFaceArea(result.positions, quad.vertexIndices);

                for (uint32_t idx : quad.vertexIndices)
                    result.faceVertexIndices.push_back(idx);
                offset += 4;

                result.faces.push_back(std::move(quad));
            }
        }

        // Recompute vertex normals from face normals
        uint32_t totalVerts = static_cast<uint32_t>(result.positions.size());
        std::vector<glm::vec3> normalAccum(totalVerts, glm::vec3(0));
        for (const auto& face : result.faces) {
            glm::vec3 fn = glm::vec3(face.normal);
            for (uint32_t idx : face.vertexIndices) {
                normalAccum[idx] += fn;
            }
        }
        for (uint32_t i = 0; i < totalVerts; i++) {
            float len = glm::length(normalAccum[i]);
            result.normals[i] = (len > 0.0001f) ? normalAccum[i] / len : glm::vec3(0, 1, 0);
        }

        result.nbVertices = totalVerts;
        result.nbFaces    = static_cast<uint32_t>(result.faces.size());

        mesh = std::move(result);

        std::cout << "Catmull-Clark subdivided (level " << (lvl + 1) << "): "
                  << mesh.nbFaces << " faces, "
                  << mesh.nbVertices << " vertices" << std::endl;
    }
}
