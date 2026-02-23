#include "loaders/LUTLoader.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <cmath>
#include <set>

LutData LUTLoader::loadControlCage(const std::string& filepath) {
    LutData lut;
    lut.filename = filepath;

    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "LUTLoader: failed to open: " << filepath << std::endl;
        return lut;
    }

    std::vector<glm::vec3> rawPositions;
    std::vector<glm::vec2> rawUVs;

    // Unique (posIdx, uvIdx) pairs extracted from face data
    // Using a set to deduplicate shared vertices across faces
    std::set<std::pair<uint32_t, uint32_t>> seen;
    std::vector<std::pair<uint32_t, uint32_t>> faceVertices; // ordered for determinism

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        std::string prefix;
        iss >> prefix;

        if (prefix == "v") {
            glm::vec3 pos;
            iss >> pos.x >> pos.y >> pos.z;
            rawPositions.push_back(pos);

        } else if (prefix == "vt") {
            glm::vec2 uv;
            iss >> uv.x >> uv.y;
            rawUVs.push_back(uv);

        } else if (prefix == "f") {
            std::string token;
            while (iss >> token) {
                uint32_t vIdx = 0, vtIdx = 0;

                size_t slash = token.find('/');
                if (slash == std::string::npos) {
                    vIdx = static_cast<uint32_t>(std::stoi(token));
                } else {
                    vIdx  = static_cast<uint32_t>(std::stoi(token.substr(0, slash)));
                    size_t slash2 = token.find('/', slash + 1);
                    std::string vtStr = (slash2 == std::string::npos)
                        ? token.substr(slash + 1)
                        : token.substr(slash + 1, slash2 - slash - 1);
                    if (!vtStr.empty())
                        vtIdx = static_cast<uint32_t>(std::stoi(vtStr));
                }

                // OBJ is 1-indexed
                auto key = std::make_pair(vIdx - 1, vtIdx - 1);
                if (seen.insert(key).second)
                    faceVertices.push_back(key);
            }
        }
    }

    if (rawPositions.empty() || rawUVs.empty()) {
        std::cerr << "LUTLoader: missing positions or UVs in " << filepath << std::endl;
        return lut;
    }

    // Build VertexUV list from face-extracted pairs
    std::vector<VertexUV> vertices;
    vertices.reserve(faceVertices.size());
    for (auto& [pIdx, uvIdx] : faceVertices) {
        if (pIdx >= rawPositions.size() || uvIdx >= rawUVs.size()) {
            std::cerr << "LUTLoader: index out of range in " << filepath << std::endl;
            return lut;
        }
        vertices.push_back({ rawPositions[pIdx], rawUVs[uvIdx] });
    }

    // Sort into row-major UV order (V outer, U inner)
    sortByUV(vertices);

    // Detect grid dimensions from unique U/V values
    if (!detectGridDimensions(vertices, lut.Nx, lut.Ny)) {
        std::cerr << "LUTLoader: grid dimension detection failed for " << filepath << std::endl;
        return lut;
    }

    if (lut.Nx * lut.Ny != static_cast<uint32_t>(vertices.size())) {
        std::cerr << "LUTLoader: grid validation failed: "
                  << lut.Nx << "x" << lut.Ny << " = " << lut.Nx * lut.Ny
                  << " but have " << vertices.size() << " vertices" << std::endl;
        return lut;
    }

    lut.controlPoints.reserve(vertices.size());
    for (const auto& v : vertices)
        lut.controlPoints.push_back(v.position);

    computeBoundingBox(lut.controlPoints, lut.bbMin, lut.bbMax);

    lut.isValid = true;

    std::cout << "LUTLoader: loaded " << filepath << std::endl;
    std::cout << "  Grid: " << lut.Nx << "x" << lut.Ny
              << " (" << lut.controlPoints.size() << " control points)" << std::endl;
    std::cout << "  BB min: (" << lut.bbMin.x << ", " << lut.bbMin.y << ", " << lut.bbMin.z << ")"
              << "  max: (" << lut.bbMax.x << ", " << lut.bbMax.y << ", " << lut.bbMax.z << ")" << std::endl;

    return lut;
}

void LUTLoader::sortByUV(std::vector<VertexUV>& vertices) {
    // Row-major: sort by V (major axis) then U (minor axis)
    std::sort(vertices.begin(), vertices.end(), [](const VertexUV& a, const VertexUV& b) {
        const float eps = 1e-5f;
        if (std::abs(a.uv.y - b.uv.y) > eps)
            return a.uv.y < b.uv.y;
        return a.uv.x < b.uv.x;
    });
}

bool LUTLoader::detectGridDimensions(const std::vector<VertexUV>& vertices,
                                     uint32_t& Nx, uint32_t& Ny) {
    if (vertices.empty()) { Nx = Ny = 0; return false; }

    const float eps = 1e-5f;

    // Count unique U values
    std::vector<float> uniqueU;
    for (const auto& v : vertices) {
        bool found = false;
        for (float u : uniqueU)
            if (std::abs(u - v.uv.x) < eps) { found = true; break; }
        if (!found) uniqueU.push_back(v.uv.x);
    }

    // Count unique V values
    std::vector<float> uniqueV;
    for (const auto& v : vertices) {
        bool found = false;
        for (float val : uniqueV)
            if (std::abs(val - v.uv.y) < eps) { found = true; break; }
        if (!found) uniqueV.push_back(v.uv.y);
    }

    Nx = static_cast<uint32_t>(uniqueU.size());
    Ny = static_cast<uint32_t>(uniqueV.size());

    return Nx > 0 && Ny > 0;
}

void LUTLoader::computeBoundingBox(const std::vector<glm::vec3>& points,
                                   glm::vec3& outMin, glm::vec3& outMax) {
    if (points.empty()) { outMin = outMax = glm::vec3(0.0f); return; }
    outMin = outMax = points[0];
    for (const auto& p : points) {
        outMin = glm::min(outMin, p);
        outMax = glm::max(outMax, p);
    }
}
