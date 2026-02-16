# Feature 5.5: Bézier Surface Evaluation

**Epic**: [Epic 5 - B-Spline Control Cages](../../05/epic-05-bspline-cages.md)
**Estimated Time**: 2 hours
**Prerequisites**: [Feature 5.4 - B-Spline Surface](feature-04-bspline-surface.md)

## Goal

Implement Bézier surface evaluation with support for degrees 1-3 (bilinear, biquadratic, bicubic).

## What You'll Build

- Bernstein polynomial basis functions (degrees 1-3)
- parametricBezier() for surface evaluation
- Degree selection via ImGui

## Implementation

Update `shaders/parametric/parametricGrids.glsl`:

```glsl
// ============================================================================
// Bernstein Basis Functions
// ============================================================================

/**
 * Linear Bernstein polynomials (degree 1)
 */
vec2 bernsteinLinear(float t) {
    return vec2(1.0 - t, t);
}

/**
 * Quadratic Bernstein polynomials (degree 2)
 */
vec3 bernsteinQuadratic(float t) {
    float t2 = (1.0 - t);
    return vec3(t2 * t2, 2.0 * t2 * t, t * t);
}

/**
 * Cubic Bernstein polynomials (degree 3)
 */
vec4 bernsteinCubic(float t) {
    float t2 = (1.0 - t);
    float t3 = t2 * t2 * t2;

    return vec4(
        t3,                          // (1-t)³
        3.0 * t2 * t2 * t,          // 3(1-t)²t
        3.0 * t2 * t * t,           // 3(1-t)t²
        t * t * t                    // t³
    );
}

// ============================================================================
// Bézier Surface Evaluation
// ============================================================================

/**
 * Evaluate Bézier surface (bicubic)
 *
 * @param uv UV coordinates in [0, 1]
 * @param pos Output: Surface position
 * @param normal Output: Surface normal
 * @param Nx Grid width
 * @param Ny Grid height
 * @param degree Bézier degree (1=linear, 2=quadratic, 3=cubic)
 */
void parametricBezier(vec2 uv, out vec3 pos, out vec3 normal,
                      uint Nx, uint Ny, uint degree) {
    vec2 gridUV = uv * vec2(float(Nx - 1), float(Ny - 1));

    int u0 = int(floor(gridUV.x));
    int v0 = int(floor(gridUV.y));

    float tu = fract(gridUV.x);
    float tv = fract(gridUV.y);

    pos = vec3(0.0);

    if (degree == 3) {
        // Bicubic Bézier
        vec4 Bu = bernsteinCubic(tu);
        vec4 Bv = bernsteinCubic(tv);

        for (int j = 0; j < 4; j++) {
            vec3 rowSum = vec3(0.0);
            for (int i = 0; i < 4; i++) {
                vec3 P = sampleControlPoint(u0 + i, v0 + j, Nx, Ny, false, false);
                rowSum += Bu[i] * P;
            }
            pos += Bv[j] * rowSum;
        }
    } else if (degree == 1) {
        // Bilinear
        vec2 Bu = bernsteinLinear(tu);
        vec2 Bv = bernsteinLinear(tv);

        vec3 P00 = sampleControlPoint(u0, v0, Nx, Ny, false, false);
        vec3 P10 = sampleControlPoint(u0 + 1, v0, Nx, Ny, false, false);
        vec3 P01 = sampleControlPoint(u0, v0 + 1, Nx, Ny, false, false);
        vec3 P11 = sampleControlPoint(u0 + 1, v0 + 1, Nx, Ny, false, false);

        pos = Bu[0] * Bv[0] * P00 +
              Bu[1] * Bv[0] * P10 +
              Bu[0] * Bv[1] * P01 +
              Bu[1] * Bv[1] * P11;
    }

    // Compute normal (same as B-spline)
    const float epsilon = 0.01;
    vec3 posU, posV, dummyNormal;

    parametricBezier(uv + vec2(epsilon, 0.0), posU, dummyNormal, Nx, Ny, degree);
    parametricBezier(uv + vec2(0.0, epsilon), posV, dummyNormal, Nx, Ny, degree);

    vec3 dpdu = (posU - pos) / epsilon;
    vec3 dpdv = (posV - pos) / epsilon;

    normal = normalize(cross(dpdu, dpdv));
}
```

## ImGui Controls

Add degree selection:

```cpp
int bezierDegree = 3;  // 1=linear, 2=quadratic, 3=cubic

ImGui::SliderInt("Bézier Degree", &bezierDegree, 1, 3);
const char* degreeNames[] = {"", "Bilinear", "Biquadratic", "Bicubic"};
ImGui::Text("Mode: %s", degreeNames[bezierDegree]);

resurfacingConfig.bezierDegree = bezierDegree;
```

## Acceptance Criteria

- [ ] Bicubic Bézier renders smoothly
- [ ] Bilinear Bézier shows faceted appearance
- [ ] Bézier INTERPOLATES corner control points
- [ ] Degree slider works

## Differences from B-Spline

| Property          | B-Spline         | Bézier           |
|-------------------|------------------|------------------|
| Passes through CPs| No              | Corners only     |
| Smoothness        | C²              | C¹ at patches    |
| Local support     | 4×4             | Patch-sized      |
| Typical use       | Smooth surfaces | Interpolation    |

## Next Steps

Next feature: **[Feature 5.6 - Integration and Boundary Modes](feature-06-integration.md)**
