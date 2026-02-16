#include "loaders/ObjLoader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>

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

    // Fill defaults for missing attributes
    if (mesh.normals.empty()) {
        mesh.normals.resize(mesh.positions.size(), glm::vec3(0.0f, 0.0f, 1.0f));
    }
    if (mesh.texCoords.empty()) {
        mesh.texCoords.resize(mesh.positions.size(), glm::vec2(0.0f));
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
