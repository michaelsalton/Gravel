# Feature 6.4: Pebble B-Spline Evaluation

**Epic**: [Epic 6 - Pebble Generation](../../06/epic-06-pebble-generation.md)
**Estimated Time**: 2-3 hours
**Prerequisites**: [Feature 6.3 - Procedural Control Cage](feature-03-control-cage-construction.md)

## Goal

Evaluate B-spline surface using the local control cage to generate smooth pebble geometry.

## Implementation

Update `shaders/pebbles/pebble.mesh`:

```glsl
// After control cage construction and barrier...

// ========================================================================
// Step 2: Evaluate B-Spline Surface
// ========================================================================

uint M = 1 << payload.subdivisionLevel;  // 2^level
uint N = M;

uint numVerts = (M + 1) * (M + 1);
uint numPrims = M * M * 2;

SetMeshOutputsEXT(numVerts, numPrims);

// Generate vertices
for (uint i = localId; i < numVerts; i += 32) {
    uint u = i % (M + 1);
    uint v = i / (M + 1);
    vec2 uv = vec2(u, v) / vec2(M, M);

    // Evaluate B-spline surface using local control cage
    vec3 pos = evaluateBSplineLocal(uv, controlCage);

    // Output
    vOut[i].worldPosU = vec4(pos, uv.x);
    vOut[i].normalV = vec4(0, 0, 1, uv.y);  // Normal computed later

    gl_MeshVerticesEXT[i].gl_Position = vec4(pos, 1.0);
}

// Generate primitives
uint numQuads = M * M;
for (uint q = localId; q < numQuads; q += 32) {
    uint qu = q % M;
    uint qv = q / M;

    uint v00 = qv * (M + 1) + qu;
    uint v10 = v00 + 1;
    uint v01 = v00 + (M + 1);
    uint v11 = v01 + 1;

    uint triId0 = 2 * q + 0;
    uint triId1 = 2 * q + 1;

    gl_PrimitiveTriangleIndicesEXT[triId0] = uvec3(v00, v10, v11);
    gl_PrimitiveTriangleIndicesEXT[triId1] = uvec3(v00, v11, v01);
}

// Helper function
vec3 evaluateBSplineLocal(vec2 uv, vec3 cage[16]) {
    vec2 gridUV = uv * 3.0;  // 4×4 cage = 3×3 patches

    int u0 = int(floor(gridUV.x));
    int v0 = int(floor(gridUV.y));

    float tu = fract(gridUV.x);
    float tv = fract(gridUV.y);

    vec4 Bu = bsplineBasis(tu);
    vec4 Bv = bsplineBasis(tv);

    vec3 pos = vec3(0);
    for (int j = 0; j < 4; j++) {
        vec3 rowSum = vec3(0);
        for (int i = 0; i < 4; i++) {
            // Sample from local cage (clamp to avoid out of bounds)
            int cageU = clamp(u0 + i - 1, 0, 3);
            int cageV = clamp(v0 + j - 1, 0, 3);
            vec3 P = cage[cageV * 4 + cageU];

            rowSum += Bu[i] * P;
        }
        pos += Bv[j] * rowSum;
    }

    return pos;
}
```

## Acceptance Criteria

- [ ] Pebbles render as smooth B-spline surfaces
- [ ] Higher subdivision = more detail
- [ ] 4×4 control cage produces smooth result
- [ ] No gaps or overlaps

## Next Steps

Next feature: **[Feature 6.5 - Procedural Noise](feature-05-procedural-noise.md)**
