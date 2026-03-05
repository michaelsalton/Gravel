#include "loaders/ObjWriter.h"
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
