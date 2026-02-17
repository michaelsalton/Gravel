#ifndef PARAMETRIC_SURFACES_GLSL
#define PARAMETRIC_SURFACES_GLSL

#include "../common.glsl"

// ============================================================================
// Parametric Torus
// ============================================================================

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

    vec3 dpdu = vec3(-tubeRadius * sinU, tubeRadius * cosU, 0.0);
    vec3 dpdv = vec3(-minorR * sinV * cosU, -minorR * sinV * sinU, minorR * cosV);
    normal = normalize(cross(dpdu, dpdv));
}

// ============================================================================
// Parametric Sphere
// ============================================================================

void parametricSphere(vec2 uv, out vec3 pos, out vec3 normal, float radius) {
    float theta = uv.x * 2.0 * PI;  // Azimuthal [0, 2pi]
    float phi = uv.y * PI;           // Polar [0, pi]

    float sinPhi = sin(phi);
    float cosPhi = cos(phi);
    float sinTheta = sin(theta);
    float cosTheta = cos(theta);

    pos.x = radius * sinPhi * cosTheta;
    pos.y = radius * sinPhi * sinTheta;
    pos.z = radius * cosPhi;

    normal = normalize(pos);
}

// ============================================================================
// Parametric Cone
// ============================================================================

void parametricCone(vec2 uv, out vec3 pos, out vec3 normal, float radius, float height) {
    float theta = uv.x * 2.0 * PI;
    float v = uv.y;

    float r = radius * (1.0 - v);

    pos.x = r * cos(theta);
    pos.y = r * sin(theta);
    pos.z = height * v;

    vec3 dpdu = vec3(-r * sin(theta), r * cos(theta), 0.0);
    vec3 dpdv = vec3(-radius * cos(theta), -radius * sin(theta), height);
    normal = normalize(cross(dpdu, dpdv));
}

// ============================================================================
// Parametric Cylinder
// ============================================================================

void parametricCylinder(vec2 uv, out vec3 pos, out vec3 normal, float radius, float height) {
    float theta = uv.x * 2.0 * PI;
    float v = uv.y;

    pos.x = radius * cos(theta);
    pos.y = radius * sin(theta);
    pos.z = height * (v - 0.5);

    normal = normalize(vec3(cos(theta), sin(theta), 0.0));
}

// ============================================================================
// Dispatch Function
// ============================================================================

void evaluateParametricSurface(vec2 uv, out vec3 pos, out vec3 normal, uint elementType,
                                float torusMajorR, float torusMinorR, float sphereRadius) {
    switch (elementType) {
        case 0:  parametricTorus(uv, pos, normal, torusMajorR, torusMinorR); break;
        case 1:  parametricSphere(uv, pos, normal, sphereRadius); break;
        case 2:  parametricCone(uv, pos, normal, 0.5, 1.0); break;
        case 3:  parametricCylinder(uv, pos, normal, 0.5, 1.0); break;
        default: parametricSphere(uv, pos, normal, sphereRadius); break;
    }
}

#endif // PARAMETRIC_SURFACES_GLSL
