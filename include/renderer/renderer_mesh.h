#pragma once

#include <cstdint>

// Asset/geometry data management

struct MeshInfoUBO {
    uint32_t nbVertices;
    uint32_t nbFaces;
    uint32_t nbHalfEdges;
    uint32_t slotsPerFace;
};
