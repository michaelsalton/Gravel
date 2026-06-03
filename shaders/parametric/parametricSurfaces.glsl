#ifndef PARAMETRIC_SURFACES_GLSL
#define PARAMETRIC_SURFACES_GLSL

#include "common.glsl"                          // PI constant, scale-LUT accessor (getScaleLutPoint), and shared helpers
#include "bspline.glsl"                          // evaluateBSplinePatch and computeFiniteDifferenceBspline for the dragon-scale surface

// =============================================================================
// parametricSurfaces.glsl — Procedural surface generators
//
// Library of analytic parametric surfaces used by the resurfacing mesh shader.
// Each generator maps a UV in [0,1]x[0,1] to a local-space position and normal
// for one element type (torus, sphere, cone, cylinder, hemisphere, dragon
// scale, straw, stud). The mesh shader calls evaluateParametricSurface() to
// dispatch on the element type, then offsets/orients the result onto the base
// mesh. Most surfaces compute their normal analytically from the parametric
// partial derivatives (cross of dP/du and dP/dv).
// =============================================================================

// *** Michael Salton ***

// ============================================================================
// Parametric Torus
// ============================================================================

void parametricTorus(vec2 uv, out vec3 pos, out vec3 normal, float majorR, float minorR) { // Donut: majorR = ring radius, minorR = tube radius
    float u = uv.x * 2.0 * PI;                  // U → angle around the main ring [0, 2π]
    float v = uv.y * 2.0 * PI;                  // V → angle around the tube cross-section [0, 2π]

    float cosU = cos(u);                        // Precompute trig for the ring angle
    float sinU = sin(u);
    float cosV = cos(v);                        // Precompute trig for the tube angle
    float sinV = sin(v);

    float tubeRadius = majorR + minorR * cosV;  // Distance from torus center to this point (ring + tube offset)
    pos.x = tubeRadius * cosU;                  // Position swept around the ring in X
    pos.y = tubeRadius * sinU;                  // ...and Y
    pos.z = minorR * sinV;                      // Height above/below the ring plane from the tube angle

    vec3 dpdu = vec3(-tubeRadius * sinU, tubeRadius * cosU, 0.0); // Tangent along the ring direction (∂P/∂u)
    vec3 dpdv = vec3(-minorR * sinV * cosU, -minorR * sinV * sinU, minorR * cosV); // Tangent along the tube direction (∂P/∂v)
    normal = normalize(cross(dpdu, dpdv));      // Surface normal = normalized cross of the two tangents
}

// ============================================================================
// Parametric Sphere
// ============================================================================

void parametricSphere(vec2 uv, out vec3 pos, out vec3 normal, float radius) { // Full sphere of the given radius
    float theta = uv.x * 2.0 * PI;  // Azimuthal [0, 2pi] // Longitude
    float phi = uv.y * PI;           // Polar [0, pi]      // Latitude (pole to pole)

    float sinPhi = sin(phi);                    // Precompute trig for the polar angle
    float cosPhi = cos(phi);
    float sinTheta = sin(theta);                // Precompute trig for the azimuthal angle
    float cosTheta = cos(theta);

    pos.x = radius * sinPhi * cosTheta;         // Spherical → Cartesian X
    pos.y = radius * sinPhi * sinTheta;         // ...Y
    pos.z = radius * cosPhi;                    // ...Z

    normal = normalize(pos);                    // For a sphere centered at origin, the normal is just the position direction
}

// ============================================================================
// Parametric Cone
// ============================================================================

void parametricCone(vec2 uv, out vec3 pos, out vec3 normal, float radius, float height) { // Cone: base radius → apex
    float theta = uv.x * 2.0 * PI;              // U → angle around the cone [0, 2π]
    float v = uv.y;                             // V → height along the axis [0,1], base to tip

    float r = radius * (1.0 - v);               // Radius shrinks linearly to 0 at the tip

    pos.x = r * cos(theta);                     // Point on the circular cross-section X
    pos.y = r * sin(theta);                     // ...Y
    pos.z = height * v;                         // Height up the axis

    vec3 dpdu = vec3(-r * sin(theta), r * cos(theta), 0.0); // Tangent around the cross-section (∂P/∂u)
    vec3 dpdv = vec3(-radius * cos(theta), -radius * sin(theta), height); // Tangent up the slope (∂P/∂v)
    normal = normalize(cross(dpdu, dpdv));      // Normal from the cross product of tangents
}

// ============================================================================
// Parametric Cylinder
// ============================================================================

void parametricCylinder(vec2 uv, out vec3 pos, out vec3 normal, float radius, float height) { // Side wall of a cylinder
    float theta = uv.x * 2.0 * PI;              // U → angle around the cylinder [0, 2π]
    float v = uv.y;                             // V → height [0,1]

    pos.x = radius * cos(theta);                // Point on the circular wall X
    pos.y = radius * sin(theta);                // ...Y
    pos.z = height * (v - 0.5);                 // Height, centered so the cylinder spans [-h/2, +h/2]

    normal = normalize(vec3(cos(theta), sin(theta), 0.0)); // Wall normal points radially outward (no Z component)
}

// ============================================================================
// Parametric Hemisphere
// ============================================================================

void parametricHemisphere(vec2 uv, out vec3 pos, out vec3 normal, float radius) { // Top half of a sphere
    float theta = uv.x * 2.0 * PI;   // Azimuthal [0, 2pi] // Longitude
    float phi   = uv.y * 0.5 * PI;   // Polar [0, pi/2] — top half only // Latitude limited to the upper hemisphere

    float sinPhi = sin(phi);                    // Precompute trig for the polar angle
    float cosPhi = cos(phi);
    float sinTheta = sin(theta);                // Precompute trig for the azimuthal angle
    float cosTheta = cos(theta);

    pos.x = radius * sinPhi * cosTheta;         // Spherical → Cartesian X
    pos.y = radius * sinPhi * sinTheta;         // ...Y
    pos.z = radius * cosPhi;                    // ...Z (always ≥ 0 since phi ≤ π/2)

    normal = normalize(pos);                    // Normal is the position direction (sphere centered at origin)
}

// ============================================================================
// Parametric Dragon Scale (B-spline LUT)
// ============================================================================

void parametricDragonScale(vec2 uv, out vec3 pos, out vec3 normal) { // Curved scale evaluated from a cubic B-spline control grid (LUT)
    uvec2 gridSize   = uvec2(resurfacingUBO.Nx, resurfacingUBO.Ny); // Control-point grid dimensions from the UBO
    // B-spline degree 3, stride 1: adjacent patches share control points.
    // For a non-cyclic grid of Nx points there are (Nx - 3) patches.
    uint numPatchesU = gridSize.x - 3u;         // Number of cubic patches in U
    uint numPatchesV = gridSize.y - 3u;         // Number of cubic patches in V

    // Map uv into patch index + local parameter
    float pU = uv.x * float(numPatchesU);       // Scale U into patch space
    float pV = uv.y * float(numPatchesV);       // Scale V into patch space
    uint  pu = min(uint(pU), numPatchesU - 1u); // Integer patch column (clamped to the last patch)
    uint  pv = min(uint(pV), numPatchesV - 1u); // Integer patch row
    vec2  localUV = vec2(pU - float(pu), pV - float(pv)); // Fractional position within the selected patch [0,1]

    // Fetch 4x4 control points: stride=1, so patch (pu,pv) starts at LUT index (pu, pv)
    vec3 P[4][4];                               // 4×4 control-point neighborhood for this cubic patch
    for (int j = 0; j < 4; j++)                 // Loop over the 4 rows...
        for (int i = 0; i < 4; i++)             // ...and 4 columns
            P[i][j] = getScaleLutPoint(uvec2(pu + uint(i), pv + uint(j)), gridSize); // Read each control point from the LUT

    pos = evaluateBSplinePatch(localUV, P);     // Evaluate the cubic B-spline surface point

    // Normalise into unit space using precomputed LUT bounding box
    vec3 extentMin = resurfacingUBO.minLutExtent.xyz; // LUT AABB minimum corner
    vec3 extentMax = resurfacingUBO.maxLutExtent.xyz; // LUT AABB maximum corner
    vec3 center    = (extentMin + extentMax) * 0.5;   // AABB center
    float scale    = max(max(extentMax.x - extentMin.x,  // Half of the largest AABB dimension (uniform scale factor)
                             extentMax.y - extentMin.y),
                             extentMax.z - extentMin.z) * 0.5;
    pos = (pos - center) / max(scale, 0.0001);  // Recenter and scale to roughly unit size

    // The LUT geometry has its flat spread in XZ and curvature height in Y.
    // offsetVertex() maps local Z → face normal (outward), so remap Y↔Z so
    // the scale lies flat on the mesh surface with curvature pointing outward.
    // Shift Z so the scale base (LUT minY) sits at Z=0 (on the mesh surface).
    float zOffset = (center.y - extentMin.y) / max(scale, 0.0001); // Offset so the scale's base rests on the surface
    pos = vec3(pos.x, pos.z, pos.y + zOffset);  // Swap Y↔Z so curvature height becomes the outward (local Z) direction

    normal = computeFiniteDifferenceBspline(localUV, P); // Approximate the normal via finite differences of the patch
    normal = vec3(normal.x, normal.z, normal.y); // Apply the same Y↔Z remap to the normal
}

// *** ************ ***

// *** AI Generated ***

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
// Parametric Stud (elongated elliptical dome for diamond plate)
// ============================================================================

void parametricStud(vec2 uv, out vec3 pos, out vec3 normal, uint elementId, float faceColor) {
    float elongation = resurfacingUBO.studElongation;
    float height     = resurfacingUBO.studHeight;
    float power      = resurfacingUBO.studPower;
    float rotation   = resurfacingUBO.studRotation;
    float rotRandom  = resurfacingUBO.studRotationRandomness;

    float u = uv.x * 2.0 * PI;  // Azimuthal [0, 2pi]
    float v = uv.y;              // Radial [0, 1] center to edge

    float cosU = cos(u);
    float sinU = sin(u);

    // Elliptical footprint
    float x = v * cosU * elongation;
    float y = v * sinU;

    // Smooth dome height: (1 - v^2)^power
    float d = v * v;
    float h = height * pow(max(1.0 - d, 0.0), power);

    // Per-element rotation
    float angle;
    if (resurfacingUBO.studTreadPlate != 0u) {
        // Tread plate: alternate by 90° using face 2-coloring
        angle = rotation + faceColor * PI * 0.5;
    } else {
        // Random rotation (golden ratio hash)
        float randomAngle = fract(float(elementId) * 0.618033988749895) * 2.0 * PI;
        angle = rotation + rotRandom * randomAngle;
    }
    float cosA = cos(angle);
    float sinA = sin(angle);
    float rx = x * cosA - y * sinA;
    float ry = x * sinA + y * cosA;

    pos = vec3(rx, ry, h);

    // Analytic partial derivatives (pre-rotation)
    vec3 dpdu = vec3(-v * sinU * elongation, v * cosU, 0.0);
    float dhdv = -2.0 * v * height * power * pow(max(1.0 - d, 0.0001), power - 1.0);
    vec3 dpdv = vec3(cosU * elongation, sinU, dhdv);

    // Apply rotation to tangent vectors
    dpdu = vec3(dpdu.x * cosA - dpdu.y * sinA, dpdu.x * sinA + dpdu.y * cosA, dpdu.z);
    dpdv = vec3(dpdv.x * cosA - dpdv.y * sinA, dpdv.x * sinA + dpdv.y * cosA, dpdv.z);

    normal = normalize(cross(dpdu, dpdv));
}

// *** ************ ***

// *** Michael Salton ***

// ============================================================================
// Dispatch Function
// ============================================================================

void evaluateParametricSurface(vec2 uv, out vec3 pos, out vec3 normal, uint elementType, // Dispatch to the right generator by type
                                float torusMajorR, float torusMinorR, float sphereRadius, // Shape params (only the relevant ones used)
                                uint elementId, float faceColor) {                        // Per-element seed + checkerboard color
    switch (elementType) {                      // Pick the surface generator for this element's type
        case 0:  parametricTorus(uv, pos, normal, torusMajorR, torusMinorR); break; // 0 = torus
        case 1:  parametricSphere(uv, pos, normal, sphereRadius); break;            // 1 = sphere
        case 2:  parametricCone(uv, pos, normal, 0.5, 1.0); break;                  // 2 = cone (fixed radius/height)
        case 3:  parametricCylinder(uv, pos, normal, 0.5, 1.0); break;              // 3 = cylinder (fixed radius/height)
        case 4:  parametricHemisphere(uv, pos, normal, sphereRadius); break;        // 4 = hemisphere
        case 5:  parametricDragonScale(uv, pos, normal); break;                     // 5 = dragon scale (B-spline LUT)
        case 6:  parametricStraw(uv, pos, normal, elementId); break;                // 6 = straw (AI-generated)
        case 7:  parametricStud(uv, pos, normal, elementId, faceColor); break;      // 7 = stud (AI-generated)
        default: parametricSphere(uv, pos, normal, sphereRadius); break;            // Unknown type → fall back to a sphere
    }
}

#endif // PARAMETRIC_SURFACES_GLSL

// *** ************ ***