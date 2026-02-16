# Feature 5.3: B-Spline Basis Functions

**Epic**: [Epic 5 - B-Spline Control Cages](../../05/epic-05-bspline-cages.md)
**Estimated Time**: 1-2 hours
**Prerequisites**: [Feature 5.2 - Control Cage GPU Upload](feature-02-control-cage-upload.md)

## Goal

Implement cubic B-spline basis functions that will be used for smooth surface evaluation.

## What You'll Build

- bsplineBasis(t) function returning vec4 weights
- Cubic B-spline formula (degree 3)
- Optional: bsplineBasisDerivative(t) for normals
- Basis function validation (weights sum to 1.0)

## Implementation

Create `shaders/parametric/parametricGrids.glsl`:

```glsl
#ifndef PARAMETRIC_GRIDS_GLSL
#define PARAMETRIC_GRIDS_GLSL

#include "../common.glsl"

// ============================================================================
// Cubic B-Spline Basis Functions
// ============================================================================

/**
 * Evaluate cubic B-spline basis functions
 *
 * Cubic B-spline (degree 3) formula:
 *   B0(t) = (1-t)³ / 6
 *   B1(t) = (3t³ - 6t² + 4) / 6
 *   B2(t) = (-3t³ + 3t² + 3t + 1) / 6
 *   B3(t) = t³ / 6
 *
 * Where t ∈ [0, 1]
 *
 * Properties:
 *   - Sum: B0 + B1 + B2 + B3 = 1 for all t
 *   - C² continuous (smooth second derivative)
 *   - Local support: only 4 control points affect any point
 *
 * @param t Parameter in [0, 1]
 * @return vec4(B0, B1, B2, B3) - basis weights
 */
vec4 bsplineBasis(float t) {
    float t2 = t * t;
    float t3 = t2 * t;

    float B0 = (1.0 - t) * (1.0 - t) * (1.0 - t) / 6.0;
    float B1 = (3.0 * t3 - 6.0 * t2 + 4.0) / 6.0;
    float B2 = (-3.0 * t3 + 3.0 * t2 + 3.0 * t + 1.0) / 6.0;
    float B3 = t3 / 6.0;

    return vec4(B0, B1, B2, B3);
}

/**
 * Evaluate cubic B-spline basis function derivatives
 *
 * Used for computing surface normals via partial derivatives.
 *
 * B0'(t) = -(1-t)² / 2
 * B1'(t) = (3t² - 4t) / 2
 * B2'(t) = (-3t² + 2t + 1) / 2
 * B3'(t) = t² / 2
 *
 * @param t Parameter in [0, 1]
 * @return vec4(B0', B1', B2', B3') - basis derivatives
 */
vec4 bsplineBasisDerivative(float t) {
    float t2 = t * t;

    float dB0 = -(1.0 - t) * (1.0 - t) / 2.0;
    float dB1 = (3.0 * t2 - 4.0 * t) / 2.0;
    float dB2 = (-3.0 * t2 + 2.0 * t + 1.0) / 2.0;
    float dB3 = t2 / 2.0;

    return vec4(dB0, dB1, dB2, dB3);
}

#endif // PARAMETRIC_GRIDS_GLSL
```

## Validation Test

Test basis functions:

```glsl
// Test that basis weights sum to 1.0
void testBSplineBasis() {
    for (float t = 0.0; t <= 1.0; t += 0.1) {
        vec4 B = bsplineBasis(t);
        float sum = B.x + B.y + B.z + B.w;

        // Should be ~1.0
        if (abs(sum - 1.0) > 0.001) {
            // ERROR: Basis functions don't sum to 1
        }
    }
}
```

## Acceptance Criteria

- [ ] Basis functions compile without errors
- [ ] Weights sum to 1.0 for all t ∈ [0, 1]
- [ ] B0(0) ≈ 0.167, B1(0) ≈ 0.667
- [ ] B2(1) ≈ 0.667, B3(1) ≈ 0.167
- [ ] No NaN or inf values

## Expected Values

| t   | B0     | B1     | B2     | B3     | Sum   |
|-----|--------|--------|--------|--------|-------|
| 0.0 | 0.1667 | 0.6667 | 0.1667 | 0.0000 | 1.000 |
| 0.5 | 0.0208 | 0.4792 | 0.4792 | 0.0208 | 1.000 |
| 1.0 | 0.0000 | 0.1667 | 0.6667 | 0.1667 | 1.000 |

## Next Steps

Next feature: **[Feature 5.4 - B-Spline Surface Evaluation](feature-04-bspline-surface.md)**
