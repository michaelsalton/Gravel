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

// ============================================================================
// Control Point Sampling (with boundary handling)
// ============================================================================

/**
 * Sample a control point from the LUT buffer with clamp or cyclic wrap.
 *
 * @param u      Grid U index (may be outside [0, Nx-1])
 * @param v      Grid V index (may be outside [0, Ny-1])
 * @param Nx     Grid width  (U direction)
 * @param Ny     Grid height (V direction)
 * @param cyclicU  Wrap in U when true, clamp when false
 * @param cyclicV  Wrap in V when true, clamp when false
 * @return       Control point world position
 */
vec3 sampleControlPoint(int u, int v, uint Nx, uint Ny, bool cyclicU, bool cyclicV) {
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

    return sampleLUT(uint(v) * Nx + uint(u));
}

// ============================================================================
// Bicubic B-Spline Surface Evaluation
// ============================================================================

/**
 * Evaluate a bicubic B-spline surface at UV coordinates.
 *
 * Maps uv ∈ [0,1]² to the grid space [0,Nx] × [0,Ny], selects the 4×4
 * control point neighbourhood, and evaluates position + normal analytically
 * using both the basis weights and their derivatives.
 *
 * This avoids finite differences and the recursion the doc suggests, which
 * is not allowed in SPIR-V.
 *
 * @param uv       Surface parameter in [0,1]²
 * @param pos      Output: surface position
 * @param normal   Output: surface normal (unit length)
 * @param Nx       Grid width
 * @param Ny       Grid height
 * @param cyclicU  Wrap U boundary
 * @param cyclicV  Wrap V boundary
 */
void parametricBSpline(vec2 uv, out vec3 pos, out vec3 normal,
                       uint Nx, uint Ny, bool cyclicU, bool cyclicV) {
    // Fallback when no LUT is loaded
    if (Nx == 0u || Ny == 0u) {
        float theta = uv.x * 2.0 * PI;
        float phi   = uv.y * PI;
        pos    = 0.5 * vec3(sin(phi) * cos(theta), sin(phi) * sin(theta), cos(phi));
        normal = normalize(pos);
        return;
    }

    // Map UV [0,1] → grid space [0,Nx] × [0,Ny]
    vec2  gridUV = uv * vec2(float(Nx), float(Ny));
    int   u0     = int(floor(gridUV.x));
    int   v0     = int(floor(gridUV.y));
    float tu     = fract(gridUV.x);
    float tv     = fract(gridUV.y);

    // Basis weights and their derivatives w.r.t. the local parameter
    vec4 Bu  = bsplineBasis(tu);
    vec4 dBu = bsplineBasisDerivative(tu);
    vec4 Bv  = bsplineBasis(tv);
    vec4 dBv = bsplineBasisDerivative(tv);

    // Sample 4×4 neighbourhood centred on (u0, v0)
    // B-spline spans use offset -1: indices [u0-1 .. u0+2]
    vec3 P[4][4];
    for (int j = 0; j < 4; j++)
        for (int i = 0; i < 4; i++)
            P[j][i] = sampleControlPoint(u0 + i - 1, v0 + j - 1,
                                         Nx, Ny, cyclicU, cyclicV);

    // Single pass: accumulate position, dS/du, dS/dv simultaneously
    pos         = vec3(0.0);
    vec3 dpdu   = vec3(0.0);
    vec3 dpdv   = vec3(0.0);

    for (int j = 0; j < 4; j++) {
        vec3 rowPos  = vec3(0.0);  // sum_i Bu[i]  * P[j][i]
        vec3 rowDerU = vec3(0.0);  // sum_i dBu[i] * P[j][i]
        for (int i = 0; i < 4; i++) {
            rowPos  += Bu[i]  * P[j][i];
            rowDerU += dBu[i] * P[j][i];
        }
        pos  += Bv[j]  * rowPos;
        dpdv += dBv[j] * rowPos;
        dpdu += Bv[j]  * rowDerU;
    }

    // Cross product → unit normal; safe fallback for degenerate tangents
    vec3  n    = cross(dpdu, dpdv);
    float lenN = length(n);
    normal = (lenN > 1e-6) ? n / lenN : vec3(0.0, 0.0, 1.0);
}

// ============================================================================
// Bernstein Basis Functions (for Bézier surfaces)
// ============================================================================

/**
 * Linear Bernstein polynomials (degree 1).
 *   B0(t) = 1 - t
 *   B1(t) = t
 */
vec2 bernsteinLinear(float t) {
    return vec2(1.0 - t, t);
}

/**
 * Quadratic Bernstein polynomials (degree 2).
 *   B0(t) = (1-t)²
 *   B1(t) = 2(1-t)t
 *   B2(t) = t²
 */
vec3 bernsteinQuadratic(float t) {
    float omt = 1.0 - t;
    return vec3(omt * omt, 2.0 * omt * t, t * t);
}

/**
 * Cubic Bernstein polynomials (degree 3).
 *   B0(t) = (1-t)³
 *   B1(t) = 3(1-t)²t
 *   B2(t) = 3(1-t)t²
 *   B3(t) = t³
 */
vec4 bernsteinCubic(float t) {
    float omt = 1.0 - t;
    return vec4(omt*omt*omt, 3.0*omt*omt*t, 3.0*omt*t*t, t*t*t);
}

/**
 * Derivatives of linear Bernstein polynomials.
 *   B0'(t) = -1,  B1'(t) = 1
 */
vec2 bernsteinLinearDeriv(float t) {
    return vec2(-1.0, 1.0);
}

/**
 * Derivatives of quadratic Bernstein polynomials.
 *   B0'(t) = -2(1-t)
 *   B1'(t) = 2(1-2t)
 *   B2'(t) = 2t
 */
vec3 bernsteinQuadraticDeriv(float t) {
    return vec3(-2.0*(1.0-t), 2.0*(1.0-2.0*t), 2.0*t);
}

/**
 * Derivatives of cubic Bernstein polynomials.
 *   B0'(t) = -3(1-t)²
 *   B1'(t) = 3(1-t)(1-3t)
 *   B2'(t) = 3t(2-3t)
 *   B3'(t) = 3t²
 */
vec4 bernsteinCubicDeriv(float t) {
    float omt = 1.0 - t;
    return vec4(-3.0*omt*omt, 3.0*omt*(1.0-3.0*t), 3.0*t*(2.0-3.0*t), 3.0*t*t);
}

// ============================================================================
// Bézier Surface Evaluation
// ============================================================================

/**
 * Evaluate a Bézier surface at UV coordinates.
 *
 * Supports degree 1 (bilinear), 2 (biquadratic), and 3 (bicubic).
 * Normals are computed analytically via cross(dS/du, dS/dv), avoiding
 * finite differences and recursive calls (which SPIR-V does not allow).
 *
 * @param uv     Surface parameter in [0,1]²
 * @param pos    Output: surface position
 * @param normal Output: surface normal (unit length)
 * @param Nx     Grid width  (control points in U)
 * @param Ny     Grid height (control points in V)
 * @param degree Bézier degree: 1, 2, or 3
 */
void parametricBezier(vec2 uv, out vec3 pos, out vec3 normal,
                      uint Nx, uint Ny, uint degree) {
    // Fallback when no LUT is loaded
    if (Nx == 0u || Ny == 0u) {
        float theta = uv.x * 2.0 * PI;
        float phi   = uv.y * PI;
        pos    = 0.5 * vec3(sin(phi)*cos(theta), sin(phi)*sin(theta), cos(phi));
        normal = normalize(pos);
        return;
    }

    // Map UV [0,1] → grid space [0, Nx-1] × [0, Ny-1]
    vec2  gridUV = uv * vec2(float(Nx - 1u), float(Ny - 1u));
    int   u0     = int(floor(gridUV.x));
    int   v0     = int(floor(gridUV.y));
    float tu     = fract(gridUV.x);
    float tv     = fract(gridUV.y);

    pos         = vec3(0.0);
    vec3 dpdu   = vec3(0.0);
    vec3 dpdv   = vec3(0.0);

    if (degree == 3u) {
        vec4 Bu  = bernsteinCubic(tu);
        vec4 Bv  = bernsteinCubic(tv);
        vec4 dBu = bernsteinCubicDeriv(tu);
        vec4 dBv = bernsteinCubicDeriv(tv);

        for (int j = 0; j < 4; j++) {
            vec3 rowPos  = vec3(0.0);
            vec3 rowDerU = vec3(0.0);
            for (int i = 0; i < 4; i++) {
                vec3 P  = sampleControlPoint(u0+i, v0+j, Nx, Ny, false, false);
                rowPos  += Bu[i]  * P;
                rowDerU += dBu[i] * P;
            }
            pos  += Bv[j]  * rowPos;
            dpdv += dBv[j] * rowPos;
            dpdu += Bv[j]  * rowDerU;
        }
    } else if (degree == 2u) {
        vec3 Bu  = bernsteinQuadratic(tu);
        vec3 Bv  = bernsteinQuadratic(tv);
        vec3 dBu = bernsteinQuadraticDeriv(tu);
        vec3 dBv = bernsteinQuadraticDeriv(tv);

        for (int j = 0; j < 3; j++) {
            vec3 rowPos  = vec3(0.0);
            vec3 rowDerU = vec3(0.0);
            for (int i = 0; i < 3; i++) {
                vec3 P  = sampleControlPoint(u0+i, v0+j, Nx, Ny, false, false);
                rowPos  += Bu[i]  * P;
                rowDerU += dBu[i] * P;
            }
            pos  += Bv[j]  * rowPos;
            dpdv += dBv[j] * rowPos;
            dpdu += Bv[j]  * rowDerU;
        }
    } else {
        // degree == 1: bilinear
        vec2 Bu  = bernsteinLinear(tu);
        vec2 Bv  = bernsteinLinear(tv);

        vec3 P00 = sampleControlPoint(u0,   v0,   Nx, Ny, false, false);
        vec3 P10 = sampleControlPoint(u0+1, v0,   Nx, Ny, false, false);
        vec3 P01 = sampleControlPoint(u0,   v0+1, Nx, Ny, false, false);
        vec3 P11 = sampleControlPoint(u0+1, v0+1, Nx, Ny, false, false);

        pos  = Bu[0]*Bv[0]*P00 + Bu[1]*Bv[0]*P10
             + Bu[0]*Bv[1]*P01 + Bu[1]*Bv[1]*P11;
        dpdu = Bv[0]*(P10-P00) + Bv[1]*(P11-P01);
        dpdv = Bu[0]*(P01-P00) + Bu[1]*(P11-P10);
    }

    vec3  n    = cross(dpdu, dpdv);
    float lenN = length(n);
    normal = (lenN > 1e-6) ? n / lenN : vec3(0.0, 0.0, 1.0);
}

#endif // PARAMETRIC_GRIDS_GLSL
