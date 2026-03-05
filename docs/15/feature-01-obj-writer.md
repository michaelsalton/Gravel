# Feature 15.1: Build System & OBJ Writer

**Epic**: [Epic 15 - Procedural Mesh Export](epic-15-procedural-mesh-export.md)
**Prerequisites**: None (standalone utility)

## Goal

Add compute shader compilation support to CMake and implement an OBJ file writer — the inverse of the existing `ObjLoader::load()`.

## Files Created

- `include/loaders/ObjWriter.h` — OBJ export interface
- `src/loaders/ObjWriter.cpp` — OBJ export implementation

## Files Modified

- `CMakeLists.txt` — add `*.comp` to shader glob, add new source files

## CMake Changes

Add `*.comp` to the shader glob (line ~162):

```cmake
file(GLOB_RECURSE SHADER_SOURCES
    ${CMAKE_SOURCE_DIR}/shaders/*.vert
    ${CMAKE_SOURCE_DIR}/shaders/*.frag
    ${CMAKE_SOURCE_DIR}/shaders/*.task
    ${CMAKE_SOURCE_DIR}/shaders/*.mesh
    ${CMAKE_SOURCE_DIR}/shaders/*.comp
)
```

Add new source files to SOURCES:

```cmake
src/loaders/ObjWriter.cpp
src/renderer/MeshExport.cpp
```

## ObjWriter Interface

```cpp
#pragma once
#include <glm/glm.hpp>
#include <string>
#include <cstdint>

class ObjWriter {
public:
    static void write(const std::string& filepath,
                      const glm::vec4* positions,
                      const glm::vec4* normals,
                      const glm::vec2* uvs,
                      const uint32_t* indices,
                      uint32_t numVertices,
                      uint32_t numTriangles);
};
```

## OBJ Format Output

```
# Exported from Gravel procedural mesh renderer
# Vertices: 25920, Triangles: 46080

v x y z
...
vn nx ny nz
...
vt u v
...
f v1/vt1/vn1 v2/vt2/vn2 v3/vt3/vn3
...
```

OBJ indices are **1-based** (add 1 to all indices). Since positions, normals, and UVs share the same vertex indexing, face entries use `i/i/i` format.

## Acceptance Criteria

- [ ] `*.comp` shaders compile via CMake shader target
- [ ] Project builds with new source files
- [ ] `ObjWriter::write()` produces valid OBJ files
- [ ] Output OBJ loads correctly in Blender and back into Gravel via `ObjLoader::load()`
