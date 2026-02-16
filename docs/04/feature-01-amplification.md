# Feature 4.1: Amplification Function K

**Epic**: [Epic 4 - Amplification and LOD](../../04/epic-04-amplification-lod.md)
**Estimated Time**: 2-3 hours
**Prerequisites**: [Epic 3 Complete](../../03/epic-03-core-resurfacing.md)

## Goal

Implement the amplification function K in the task shader to enable rendering high-resolution grids (16×16, 32×32, 64×64) by tiling them into multiple mesh shader invocations that respect hardware vertex/primitive limits.

## What You'll Build

- getDeltaUV function to compute maximum tile size
- Constraint checking: (ΔU+1)×(ΔV+1) ≤ maxVertices, ΔU×ΔV×2 ≤ maxPrimitives
- numMeshTasks computation: ceil(MN / ΔUV)
- TaskPayload extended with MN and ΔUV
- 2D mesh task dispatch

## Files to Create/Modify

### Modify
- `shaders/parametric/parametric.task`
- `shaders/parametric/parametric.mesh`

## Implementation Steps

### Step 1: Add Amplification Helper Functions to Task Shader

Update `shaders/parametric/parametric.task`:

```glsl
#version 450
#extension GL_EXT_mesh_shader : require
#extension GL_GOOGLE_include_directive : require

#include "../shaderInterface.h"
#include "../common.glsl"

layout(local_size_x = 1) in;

// ============================================================================
// Task Payload (Extended)
// ============================================================================

taskPayloadSharedEXT TaskPayload {
    vec3 position;
    vec3 normal;
    float area;
    uint taskId;
    uint isVertex;
    uint elementType;
    float userScaling;

    // Amplification fields
    uint resolutionM;   // Target M resolution
    uint resolutionN;   // Target N resolution
    uint deltaU;        // Tile size in U direction
    uint deltaV;        // Tile size in V direction
} payload;

// ============================================================================
// Push Constants
// ============================================================================

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    uint nbFaces;
    uint nbVertices;
    uint elementType;
    float userScaling;
    uint debugMode;
    uint resolutionM;   // Desired resolution M
    uint resolutionN;   // Desired resolution N
} push;

// ============================================================================
// Amplification Function K
// ============================================================================

/**
 * Compute maximum tile size that fits within hardware limits
 *
 * Constraints:
 *   - (ΔU + 1) × (ΔV + 1) ≤ maxMeshOutputVertices
 *   - ΔU × ΔV × 2 ≤ maxMeshOutputPrimitives
 *
 * For most GPUs:
 *   - maxMeshOutputVertices = 256 (or 81 on some)
 *   - maxMeshOutputPrimitives = 256 (or 128 on some)
 *
 * Safe values:
 *   - For 256 limits: ΔU = ΔV = 15 → 256 vertices, 450 primitives
 *   - For 81 limits: ΔU = ΔV = 8 → 81 vertices, 128 primitives
 *
 * @param M Target M resolution
 * @param N Target N resolution
 * @param deltaU Output tile size in U direction
 * @param deltaV Output tile size in V direction
 */
void getDeltaUV(uint M, uint N, out uint deltaU, out uint deltaV) {
    // Hardware limits (conservative for compatibility)
    const uint maxVertices = 81;      // Can increase to 256 for newer GPUs
    const uint maxPrimitives = 128;   // Can increase to 256 for newer GPUs

    // Start with full resolution
    deltaU = M;
    deltaV = N;

    // Check vertex constraint: (ΔU + 1) × (ΔV + 1) ≤ maxVertices
    uint numVerts = (deltaU + 1) * (deltaV + 1);
    if (numVerts > maxVertices) {
        // Reduce to fit within vertex limit
        // Use square tiles: ΔU = ΔV
        uint maxDelta = uint(sqrt(float(maxVertices))) - 1;
        deltaU = min(deltaU, maxDelta);
        deltaV = min(deltaV, maxDelta);
    }

    // Check primitive constraint: ΔU × ΔV × 2 ≤ maxPrimitives
    uint numPrims = deltaU * deltaV * 2;
    if (numPrims > maxPrimitives) {
        // Reduce to fit within primitive limit
        uint maxDelta = uint(sqrt(float(maxPrimitives) / 2.0));
        deltaU = min(deltaU, maxDelta);
        deltaV = min(deltaV, maxDelta);
    }

    // Ensure at least 2×2 grid
    deltaU = max(deltaU, 2u);
    deltaV = max(deltaV, 2u);
}

// ============================================================================
// Main: Mapping Function F with Amplification K
// ============================================================================

void main() {
    uint globalId = gl_WorkGroupID.x;

    bool isVertex = (globalId >= push.nbFaces);

    uint faceId = 0;
    uint vertId = 0;

    if (isVertex) {
        vertId = globalId - push.nbFaces;
        int edge = readVertexEdge(vertId);
        if (edge >= 0) {
            faceId = uint(readHalfEdgeFace(uint(edge)));
        } else {
            faceId = 0;
        }
        payload.position = readVertexPosition(vertId);
        payload.normal = readVertexNormal(vertId);
        payload.area = readFaceArea(faceId);
    } else {
        faceId = globalId;
        payload.position = readFaceCenter(faceId);
        payload.normal = readFaceNormal(faceId);
        payload.area = readFaceArea(faceId);
    }

    payload.taskId = globalId;
    payload.isVertex = isVertex ? 1u : 0u;
    payload.elementType = push.elementType;
    payload.userScaling = push.userScaling;

    // ========================================================================
    // Amplification
    // ========================================================================

    // Read desired resolution
    uint M = push.resolutionM;
    uint N = push.resolutionN;

    payload.resolutionM = M;
    payload.resolutionN = N;

    // Compute tile size that fits within hardware limits
    uint deltaU, deltaV;
    getDeltaUV(M, N, deltaU, deltaV);

    payload.deltaU = deltaU;
    payload.deltaV = deltaV;

    // Compute number of mesh shader invocations needed
    // We need to tile the M×N grid into (deltaU × deltaV) tiles
    uint numTilesU = (M + deltaU - 1) / deltaU;  // ceil(M / deltaU)
    uint numTilesV = (N + deltaV - 1) / deltaV;  // ceil(N / deltaV)

    // Emit 2D grid of mesh shader invocations
    EmitMeshTasksEXT(numTilesU, numTilesV, 1);
}
```

### Step 2: Update Mesh Shader to Handle Tiles

This is a preview - full implementation in Feature 4.2. For now, add a check:

```glsl
// In parametric.mesh main():

void main() {
    // Read tile information from payload
    uint M = payload.resolutionM;
    uint N = payload.resolutionN;
    uint deltaU = payload.deltaU;
    uint deltaV = payload.deltaV;

    // For now, verify we're using correct tile size
    // (Feature 4.2 will implement actual tiling)
    if (deltaU != M || deltaV != N) {
        // Multi-tile mode not yet implemented
        // For now, just render the tile at reduced resolution
        M = deltaU;
        N = deltaV;
    }

    // ... rest of mesh shader code
}
```

### Step 3: Update Push Constants in C++

Update push constant structure:

```cpp
struct PushConstants {
    glm::mat4 mvp;
    uint32_t nbFaces;
    uint32_t nbVertices;
    uint32_t elementType;
    float userScaling;
    uint32_t debugMode;
    uint32_t resolutionM;
    uint32_t resolutionN;
};

PushConstants pushConstants;
pushConstants.mvp = projection * view * model;
pushConstants.nbFaces = halfEdgeMesh.nbFaces;
pushConstants.nbVertices = halfEdgeMesh.nbVertices;
pushConstants.elementType = resurfacingConfig.elementType;
pushConstants.userScaling = resurfacingConfig.userScaling;
pushConstants.debugMode = debugMode;
pushConstants.resolutionM = resurfacingConfig.resolutionM;
pushConstants.resolutionN = resurfacingConfig.resolutionN;

vkCmdPushConstants(commandBuffer, pipelineLayout,
                   VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT,
                   0, sizeof(pushConstants), &pushConstants);
```

### Step 4: Test Amplification Logic

Add debug output to verify amplification:

```cpp
// In C++ (optional debug)
uint32_t M = resurfacingConfig.resolutionM;
uint32_t N = resurfacingConfig.resolutionN;

// Compute deltaU, deltaV (same logic as shader)
const uint32_t maxVertices = 81;
const uint32_t maxPrimitives = 128;

uint32_t deltaU = M;
uint32_t deltaV = N;

uint32_t numVerts = (deltaU + 1) * (deltaV + 1);
if (numVerts > maxVertices) {
    uint32_t maxDelta = (uint32_t)sqrt(maxVertices) - 1;
    deltaU = std::min(deltaU, maxDelta);
    deltaV = std::min(deltaV, maxDelta);
}

uint32_t numPrims = deltaU * deltaV * 2;
if (numPrims > maxPrimitives) {
    uint32_t maxDelta = (uint32_t)sqrt(maxPrimitives / 2.0f);
    deltaU = std::min(deltaU, maxDelta);
    deltaV = std::min(deltaV, maxDelta);
}

uint32_t numTilesU = (M + deltaU - 1) / deltaU;
uint32_t numTilesV = (N + deltaV - 1) / deltaV;
uint32_t totalMeshTasks = numTilesU * numTilesV;

std::cout << "Amplification:" << std::endl;
std::cout << "  Target resolution: " << M << "×" << N << std::endl;
std::cout << "  Tile size: " << deltaU << "×" << deltaV << std::endl;
std::cout << "  Tiles per element: " << numTilesU << "×" << numTilesV
          << " = " << totalMeshTasks << std::endl;
```

### Step 5: Compile and Test

```bash
cd build
cmake --build .
./Gravel
```

## Acceptance Criteria

- [ ] Task shader compiles without errors
- [ ] getDeltaUV correctly computes tile size for various resolutions
- [ ] For 8×8: deltaU=8, deltaV=8, numTiles=1×1=1
- [ ] For 16×16: deltaU=8, deltaV=8, numTiles=2×2=4
- [ ] For 32×32: deltaU=8, deltaV=8, numTiles=4×4=16
- [ ] EmitMeshTasksEXT called with correct 2D grid dimensions
- [ ] No validation errors

## Expected Output

```
Amplification test:

Resolution 8×8:
  Tile size: 8×8
  Tiles: 1×1 = 1 mesh task
  Vertices: 81, Primitives: 128

Resolution 16×16:
  Tile size: 8×8
  Tiles: 2×2 = 4 mesh tasks
  Vertices per tile: 81, Primitives per tile: 128

Resolution 32×32:
  Tile size: 8×8
  Tiles: 4×4 = 16 mesh tasks
  Vertices per tile: 81, Primitives per tile: 128
```

## Troubleshooting

### Validation Error: Too Many Vertices

**Error**: `VUID-RuntimeSpirv-MeshEXT-07113`

**Fix**: Reduce maxVertices constant. Some GPUs only support 81, not 256.

### Wrong Number of Tiles

**Symptom**: Tiles don't match expected count.

**Fix**: Check ceiling division: `(M + deltaU - 1) / deltaU` not `M / deltaU`.

### Mesh Shader Not Receiving All Tiles

**Symptom**: Only some tiles render.

**Fix**: Ensure mesh shader reads `gl_WorkGroupID.xy` correctly (Feature 4.2).

### Task Payload Too Large

**Error**: Payload exceeds limits.

**Fix**: Current payload is ~60 bytes. Limit is typically 16KB. You're safe.

## Common Pitfalls

1. **Integer Division**: Use `(M + deltaU - 1) / deltaU` for ceiling, not floating-point ceil().

2. **Square Root**: `sqrt(float(maxVertices))` needs float cast, then convert back to uint.

3. **Minimum Tile Size**: Always ensure `deltaU >= 2` and `deltaV >= 2` to avoid degenerate grids.

4. **2D vs 3D Dispatch**: `EmitMeshTasksEXT(x, y, z)` - we use (numTilesU, numTilesV, 1).

5. **Payload Struct Alignment**: Add padding if needed to match std430 alignment.

## Validation Tests

### Test 1: Tile Size Computation

| M×N   | maxV=81 | ΔU×ΔV | Tiles | Total Verts | Total Prims |
|-------|---------|-------|-------|-------------|-------------|
| 8×8   | 81      | 8×8   | 1×1   | 81          | 128         |
| 16×16 | 81      | 8×8   | 2×2   | 81×4        | 128×4       |
| 32×32 | 81      | 8×8   | 4×4   | 81×16       | 128×16      |
| 64×64 | 81      | 8×8   | 8×8   | 81×64       | 128×64      |

### Test 2: Constraint Checking

For maxVertices=81, maxPrimitives=128:
- ΔU=8, ΔV=8: (8+1)×(8+1) = 81 ✓, 8×8×2 = 128 ✓
- ΔU=9, ΔV=9: (9+1)×(9+1) = 100 ✗ (exceeds vertex limit)

### Test 3: Edge Cases

- M=1, N=1: Should clamp to ΔU=2, ΔV=2 (minimum)
- M=7, N=7: ΔU=7, ΔV=7 → 64 verts, 98 prims ✓
- M=100, N=100: ΔU=8, ΔV=8 → 13×13 = 169 tiles

## Mathematical Background

### Amplification Factor

The amplification factor is the number of mesh shader invocations per task shader:

```
K = numTilesU × numTilesV
  = ⌈M / ΔU⌉ × ⌈N / ΔV⌉
```

For M=N and ΔU=ΔV:
```
K = ⌈M / Δ⌉²
```

### Ceiling Division

Integer ceiling division without floating-point:

```
⌈a / b⌉ = (a + b - 1) / b
```

Proof:
- If a % b == 0: (a + b - 1) / b = a/b
- If a % b != 0: (a + b - 1) / b = a/b + 1

### Hardware Limits

Typical GPU limits:
- NVIDIA RTX 30xx: 256 vertices, 256 primitives
- NVIDIA RTX 20xx: 256 vertices, 256 primitives
- AMD RDNA2: 256 vertices, 256 primitives
- Intel Arc: 256 vertices, 256 primitives
- Older GPUs: 81 vertices, 128 primitives (use conservative)

## Next Steps

Next feature: **[Feature 4.2 - Variable Resolution in Mesh Shader](feature-02-variable-resolution.md)**

You'll implement the actual tiling logic in the mesh shader to handle multiple tiles per element.

## Summary

You've implemented:
- ✅ getDeltaUV function to compute maximum tile size
- ✅ Constraint checking for vertex/primitive limits
- ✅ numMeshTasks computation with ceiling division
- ✅ TaskPayload extended with MN and ΔUV
- ✅ 2D mesh task dispatch via EmitMeshTasksEXT

The task shader can now emit multiple mesh shader invocations to handle high-resolution grids!
