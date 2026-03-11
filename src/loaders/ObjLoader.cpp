#include "loaders/ObjLoader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <map>

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
    for (int lvl = 0; lvl < levels; lvl++) {
        NGonMesh result;
        result.positions = mesh.positions;
        result.normals   = mesh.normals;
        result.texCoords = mesh.texCoords;
        result.colors    = mesh.colors;

        // Ensure normals/texCoords/colors arrays match positions size
        result.normals.resize(result.positions.size(), glm::vec3(0, 1, 0));
        result.texCoords.resize(result.positions.size(), glm::vec2(0));
        result.colors.resize(result.positions.size(), glm::vec3(1));

        // Edge midpoint cache: sorted(v0,v1) -> new vertex index
        std::map<std::pair<uint32_t, uint32_t>, uint32_t> edgeMidpoints;

        auto getEdgeMidpoint = [&](uint32_t a, uint32_t b) -> uint32_t {
            auto key = (a < b) ? std::make_pair(a, b) : std::make_pair(b, a);
            auto it = edgeMidpoints.find(key);
            if (it != edgeMidpoints.end()) return it->second;

            uint32_t idx = static_cast<uint32_t>(result.positions.size());
            result.positions.push_back((result.positions[a] + result.positions[b]) * 0.5f);
            result.normals.push_back(glm::normalize(result.normals[a] + result.normals[b]));
            result.texCoords.push_back((result.texCoords[a] + result.texCoords[b]) * 0.5f);
            result.colors.push_back((result.colors[a] + result.colors[b]) * 0.5f);
            edgeMidpoints[key] = idx;
            return idx;
        };

        uint32_t offset = 0;
        for (const auto& face : mesh.faces) {
            uint32_t n = face.count;
            const auto& vi = face.vertexIndices;

            // Compute face centroid vertex
            glm::vec3 centerPos(0), centerNrm(0), centerCol(0);
            glm::vec2 centerUV(0);
            for (uint32_t i = 0; i < n; i++) {
                centerPos += result.positions[vi[i]];
                centerNrm += result.normals[vi[i]];
                centerUV  += result.texCoords[vi[i]];
                centerCol += result.colors[vi[i]];
            }
            float inv = 1.0f / static_cast<float>(n);
            centerPos *= inv;
            centerNrm = glm::normalize(centerNrm);
            centerUV  *= inv;
            centerCol *= inv;

            uint32_t centerIdx = static_cast<uint32_t>(result.positions.size());
            result.positions.push_back(centerPos);
            result.normals.push_back(centerNrm);
            result.texCoords.push_back(centerUV);
            result.colors.push_back(centerCol);

            // Compute edge midpoints for this face
            std::vector<uint32_t> edgeMids(n);
            for (uint32_t i = 0; i < n; i++) {
                edgeMids[i] = getEdgeMidpoint(vi[i], vi[(i + 1) % n]);
            }

            // Create n quads: (corner_i, edgeMid_i, center, edgeMid_{i-1})
            for (uint32_t i = 0; i < n; i++) {
                uint32_t prevEdge = edgeMids[(i + n - 1) % n];
                uint32_t corner   = vi[i];
                uint32_t nextEdge = edgeMids[i];

                NGonFace quad;
                quad.vertexIndices = { corner, nextEdge, centerIdx, prevEdge };
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

        result.nbVertices = static_cast<uint32_t>(result.positions.size());
        result.nbFaces    = static_cast<uint32_t>(result.faces.size());

        mesh = std::move(result);

        std::cout << "Subdivided (level " << (lvl + 1) << "): "
                  << mesh.nbFaces << " faces, "
                  << mesh.nbVertices << " vertices" << std::endl;
    }
}
