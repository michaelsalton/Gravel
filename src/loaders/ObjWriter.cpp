#include "loaders/ObjWriter.h"
#include "loaders/ObjLoader.h"
#include <fstream>
#include <iostream>
#include <stdexcept>

void ObjWriter::write(const std::string& filepath,
                      const glm::vec4* positions,
                      const glm::vec4* normals,
                      const glm::vec2* uvs,
                      const uint32_t* indices,
                      uint32_t numVertices,
                      uint32_t numTriangles) {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file for writing: " + filepath);
    }

    file << "# Exported from Gravel procedural mesh renderer\n";
    file << "# Vertices: " << numVertices
         << ", Triangles: " << numTriangles << "\n\n";

    // Write vertex positions
    for (uint32_t i = 0; i < numVertices; i++) {
        file << "v " << positions[i].x << " "
             << positions[i].y << " "
             << positions[i].z << "\n";
    }
    file << "\n";

    // Write vertex normals
    for (uint32_t i = 0; i < numVertices; i++) {
        file << "vn " << normals[i].x << " "
             << normals[i].y << " "
             << normals[i].z << "\n";
    }
    file << "\n";

    // Write texture coordinates
    for (uint32_t i = 0; i < numVertices; i++) {
        file << "vt " << uvs[i].x << " "
             << uvs[i].y << "\n";
    }
    file << "\n";

    // Write faces (OBJ is 1-indexed)
    for (uint32_t t = 0; t < numTriangles; t++) {
        uint32_t i0 = indices[t * 3 + 0] + 1;
        uint32_t i1 = indices[t * 3 + 1] + 1;
        uint32_t i2 = indices[t * 3 + 2] + 1;
        file << "f " << i0 << "/" << i0 << "/" << i0 << " "
             << i1 << "/" << i1 << "/" << i1 << " "
             << i2 << "/" << i2 << "/" << i2 << "\n";
    }

    file.close();

    std::cout << "Exported OBJ: " << filepath
              << " (" << numVertices << " vertices, "
              << numTriangles << " triangles)" << std::endl;
}

void ObjWriter::appendMesh(const std::string& filepath,
                           const NGonMesh& mesh,
                           uint32_t vertexOffset) {
    std::ofstream file(filepath, std::ios::app);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file for appending: " + filepath);
    }

    file << "\n# Base mesh (" << mesh.positions.size() << " vertices)\n";
    file << "o BaseMesh\n";

    for (const auto& p : mesh.positions) {
        file << "v " << p.x << " " << p.y << " " << p.z << "\n";
    }
    file << "\n";

    for (const auto& n : mesh.normals) {
        file << "vn " << n.x << " " << n.y << " " << n.z << "\n";
    }
    file << "\n";

    for (const auto& t : mesh.texCoords) {
        file << "vt " << t.x << " " << t.y << "\n";
    }
    file << "\n";

    // Write triangulated faces
    for (const auto& face : mesh.faces) {
        if (face.vertexIndices.size() < 3) continue;
        // Fan triangulate
        for (size_t i = 1; i + 1 < face.vertexIndices.size(); i++) {
            uint32_t i0 = face.vertexIndices[0] + vertexOffset;
            uint32_t i1 = face.vertexIndices[i] + vertexOffset;
            uint32_t i2 = face.vertexIndices[i + 1] + vertexOffset;

            uint32_t n0 = face.normalIndices.empty() ? i0 : face.normalIndices[0] + vertexOffset;
            uint32_t n1 = face.normalIndices.empty() ? i1 : face.normalIndices[i] + vertexOffset;
            uint32_t n2 = face.normalIndices.empty() ? i2 : face.normalIndices[i + 1] + vertexOffset;

            file << "f " << i0 << "//" << n0 << " "
                 << i1 << "//" << n1 << " "
                 << i2 << "//" << n2 << "\n";
        }
    }

    file.close();

    uint32_t baseTris = 0;
    for (const auto& face : mesh.faces) {
        if (face.vertexIndices.size() >= 3)
            baseTris += static_cast<uint32_t>(face.vertexIndices.size()) - 2;
    }
    std::cout << "Appended base mesh: " << mesh.positions.size()
              << " vertices, " << baseTris << " triangles" << std::endl;
}
