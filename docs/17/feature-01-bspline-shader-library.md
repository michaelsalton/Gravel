# Feature 17.1: B-Spline Shader Library

**Epic**: [Epic 17 - Dragon Scale](epic-17-dragon-scale.md)

## Goal

Create `shaders/include/bspline.glsl` — a shared GLSL library for bicubic B-spline patch evaluation. Used exclusively by the dragon scale surface type; no other surface types are affected.

## File Created

- `shaders/include/bspline.glsl`

## Functions to Implement

### B-Spline Basis Matrix

Uniform cubic B-spline (non-clamped):

```glsl
const mat4 BSPLINE_MATRIX_4 = mat4(
    -1.0/6.0,  3.0/6.0, -3.0/6.0,  1.0/6.0,
     3.0/6.0, -6.0/6.0,  3.0/6.0,  0.0,
    -3.0/6.0,  0.0,       3.0/6.0,  0.0,
     1.0/6.0,  4.0/6.0,  1.0/6.0,  0.0
);
```

> Note: column-major in GLSL. Row order above is mathematical row order before transposing.

### 1D Cubic B-Spline Curve Point

```glsl
vec3 computeBSplinePoint(float t, vec3 P0, vec3 P1, vec3 P2, vec3 P3) {
    vec4 T = vec4(t*t*t, t*t, t, 1.0);
    vec4 B = BSPLINE_MATRIX_4 * T;
    return B.x * P0 + B.y * P1 + B.z * P2 + B.w * P3;
}
```

### Tensor Product Patch Evaluation

```glsl
vec3 evaluateBSplinePatch(vec2 uv, vec3 P[4][4]) {
    vec3 Q[4];
    for (int i = 0; i < 4; i++)
        Q[i] = computeBSplinePoint(uv.x, P[i][0], P[i][1], P[i][2], P[i][3]);
    return computeBSplinePoint(uv.y, Q[0], Q[1], Q[2], Q[3]);
}
```

### Finite Difference Normal

Uses central differences at offset 0.001 to avoid analytic derivative complexity:

```glsl
#define NORMAL_OFFSET 0.001

vec3 computeFiniteDifferenceBspline(vec2 uv, vec3 P[4][4]) {
    vec3 dU = evaluateBSplinePatch(vec2(uv.x + NORMAL_OFFSET, uv.y))
            - evaluateBSplinePatch(vec2(uv.x - NORMAL_OFFSET, uv.y));
    vec3 dV = evaluateBSplinePatch(vec2(uv.x, uv.y + NORMAL_OFFSET))
            - evaluateBSplinePatch(vec2(uv.x, uv.y - NORMAL_OFFSET));
    return normalize(cross(dU, dV));
}
```

## Notes

- `#pragma once` / include guard: use `#ifndef BSPLINE_GLSL` guard to match other includes in the project
- This file has no UBO or SSBO dependencies — it is pure math, safe to include anywhere
- `computeFiniteDifferenceBspline()` calls `evaluateBSplinePatch()` four times. This is acceptable given the dragon scale mesh shader already has 256 vertex threads; normal evaluation only occurs once per vertex
