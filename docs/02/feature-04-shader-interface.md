# Feature 2.4: Shared Shader Interface

**Epic**: [Epic 2 - Mesh Loading and GPU Upload](../../epic-02-mesh-loading.md)
**Estimated Time**: 2 hours
**Prerequisites**: [Feature 2.3 - GPU Buffer Upload](feature-03-gpu-buffers.md)

## Goal

Create a shared header file that works in both C++ and GLSL, defining all UBO structures, descriptor bindings, and helper functions for accessing half-edge data from shaders.

## What You'll Build

- Cross-platform shader interface header (C++/GLSL compatible)
- UBO structure definitions
- Descriptor set/binding constants
- GLSL helper functions for mesh data access
- Test mesh shader that reads and validates GPU data

## Files to Create/Modify

### Create
- `shaders/shaderInterface.h`
- `shaders/test_halfedge.mesh`

### Modify
- `CMakeLists.txt` (add shader compilation with -I flag)

## Implementation Steps

### Step 1: Create shaders/shaderInterface.h

```glsl
#ifndef SHADER_INTERFACE_H
#define SHADER_INTERFACE_H

// Cross-platform compatibility
#ifdef __cplusplus
    // C++ definitions
    #include <glm/glm.hpp>
    using vec2 = glm::vec2;
    using vec3 = glm::vec3;
    using vec4 = glm::vec4;
    using mat4 = glm::mat4;
    using uint = uint32_t;
    #define LAYOUT_STD140(set, binding)
    #define LAYOUT_STD430(set, binding)
#else
    // GLSL definitions
    #define LAYOUT_STD140(set, binding) layout(std140, set=set, binding=binding)
    #define LAYOUT_STD430(set, binding) layout(std430, set=set, binding=binding)
#endif

// ============================================================================
// Descriptor Set Numbers
// ============================================================================

#define SET_SCENE 0
#define SET_HALF_EDGE 1
#define SET_PER_OBJECT 2

// ============================================================================
// Descriptor Bindings - Set 0 (SceneSet)
// ============================================================================

#define BINDING_VIEW_UBO 0
#define BINDING_SHADING_UBO 1

struct ViewUBO {
    mat4 view;
    mat4 projection;
    vec4 cameraPosition;  // xyz = position, w = unused
    float nearPlane;
    float farPlane;
    float padding[2];
};

struct GlobalShadingUBO {
    vec4 lightPosition;   // xyz = position, w = unused
    vec4 ambient;         // rgb = ambient color, a = intensity
    float diffuse;
    float specular;
    float shininess;
    float padding;
};

// ============================================================================
// Descriptor Bindings - Set 1 (HESet - Half-Edge Data)
// ============================================================================

#define BINDING_HE_VEC4 0
#define BINDING_HE_VEC2 1
#define BINDING_HE_INT 2
#define BINDING_HE_FLOAT 3

struct MeshInfoUBO {
    uint nbVertices;
    uint nbFaces;
    uint nbHalfEdges;
    uint padding;
};

// GLSL-only: Declare buffer arrays
#ifndef __cplusplus

// Vec4 buffers (binding 0, array size 5)
LAYOUT_STD430(SET_HALF_EDGE, BINDING_HE_VEC4) readonly buffer HEVec4Buffer {
    vec4 data[];
} heVec4Buffer[5];

// Vec2 buffers (binding 1, array size 1)
LAYOUT_STD430(SET_HALF_EDGE, BINDING_HE_VEC2) readonly buffer HEVec2Buffer {
    vec2 data[];
} heVec2Buffer[1];

// Int buffers (binding 2, array size 10)
LAYOUT_STD430(SET_HALF_EDGE, BINDING_HE_INT) readonly buffer HEIntBuffer {
    int data[];
} heIntBuffer[10];

// Float buffers (binding 3, array size 1)
LAYOUT_STD430(SET_HALF_EDGE, BINDING_HE_FLOAT) readonly buffer HEFloatBuffer {
    float data[];
} heFloatBuffer[1];

// ============================================================================
// Helper Functions (GLSL only)
// ============================================================================

// Vertex data access (indices into heVec4Buffer/heVec2Buffer)
vec3 getVertexPosition(uint vertId) {
    return heVec4Buffer[0].data[vertId].xyz;
}

vec3 getVertexColor(uint vertId) {
    return heVec4Buffer[1].data[vertId].rgb;
}

vec3 getVertexNormal(uint vertId) {
    return heVec4Buffer[2].data[vertId].xyz;
}

vec2 getVertexTexCoord(uint vertId) {
    return heVec2Buffer[0].data[vertId];
}

int getVertexEdge(uint vertId) {
    return heIntBuffer[0].data[vertId];
}

// Face data access
int getFaceEdge(uint faceId) {
    return heIntBuffer[1].data[faceId];
}

int getFaceVertCount(uint faceId) {
    return heIntBuffer[2].data[faceId];
}

int getFaceOffset(uint faceId) {
    return heIntBuffer[3].data[faceId];
}

vec3 getFaceNormal(uint faceId) {
    return heVec4Buffer[3].data[faceId].xyz;
}

vec3 getFaceCenter(uint faceId) {
    return heVec4Buffer[4].data[faceId].xyz;
}

float getFaceArea(uint faceId) {
    return heFloatBuffer[0].data[faceId];
}

// Half-edge topology access
int getHalfEdgeVertex(uint heId) {
    return heIntBuffer[4].data[heId];
}

int getHalfEdgeFace(uint heId) {
    return heIntBuffer[5].data[heId];
}

int getHalfEdgeNext(uint heId) {
    return heIntBuffer[6].data[heId];
}

int getHalfEdgePrev(uint heId) {
    return heIntBuffer[7].data[heId];
}

int getHalfEdgeTwin(uint heId) {
    return heIntBuffer[8].data[heId];
}

int getVertexFaceIndex(uint index) {
    return heIntBuffer[9].data[index];
}

#endif // __cplusplus

// ============================================================================
// Descriptor Bindings - Set 2 (PerObjectSet)
// ============================================================================

#define BINDING_CONFIG_UBO 0
#define BINDING_SHADING_UBO_PER_OBJECT 1

struct ResurfacingUBO {
    uint elementType;      // 0-10 (torus, sphere, ..., B-spline, Bezier)
    float userScaling;     // global scale multiplier
    uint resolutionM;      // U direction resolution
    uint resolutionN;      // V direction resolution

    float torusMajorR;
    float torusMinorR;
    float sphereRadius;
    float padding1;

    uint doLod;            // enable LOD
    float lodFactor;       // LOD multiplier
    uint doCulling;        // enable culling
    float cullingThreshold;

    uint padding[4];
};

// ============================================================================
// Constants
// ============================================================================

#ifndef __cplusplus
const float PI = 3.14159265359;
#endif

#endif // SHADER_INTERFACE_H
```

### Step 2: Create shaders/test_halfedge.mesh

```glsl
#version 450
#extension GL_EXT_mesh_shader : require
#extension GL_GOOGLE_include_directive : require

#include "shaderInterface.h"

layout(local_size_x = 1) in;
layout(max_vertices = 8, max_primitives = 6, triangles) out;

// Per-vertex outputs
layout(location = 0) out vec4 vColor[];

// Push constants (for testing, pass nbVertices and nbFaces)
layout(push_constant) uniform PushConstants {
    mat4 mvp;
    uint testMode;  // 0 = show first 8 vertices, 1 = show first face
} push;

void main() {
    uint workgroupId = gl_WorkGroupID.x;

    if (push.testMode == 0) {
        // Test mode 0: Render first 8 vertices as points (cube corners)
        SetMeshOutputsEXT(8, 0);  // 8 vertices, 0 primitives (just points)

        for (uint i = 0; i < 8; ++i) {
            vec3 pos = getVertexPosition(i);
            gl_MeshVerticesEXT[i].gl_Position = push.mvp * vec4(pos, 1.0);
            vColor[i] = vec4(getVertexColor(i), 1.0);

            // Debug: print first vertex position
            if (i == 0) {
                // Can't print from shader, but verify no crashes
            }
        }

    } else {
        // Test mode 1: Render first face as triangle fan
        uint faceId = 0;
        int faceEdge = getFaceEdge(faceId);
        int vertCount = getFaceVertCount(faceId);

        SetMeshOutputsEXT(uint(vertCount), uint(vertCount - 2));

        // Emit vertices
        int currentEdge = faceEdge;
        for (int i = 0; i < vertCount; ++i) {
            int vertId = getHalfEdgeVertex(currentEdge);
            vec3 pos = getVertexPosition(uint(vertId));

            gl_MeshVerticesEXT[i].gl_Position = push.mvp * vec4(pos, 1.0);
            vColor[i] = vec4(getFaceNormal(faceId) * 0.5 + 0.5, 1.0);

            currentEdge = getHalfEdgeNext(currentEdge);
        }

        // Emit triangle fan
        for (int i = 0; i < vertCount - 2; ++i) {
            gl_PrimitiveTriangleIndicesEXT[i] = uvec3(0, i + 1, i + 2);
        }
    }
}
```

### Step 3: Create simple test fragment shader

Create `shaders/test_halfedge.frag`:

```glsl
#version 450

layout(location = 0) in vec4 vColor;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vColor;
}
```

### Step 4: Update CMakeLists.txt for shader compilation

```cmake
# Shader compilation with include support
set(SHADER_DIR ${CMAKE_SOURCE_DIR}/shaders)
set(SHADER_OUTPUT_DIR ${CMAKE_BINARY_DIR}/shaders)

file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

# Find all shader files
file(GLOB_RECURSE SHADER_SOURCES
    "${SHADER_DIR}/*.vert"
    "${SHADER_DIR}/*.frag"
    "${SHADER_DIR}/*.comp"
    "${SHADER_DIR}/*.geom"
    "${SHADER_DIR}/*.tesc"
    "${SHADER_DIR}/*.tese"
    "${SHADER_DIR}/*.mesh"
    "${SHADER_DIR}/*.task"
)

foreach(SHADER ${SHADER_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    set(SHADER_OUTPUT ${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv)

    add_custom_command(
        OUTPUT ${SHADER_OUTPUT}
        COMMAND ${GLSLANG_VALIDATOR}
            --target-env vulkan1.3
            -V ${SHADER}
            -o ${SHADER_OUTPUT}
            -I${SHADER_DIR}
        DEPENDS ${SHADER}
        COMMENT "Compiling shader ${SHADER_NAME}"
        VERBATIM
    )

    list(APPEND SHADER_OUTPUTS ${SHADER_OUTPUT})
endforeach()

add_custom_target(shaders ALL DEPENDS ${SHADER_OUTPUTS})

# Make sure executable depends on shaders
if(TARGET ${PROJECT_NAME})
    add_dependencies(${PROJECT_NAME} shaders)
endif()
```

### Step 5: Test Shader Compilation

```bash
cd build
cmake ..
cmake --build .

# Check that shaders compiled
ls build/shaders/
# Should see: test_halfedge.mesh.spv, test_halfedge.frag.spv
```

### Step 6: Verify in Application

For now, just verify shaders compile. In Epic 3, you'll actually render with them.

Print compilation success:

```cpp
// In main.cpp or renderer.cpp
std::cout << "✓ Shaders compiled successfully" << std::endl;
```

## Acceptance Criteria

- [ ] shaderInterface.h compiles in C++ without errors
- [ ] shaderInterface.h compiles in GLSL without errors
- [ ] test_halfedge.mesh.spv compiled successfully
- [ ] test_halfedge.frag.spv compiled successfully
- [ ] No compilation warnings
- [ ] Helper functions accessible in GLSL
- [ ] Can include shaderInterface.h with #include directive

## Expected Output

```
✓ Shaders compiled successfully

Compiling shader test_halfedge.mesh
Compiling shader test_halfedge.frag

Shader compilation complete:
  test_halfedge.mesh.spv (size: ~2-3 KB)
  test_halfedge.frag.spv (size: ~1 KB)
```

## Troubleshooting

### Include Not Found
- Ensure -I${SHADER_DIR} is passed to glslangValidator
- Check SHADER_DIR path is correct
- Verify shaderInterface.h exists

### Syntax Errors in GLSL
- Check #ifdef __cplusplus guards are correct
- Ensure GLSL-only code is guarded
- Verify struct definitions have no C++-only syntax

### Buffer Array Declaration Issues
- Ensure GL_EXT_shader_buffer_reference or similar is not needed
- Check descriptor set/binding numbers match C++ side
- Verify array sizes match (vec4[5], vec2[1], int[10], float[1])

## Common Pitfalls

1. **Include Path**: Must use -I flag, not just #include
2. **std430 vs std140**: Use std430 for SSBOs, std140 for UBOs
3. **Array Indexing**: GLSL arrays are 0-indexed like C++
4. **Type Compatibility**: uint in C++ = uint32_t, in GLSL = uint
5. **Padding**: Ensure UBO structs have proper alignment

## Validation Tests

### Test 1: C++ Compilation

```cpp
#include "shaders/shaderInterface.h"

// Should compile without errors
ViewUBO viewUBO;
viewUBO.nearPlane = 0.1f;
viewUBO.farPlane = 100.0f;
```

### Test 2: GLSL Compilation

```glsl
#include "shaderInterface.h"

// In mesh shader:
vec3 pos = getVertexPosition(0);
vec3 normal = getFaceNormal(0);
```

### Test 3: Data Access

In Epic 3, you'll verify actual data reads correctly. For now, ensure:
- Functions are callable
- No linker errors
- Shader compiles with all helper functions

## Memory Layout Notes

GLSL `std430` layout (used for SSBOs):
- Scalars: natural alignment (int=4, float=4)
- vec2: 8-byte alignment
- vec3: 16-byte alignment (same as vec4!)
- vec4: 16-byte alignment
- Arrays: element size rounded to vec4 alignment

C++ must match this layout exactly. GLM types already do.

## Next Steps

Epic 2 is now complete! Next:

**[Epic 3: Core Resurfacing Pipeline](../../epic-03-core-resurfacing.md)**

You'll use the half-edge data from GPU to generate procedural surfaces in the mesh shader.

## Summary of Epic 2

You've accomplished:
- ✅ OBJ parser with n-gon support
- ✅ Half-edge topology construction
- ✅ GPU buffer upload (17 SSBOs)
- ✅ Shared CPU/GPU shader interface

The mesh data is now on the GPU and accessible from shaders via helper functions!
