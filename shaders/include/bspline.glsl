#ifndef BSPLINE_GLSL                             // Include guard: skip if already included this compilation unit
#define BSPLINE_GLSL

// =============================================================================
// bspline.glsl — Uniform cubic B-spline evaluation
//
// Evaluates uniform cubic B-spline curves and bicubic tensor-product patches
// from control points, plus a finite-difference normal. Used by the dragon-
// scale surface generator (parametricSurfaces.glsl) to turn a 4x4 control grid
// (the scale LUT) into a smooth surface point and normal.
// =============================================================================

// *** Michael Salton ***

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
const mat4 BSPLINE_MATRIX_4 = mat4(              // Cubic B-spline basis (stored column-major, see note above)
    -1.0/6.0,  3.0/6.0, -3.0/6.0,  1.0/6.0,   // col 0 = row 0
     3.0/6.0, -6.0/6.0,  3.0/6.0,  0.0,        // col 1 = row 1
    -3.0/6.0,  0.0,       3.0/6.0,  0.0,        // col 2 = row 2
     1.0/6.0,  4.0/6.0,  1.0/6.0,  0.0         // col 3 = row 3
);

// Evaluate a cubic B-spline curve at parameter t in [0, 1] given 4 control points
vec3 computeBSplinePoint(float t, vec3 P0, vec3 P1, vec3 P2, vec3 P3) { // Point on a cubic B-spline curve at t
    vec4 T = vec4(t * t * t, t * t, t, 1.0);     // Power basis [t³, t², t, 1]
    vec4 B = BSPLINE_MATRIX_4 * T;               // B-spline blending weights for the 4 control points
    return B.x * P0 + B.y * P1 + B.z * P2 + B.w * P3; // Weighted sum of control points
}

// Evaluate a bicubic B-spline tensor product patch at (uv.x, uv.y) in [0,1]^2
// P[i][j]: control point at U-column i, V-row j.
// Inner loop: for each V row j, interpolate in U; outer: interpolate in V.
vec3 evaluateBSplinePatch(vec2 uv, vec3 P[4][4]) { // Point on a bicubic B-spline surface patch
    vec3 Cu[4];                                  // Per-row curve evaluations along U
    for (int j = 0; j < 4; j++)                  // For each of the 4 V rows...
        Cu[j] = computeBSplinePoint(uv.x, P[0][j], P[1][j], P[2][j], P[3][j]); // ...evaluate the row's curve at uv.x
    return computeBSplinePoint(uv.y, Cu[0], Cu[1], Cu[2], Cu[3]); // Then interpolate those 4 results along V at uv.y
}

// Compute surface normal via finite differences
#define BSPLINE_NORMAL_OFFSET 0.001              // Small UV step used to sample neighboring surface points

vec3 computeFiniteDifferenceBspline(vec2 uv, vec3 P[4][4]) { // Approximate the patch normal numerically
    vec3 dU = evaluateBSplinePatch(vec2(uv.x + BSPLINE_NORMAL_OFFSET, uv.y), P) // Central difference along U
            - evaluateBSplinePatch(vec2(uv.x - BSPLINE_NORMAL_OFFSET, uv.y), P);
    vec3 dV = evaluateBSplinePatch(vec2(uv.x, uv.y + BSPLINE_NORMAL_OFFSET), P) // Central difference along V
            - evaluateBSplinePatch(vec2(uv.x, uv.y - BSPLINE_NORMAL_OFFSET), P);
    return normalize(cross(dU, dV));             // Normal = normalized cross of the two surface tangents
}

#endif // BSPLINE_GLSL

// *** ************ ***
