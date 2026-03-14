#ifndef BSPLINE_GLSL
#define BSPLINE_GLSL

// ============================================================================
// Uniform Cubic B-Spline Evaluation
// ============================================================================

// Basis matrix for uniform cubic B-spline.
// GLSL mat4 is column-major; each column here equals the corresponding
// mathematical row of the B-spline matrix, so that B = M * T evaluates correctly.
//
// Mathematical form (B = M * [t³, t², t, 1]ᵀ):
//   row 0: [-1/6,  3/6, -3/6,  1/6]   (b0 = (-t³+3t²-3t+1)/6)
//   row 1: [ 3/6, -6/6,  3/6,  0  ]   (b1 = (3t³-6t²+4)/6)
//   row 2: [-3/6,  0,    3/6,  0  ]   (b2 = (-3t³+3t²+3t+1)/6)
//   row 3: [ 1/6,  4/6,  1/6,  0  ]   (b3 = t³/6)
const mat4 BSPLINE_MATRIX_4 = mat4(
    -1.0/6.0,  3.0/6.0, -3.0/6.0,  1.0/6.0,   // col 0 = row 0
     3.0/6.0, -6.0/6.0,  3.0/6.0,  0.0,        // col 1 = row 1
    -3.0/6.0,  0.0,       3.0/6.0,  0.0,        // col 2 = row 2
     1.0/6.0,  4.0/6.0,  1.0/6.0,  0.0         // col 3 = row 3
);

// Evaluate a cubic B-spline curve at parameter t in [0, 1] given 4 control points
vec3 computeBSplinePoint(float t, vec3 P0, vec3 P1, vec3 P2, vec3 P3) {
    vec4 T = vec4(t * t * t, t * t, t, 1.0);
    vec4 B = BSPLINE_MATRIX_4 * T;
    return B.x * P0 + B.y * P1 + B.z * P2 + B.w * P3;
}

// Evaluate a bicubic B-spline tensor product patch at (uv.x, uv.y) in [0,1]^2
// P[i][j]: control point at U-column i, V-row j.
// Inner loop: for each V row j, interpolate in U; outer: interpolate in V.
vec3 evaluateBSplinePatch(vec2 uv, vec3 P[4][4]) {
    vec3 Cu[4];
    for (int j = 0; j < 4; j++)
        Cu[j] = computeBSplinePoint(uv.x, P[0][j], P[1][j], P[2][j], P[3][j]);
    return computeBSplinePoint(uv.y, Cu[0], Cu[1], Cu[2], Cu[3]);
}

// Compute surface normal via finite differences
#define BSPLINE_NORMAL_OFFSET 0.001

vec3 computeFiniteDifferenceBspline(vec2 uv, vec3 P[4][4]) {
    vec3 dU = evaluateBSplinePatch(vec2(uv.x + BSPLINE_NORMAL_OFFSET, uv.y), P)
            - evaluateBSplinePatch(vec2(uv.x - BSPLINE_NORMAL_OFFSET, uv.y), P);
    vec3 dV = evaluateBSplinePatch(vec2(uv.x, uv.y + BSPLINE_NORMAL_OFFSET), P)
            - evaluateBSplinePatch(vec2(uv.x, uv.y - BSPLINE_NORMAL_OFFSET), P);
    return normalize(cross(dU, dV));
}

#endif // BSPLINE_GLSL
