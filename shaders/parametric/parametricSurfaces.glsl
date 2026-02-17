#ifndef PARAMETRIC_SURFACES_GLSL
#define PARAMETRIC_SURFACES_GLSL

#include "../common.glsl"

// ============================================================================
// Parametric Torus
// ============================================================================

// Torus equation:
//   x = (R + r*cos(v)) * cos(u)
//   y = (R + r*cos(v)) * sin(u)
//   z = r * sin(v)
//
// u, v in [0, 2pi], R = major radius, r = minor radius
// Normal computed from cross product of partial derivatives

void parametricTorus(vec2 uv, out vec3 pos, out vec3 normal, float majorR, float minorR) {
    float u = uv.x * 2.0 * PI;
    float v = uv.y * 2.0 * PI;

    float cosU = cos(u);
    float sinU = sin(u);
    float cosV = cos(v);
    float sinV = sin(v);

    float tubeRadius = majorR + minorR * cosV;
    pos.x = tubeRadius * cosU;
    pos.y = tubeRadius * sinU;
    pos.z = minorR * sinV;

    // Partial derivatives
    vec3 dpdu = vec3(
        -tubeRadius * sinU,
         tubeRadius * cosU,
         0.0
    );

    vec3 dpdv = vec3(
        -minorR * sinV * cosU,
        -minorR * sinV * sinU,
         minorR * cosV
    );

    normal = normalize(cross(dpdu, dpdv));
}

#endif // PARAMETRIC_SURFACES_GLSL
