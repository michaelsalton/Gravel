# Feature 3.1: Task Shader Mapping Function (F)

**Epic**: [Epic 3 - Core Resurfacing Pipeline](../../03/epic-03-core-resurfacing.md)
**Estimated Time**: 2 hours
**Prerequisites**: [Epic 2 Complete](../../02/epic-02-mesh-loading.md)

## Goal

Create the task shader that maps one workgroup per face+vertex element, determines element type, reads data from half-edge SSBOs, and emits mesh shader invocations via task payload.

## What You'll Build

- Task shader with mapping function F
- TaskPayload structure for data transfer
- Element type detection (face vs vertex)
- Half-edge data reading
- Mesh shader dispatch (1:1 for now, amplification in Epic 4)

## Files to Create/Modify

### Create
- `shaders/parametric/parametric.task`
- `shaders/common.glsl` (shared utilities)

### Modify
- `CMakeLists.txt` (ensure task shaders are compiled)

## Implementation Steps

### Step 1: Create shaders/common.glsl

```glsl
#ifndef COMMON_GLSL
#define COMMON_GLSL

#include "shaderInterface.h"

// ============================================================================
// Half-Edge Data Access Helpers
// ============================================================================

// These are convenience wrappers around the shaderInterface.h functions
// to make code more readable in the main shaders

vec3 readVertexPosition(uint vertId) {
    return getVertexPosition(vertId);
}

vec3 readVertexNormal(uint vertId) {
    return getVertexNormal(vertId);
}

vec3 readFaceCenter(uint faceId) {
    return getFaceCenter(faceId);
}

vec3 readFaceNormal(uint faceId) {
    return getFaceNormal(faceId);
}

float readFaceArea(uint faceId) {
    return getFaceArea(faceId);
}

int readVertexEdge(uint vertId) {
    return getVertexEdge(vertId);
}

int readFaceEdge(uint faceId) {
    return getFaceEdge(faceId);
}

int readHalfEdgeFace(uint heId) {
    return getHalfEdgeFace(heId);
}

// ============================================================================
// Math Utilities
// ============================================================================

const float PI = 3.14159265359;

// More utilities will be added in later features

#endif // COMMON_GLSL
```

### Step 2: Create shaders/parametric/parametric.task

```glsl
#version 450
#extension GL_EXT_mesh_shader : require
#extension GL_GOOGLE_include_directive : require

#include "../shaderInterface.h"
#include "../common.glsl"

// Task shader work group size: one thread per task
layout(local_size_x = 1) in;

// ============================================================================
// Task Payload
// ============================================================================

// Payload transferred from task shader to mesh shader
taskPayloadSharedEXT TaskPayload {
    vec3 position;      // Face center or vertex position
    vec3 normal;        // Face normal or vertex normal
    float area;         // Face area (used for scaling)
    uint taskId;        // Global element ID
    uint isVertex;      // 1 if vertex element, 0 if face element
    uint elementType;   // Parametric surface type (from UBO)
    uint padding;       // Alignment
} payload;

// ============================================================================
// Push Constants
// ============================================================================

layout(push_constant) uniform PushConstants {
    mat4 mvp;           // Model-View-Projection matrix
    uint nbFaces;       // Number of faces in mesh
    uint nbVertices;    // Number of vertices in mesh
    uint elementType;   // Parametric surface type (0=torus, 1=sphere, etc.)
    uint padding;
} push;

// ============================================================================
// Main: Mapping Function F
// ============================================================================

void main() {
    // Global task ID = workgroup ID
    uint globalId = gl_WorkGroupID.x;

    // Total number of tasks = nbFaces + nbVertices
    // First nbFaces tasks are face elements
    // Remaining nbVertices tasks are vertex elements

    bool isVertex = (globalId >= push.nbFaces);

    uint faceId = 0;
    uint vertId = 0;

    if (isVertex) {
        // This is a vertex element
        vertId = globalId - push.nbFaces;

        // Get associated face via outgoing half-edge
        int edge = readVertexEdge(vertId);
        if (edge >= 0) {
            faceId = uint(readHalfEdgeFace(uint(edge)));
        } else {
            // Isolated vertex (shouldn't happen with valid mesh)
            faceId = 0;
        }

        // Read vertex data
        payload.position = readVertexPosition(vertId);
        payload.normal = readVertexNormal(vertId);
        payload.area = readFaceArea(faceId);  // Use adjacent face area for scaling
    } else {
        // This is a face element
        faceId = globalId;

        // Read face data
        payload.position = readFaceCenter(faceId);
        payload.normal = readFaceNormal(faceId);
        payload.area = readFaceArea(faceId);
    }

    // Populate payload
    payload.taskId = globalId;
    payload.isVertex = isVertex ? 1u : 0u;
    payload.elementType = push.elementType;

    // Emit one mesh shader invocation
    // (In Epic 4, this will emit multiple invocations for large grids)
    EmitMeshTasksEXT(1, 1, 1);
}
```

### Step 3: Update CMakeLists.txt (if needed)

The shader compilation setup from Epic 2 should already handle `.task` files:

```cmake
# Check if task shaders are included
file(GLOB_RECURSE SHADER_SOURCES
    "${SHADER_DIR}/*.vert"
    "${SHADER_DIR}/*.frag"
    "${SHADER_DIR}/*.comp"
    "${SHADER_DIR}/*.geom"
    "${SHADER_DIR}/*.tesc"
    "${SHADER_DIR}/*.tese"
    "${SHADER_DIR}/*.mesh"
    "${SHADER_DIR}/*.task"  # <-- Ensure this is present
)
```

If not present, add `"${SHADER_DIR}/*.task"` to the GLOB_RECURSE list.

### Step 4: Create Placeholder Mesh Shader (for testing)

Create `shaders/parametric/parametric.mesh` (minimal for now):

```glsl
#version 450
#extension GL_EXT_mesh_shader : require
#extension GL_GOOGLE_include_directive : require

#include "../shaderInterface.h"

layout(local_size_x = 1) in;
layout(max_vertices = 3, max_primitives = 1, triangles) out;

taskPayloadSharedEXT TaskPayload {
    vec3 position;
    vec3 normal;
    float area;
    uint taskId;
    uint isVertex;
    uint elementType;
    uint padding;
} payload;

layout(location = 0) out vec4 vColor[];

void main() {
    // Emit one triangle at payload position
    SetMeshOutputsEXT(3, 1);

    vec3 offset = vec3(0.1, 0.0, 0.0);
    gl_MeshVerticesEXT[0].gl_Position = vec4(payload.position + offset, 1.0);
    gl_MeshVerticesEXT[1].gl_Position = vec4(payload.position - offset + vec3(0, 0.1, 0), 1.0);
    gl_MeshVerticesEXT[2].gl_Position = vec4(payload.position - offset - vec3(0, 0.1, 0), 1.0);

    vColor[0] = vec4(1, 0, 0, 1);
    vColor[1] = vec4(0, 1, 0, 1);
    vColor[2] = vec4(0, 0, 1, 1);

    gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0, 1, 2);
}
```

### Step 5: Create Placeholder Fragment Shader

Create `shaders/parametric/parametric.frag`:

```glsl
#version 450

layout(location = 0) in vec4 vColor;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vColor;
}
```

### Step 6: Compile Shaders

```bash
cd build
cmake --build .

# Check that task shader compiled
ls build/shaders/parametric/
# Should see: parametric.task.spv, parametric.mesh.spv, parametric.frag.spv
```

### Step 7: Update C++ Renderer (Optional Validation)

For now, you don't need to hook up the pipeline yet (that's in the next features). Just verify the shaders compile without errors.

If you want to verify the task shader count is correct:

```cpp
// In your renderer or main loop
uint32_t nbFaces = halfEdgeMesh.nbFaces;
uint32_t nbVertices = halfEdgeMesh.nbVertices;
uint32_t totalTasks = nbFaces + nbVertices;

std::cout << "Task shader dispatch count: " << totalTasks << std::endl;
std::cout << "  Face elements: " << nbFaces << std::endl;
std::cout << "  Vertex elements: " << nbVertices << std::endl;
```

## Acceptance Criteria

- [ ] parametric.task compiles to .spv without errors
- [ ] parametric.mesh compiles to .spv without errors
- [ ] parametric.frag compiles to .spv without errors
- [ ] No validation errors from glslangValidator
- [ ] Task payload size is reasonable (currently 32 bytes - well within limits)
- [ ] common.glsl can be included from other shaders

## Expected Output

```
Compiling shader parametric.task
Compiling shader parametric.mesh
Compiling shader parametric.frag

Shader compilation complete:
  parametric.task.spv (size: ~1-2 KB)
  parametric.mesh.spv (size: ~1-2 KB)
  parametric.frag.spv (size: ~500 bytes)

Task shader dispatch count: 14
  Face elements: 6
  Vertex elements: 8
```

(For a cube mesh with 8 vertices and 6 faces)

## Troubleshooting

### Task Shader Doesn't Compile

**Error**: `undeclared identifier 'readVertexPosition'`

**Fix**: Ensure `common.glsl` is in `shaders/` directory and the include path is correct (`#include "../common.glsl"`).

---

**Error**: `'TaskPayload' : identifier not previously declared`

**Fix**: Ensure `taskPayloadSharedEXT` keyword is used (not `taskPayloadEXT` or `shared`).

---

**Error**: `'EmitMeshTasksEXT' : no matching overloaded function found`

**Fix**: Ensure `#extension GL_EXT_mesh_shader : require` is at the top of the file.

### Payload Size Too Large

**Symptom**: Validation error about payload size exceeding limits.

**Fix**: The current payload is 32 bytes (7 floats + 4 uints). This is well under the typical 16KB limit. If you add more fields, ensure total size stays under the hardware limit.

### Wrong Number of Tasks

**Symptom**: Expected 14 tasks for a cube, but getting different count.

**Fix**: Verify `push.nbFaces` and `push.nbVertices` match the actual mesh data. Check that push constants are set correctly in C++.

## Common Pitfalls

1. **Include Paths**: Use relative paths from shader file location (`../common.glsl` not `shaders/common.glsl`).

2. **Task Payload Alignment**: The payload structure should have proper alignment. Vulkan uses std430 rules. Current layout is correct.

3. **EmitMeshTasksEXT Arguments**: Three arguments (x, y, z) for 3D dispatch. For now, use (1, 1, 1).

4. **WorkGroup ID vs Invocation ID**: Use `gl_WorkGroupID.x` for task ID, not `gl_LocalInvocationID.x` (which is always 0 with local_size_x = 1).

5. **Boundary Half-Edges**: Twin edges on mesh boundaries are -1. Handle this case when reading topology.

## Validation Tests

### Test 1: Shader Compilation

```bash
glslangValidator --target-env vulkan1.3 \
    -V shaders/parametric/parametric.task \
    -o build/shaders/parametric.task.spv \
    -I./shaders

echo $?  # Should be 0 (success)
```

### Test 2: Payload Size

Check payload size:

```cpp
struct TaskPayload {
    glm::vec3 position;  // 12 bytes
    glm::vec3 normal;    // 12 bytes
    float area;          // 4 bytes
    uint32_t taskId;     // 4 bytes
    uint32_t isVertex;   // 4 bytes
    uint32_t elementType; // 4 bytes
    uint32_t padding;    // 4 bytes
};
// Total: 44 bytes (with padding)

static_assert(sizeof(TaskPayload) <= 16384, "Payload too large");
```

### Test 3: Dispatch Count

For various meshes:

| Mesh      | Vertices | Faces | Total Tasks |
|-----------|----------|-------|-------------|
| Cube      | 8        | 6     | 14          |
| Icosahedron | 12     | 20    | 32          |
| Suzanne   | 507      | 500   | 1007        |

## Next Steps

Next feature: **[Feature 3.2 - Mesh Shader Hardcoded Quad](feature-02-mesh-shader-quad.md)**

You'll create the mesh shader that receives the task payload and outputs a simple 2×2 quad at each element position.

## Summary

You've implemented:
- ✅ Task shader with mapping function F
- ✅ TaskPayload structure for data transfer
- ✅ Element type detection (face vs vertex)
- ✅ Half-edge data reading via common.glsl helpers
- ✅ Mesh shader dispatch (1:1 ratio)

The task shader now correctly maps workgroups to mesh elements and prepares data for the mesh shader!
