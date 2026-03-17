#ifndef PARAMETRIC_SURFACES_GLSL
#define PARAMETRIC_SURFACES_GLSL

#include "common.glsl"
#include "bspline.glsl"

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
// Parametric Hemisphere
// ============================================================================

void parametricHemisphere(vec2 uv, out vec3 pos, out vec3 normal, float radius) {
    float theta = uv.x * 2.0 * PI;   // Azimuthal [0, 2pi]
    float phi   = uv.y * 0.5 * PI;   // Polar [0, pi/2] — top half only

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
// Parametric Dragon Scale (B-spline LUT)
// ============================================================================

void parametricDragonScale(vec2 uv, out vec3 pos, out vec3 normal) {
    uvec2 gridSize   = uvec2(resurfacingUBO.Nx, resurfacingUBO.Ny);
    // B-spline degree 3, stride 1: adjacent patches share control points.
    // For a non-cyclic grid of Nx points there are (Nx - 3) patches.
    uint numPatchesU = gridSize.x - 3u;
    uint numPatchesV = gridSize.y - 3u;

    // Map uv into patch index + local parameter
    float pU = uv.x * float(numPatchesU);
    float pV = uv.y * float(numPatchesV);
    uint  pu = min(uint(pU), numPatchesU - 1u);
    uint  pv = min(uint(pV), numPatchesV - 1u);
    vec2  localUV = vec2(pU - float(pu), pV - float(pv));

    // Fetch 4x4 control points: stride=1, so patch (pu,pv) starts at LUT index (pu, pv)
    vec3 P[4][4];
    for (int j = 0; j < 4; j++)
        for (int i = 0; i < 4; i++)
            P[i][j] = getScaleLutPoint(uvec2(pu + uint(i), pv + uint(j)), gridSize);

    pos = evaluateBSplinePatch(localUV, P);

    // Normalise into unit space using precomputed LUT bounding box
    vec3 extentMin = resurfacingUBO.minLutExtent.xyz;
    vec3 extentMax = resurfacingUBO.maxLutExtent.xyz;
    vec3 center    = (extentMin + extentMax) * 0.5;
    float scale    = max(max(extentMax.x - extentMin.x,
                             extentMax.y - extentMin.y),
                             extentMax.z - extentMin.z) * 0.5;
    pos = (pos - center) / max(scale, 0.0001);

    // The LUT geometry has its flat spread in XZ and curvature height in Y.
    // offsetVertex() maps local Z → face normal (outward), so remap Y↔Z so
    // the scale lies flat on the mesh surface with curvature pointing outward.
    // Shift Z so the scale base (LUT minY) sits at Z=0 (on the mesh surface).
    float zOffset = (center.y - extentMin.y) / max(scale, 0.0001);
    pos = vec3(pos.x, pos.z, pos.y + zOffset);

    normal = computeFiniteDifferenceBspline(localUV, P);
    normal = vec3(normal.x, normal.z, normal.y);
}

// ============================================================================
// Parametric Straw (curved tapered cone)
// ============================================================================

void parametricStraw(vec2 uv, out vec3 pos, out vec3 normal, uint elementId) {
    float taperPower    = resurfacingUBO.strawTaperPower;
    float bendAmount    = resurfacingUBO.strawBendAmount;
    float baseRadius    = resurfacingUBO.strawBaseRadius;
    float bendDirection = resurfacingUBO.strawBendDirection;
    float bendRandomness = resurfacingUBO.strawBendRandomness;

    float u = uv.x * 2.0 * PI;   // Azimuthal [0, 2pi]
    float v = uv.y;               // Height [0, 1] base to tip

    float cosU = cos(u);
    float sinU = sin(u);

    // Thin constant-radius tube with subtle taper only at the very tip.
    // smoothstep kicks in only in the last ~10% of the length, controlled
    // by taperPower (higher = taper starts later / is more abrupt).
    float taperStart = clamp(1.0 - 1.0 / taperPower, 0.5, 0.98);
    float taper = 1.0 - smoothstep(taperStart, 1.0, v);
    float r = baseRadius * taper;

    // Height: tall straw (aspect ratio ~20:1)
    float height = 2.0;

    // Per-element random offset for bend direction (golden ratio hash)
    float randomAngle = fract(float(elementId) * 0.618033988749895) * 2.0 * PI;
    float bendAngle = bendDirection + bendRandomness * randomAngle;
    float bendCos = cos(bendAngle);
    float bendSin = sin(bendAngle);

    // Quadratic bend along length, rotated by bendAngle in XY plane
    float bend = bendAmount * v * v;
    float bendX = bend * bendCos;
    float bendY = bend * bendSin;

    pos.x = r * cosU + bendX;
    pos.y = r * sinU + bendY;
    pos.z = height * v;

    // Analytic partial derivatives for normal
    float taperRange = 1.0 - taperStart;
    float t = clamp((v - taperStart) / taperRange, 0.0, 1.0);
    float dtaperdv = -6.0 * t * (1.0 - t) / taperRange;
    float drdv = baseRadius * dtaperdv;
    float dbdv = 2.0 * bendAmount * v;
    float dbxdv = dbdv * bendCos;
    float dbydv = dbdv * bendSin;

    vec3 dpdu = vec3(-r * sinU, r * cosU, 0.0);
    vec3 dpdv = vec3(drdv * cosU + dbxdv, drdv * sinU + dbydv, height);
    normal = normalize(cross(dpdu, dpdv));
}

// ============================================================================
// Dispatch Function
// ============================================================================

void evaluateParametricSurface(vec2 uv, out vec3 pos, out vec3 normal, uint elementType,
                                float torusMajorR, float torusMinorR, float sphereRadius,
                                uint elementId) {
    switch (elementType) {
        case 0:  parametricTorus(uv, pos, normal, torusMajorR, torusMinorR); break;
        case 1:  parametricSphere(uv, pos, normal, sphereRadius); break;
        case 2:  parametricCone(uv, pos, normal, 0.5, 1.0); break;
        case 3:  parametricCylinder(uv, pos, normal, 0.5, 1.0); break;
        case 4:  parametricHemisphere(uv, pos, normal, sphereRadius); break;
        case 5:  parametricDragonScale(uv, pos, normal); break;
        case 6:  parametricStraw(uv, pos, normal, elementId); break;
        default: parametricSphere(uv, pos, normal, sphereRadius); break;
    }
}

#endif // PARAMETRIC_SURFACES_GLSL
