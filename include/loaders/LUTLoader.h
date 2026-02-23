#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <cstdint>

/**
 * Control cage data (LUT = Look-Up Table of control points).
 * Control points are stored in row-major order: V outer loop, U inner loop.
 * Index = v * Nx + u
 */
struct LutData {
    std::vector<glm::vec3> controlPoints;  // Nx * Ny control points

    uint32_t Nx = 0;  // Width  (U direction)
    uint32_t Ny = 0;  // Height (V direction)

    glm::vec3 bbMin{0.0f};
    glm::vec3 bbMax{0.0f};

    std::string filename;
    bool isValid = false;
};

/**
 * Loader for control cage OBJ files.
 *
 * Expects quad-grid topology with one UV coordinate per vertex.
 * Face data is parsed to correctly associate positions with UVs,
 * so arbitrary OBJ export orderings are handled correctly.
 */
class LUTLoader {
public:
    static LutData loadControlCage(const std::string& filepath);

private:
    struct VertexUV {
        glm::vec3 position;
        glm::vec2 uv;
    };

    static void sortByUV(std::vector<VertexUV>& vertices);
    static bool detectGridDimensions(const std::vector<VertexUV>& vertices,
                                     uint32_t& Nx, uint32_t& Ny);
    static void computeBoundingBox(const std::vector<glm::vec3>& points,
                                   glm::vec3& outMin, glm::vec3& outMax);
};
