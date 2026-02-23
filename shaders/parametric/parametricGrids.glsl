#ifndef PARAMETRIC_GRIDS_GLSL
#define PARAMETRIC_GRIDS_GLSL

#include "../common.glsl"

// ============================================================================
// Cubic B-Spline Basis Functions
// ============================================================================

/**
 * Evaluate cubic B-spline basis functions.
 *
 * Cubic B-spline (degree 3):
 *   B0(t) = (1-t)³ / 6
 *   B1(t) = (3t³ - 6t² + 4) / 6
 *   B2(t) = (-3t³ + 3t² + 3t + 1) / 6
 *   B3(t) = t³ / 6
 *
 * Properties:
 *   - B0 + B1 + B2 + B3 = 1 for all t ∈ [0,1]  (partition of unity)
 *   - C² continuous (smooth second derivative)
 *   - Local support: exactly 4 control points per evaluation point
 *
 * @param t  Parameter in [0, 1]
 * @return   vec4(B0, B1, B2, B3) basis weights
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
 * Evaluate cubic B-spline basis function derivatives.
 *
 * Used for computing surface normals via the cross product of
 * partial derivatives dS/du and dS/dv.
 *
 * B0'(t) = -(1-t)² / 2
 * B1'(t) = (3t² - 4t) / 2
 * B2'(t) = (-3t² + 2t + 1) / 2
 * B3'(t) = t² / 2
 *
 * @param t  Parameter in [0, 1]
 * @return   vec4(B0', B1', B2', B3') basis derivatives
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
