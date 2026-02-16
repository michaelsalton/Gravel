# Feature 5.4: B-Spline Surface Evaluation

**Epic**: [Epic 5 - B-Spline Control Cages](../../05/epic-05-bspline-cages.md)
**Estimated Time**: 3-4 hours
**Prerequisites**: [Feature 5.3 - B-Spline Basis Functions](feature-03-bspline-basis.md)

## Goal

Implement bicubic B-spline surface evaluation using the control cage and basis functions.

## What You'll Build

- sampleControlPoint() with cyclic/non-cyclic boundaries
- parametricBSpline() for surface evaluation
- UV to grid space mapping
- 4×4 control point neighborhood evaluation
- Normal computation via finite differences

## Implementation

Update `shaders/parametric/parametricGrids.glsl`:

```glsl
// ============================================================================
// Control Point Sampling
// ============================================================================

/**
 * Sample control point from LUT with boundary handling
 *
 * @param u Grid U coordinate (may be outside [0, Nx-1])
 * @param v Grid V coordinate (may be outside [0, Ny-1])
 * @param Nx Grid width
 * @param Ny Grid height
 * @param cyclicU Wrap in U (true) or clamp (false)
 * @param cyclicV Wrap in V (true) or clamp (false)
 * @return Control point position
 */
vec3 sampleControlPoint(int u, int v, uint Nx, uint Ny, bool cyclicU, bool cyclicV) {
    // Handle boundaries
    if (cyclicU) {
        u = u % int(Nx);
        if (u < 0) u += int(Nx);
    } else {
        u = clamp(u, 0, int(Nx) - 1);
    }

    if (cyclicV) {
        v = v % int(Ny);
        if (v < 0) v += int(Ny);
    } else {
        v = clamp(v, 0, int(Ny) - 1);
    }

    // Row-major indexing
    uint index = uint(v) * Nx + uint(u);
    return sampleLUT(index);
}

// ============================================================================
// B-Spline Surface Evaluation
// ============================================================================

/**
 * Evaluate bicubic B-spline surface
 *
 * @param uv UV coordinates in [0, 1]
 * @param pos Output: Surface position
 * @param normal Output: Surface normal
 * @param Nx Grid width
 * @param Ny Grid height
 * @param cyclicU Wrap U boundary
 * @param cyclicV Wrap V boundary
 */
void parametricBSpline(vec2 uv, out vec3 pos, out vec3 normal,
                       uint Nx, uint Ny, bool cyclicU, bool cyclicV) {
    // Map UV [0,1] to grid space
    vec2 gridUV = uv * vec2(float(Nx), float(Ny));

    // Integer grid cell
    int u0 = int(floor(gridUV.x));
    int v0 = int(floor(gridUV.y));

    // Local parameter within cell [0,1]
    float tu = fract(gridUV.x);
    float tv = fract(gridUV.y));

    // Evaluate basis functions
    vec4 Bu = bsplineBasis(tu);
    vec4 Bv = bsplineBasis(tv);

    // Sample 4×4 control point neighborhood
    // B-spline uses (u-1, u, u+1, u+2) not (u, u+1, u+2, u+3)
    vec3 P[4][4];
    for (int j = 0; j < 4; j++) {
        for (int i = 0; i < 4; i++) {
            P[j][i] = sampleControlPoint(u0 + i - 1, v0 + j - 1, Nx, Ny, cyclicU, cyclicV);
        }
    }

    // Bicubic evaluation: sum(Bu[i] * Bv[j] * P[i][j])
    pos = vec3(0.0);
    for (int j = 0; j < 4; j++) {
        vec3 rowSum = vec3(0.0);
        for (int i = 0; i < 4; i++) {
            rowSum += Bu[i] * P[j][i];
        }
        pos += Bv[j] * rowSum;
    }

    // Compute normal via finite differences
    const float epsilon = 0.01;
    vec3 posU, posV, dummyNormal;

    parametricBSpline(uv + vec2(epsilon, 0.0), posU, dummyNormal, Nx, Ny, cyclicU, cyclicV);
    parametricBSpline(uv + vec2(0.0, epsilon), posV, dummyNormal, Nx, Ny, cyclicU, cyclicV);

    vec3 dpdu = (posU - pos) / epsilon;
    vec3 dpdv = (posV - pos) / epsilon;

    normal = normalize(cross(dpdu, dpdv));
}
```

## Acceptance Criteria

- [ ] B-spline surfaces render smoothly
- [ ] Surfaces approximate control cage shape
- [ ] Cyclic boundaries wrap seamlessly
- [ ] Non-cyclic boundaries clamp correctly
- [ ] Normals point outward
- [ ] No NaN or artifacts

## Expected Behavior

For 4×4 scale cage:
- Surface is smooth (C² continuous)
- Does NOT pass through control points
- Approximates control cage shape
- Bounding box smaller than control cage

## Next Steps

Next feature: **[Feature 5.5 - Bézier Surface Evaluation](feature-05-bezier-surface.md)**
