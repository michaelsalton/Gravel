#ifndef NOISE_GLSL
#define NOISE_GLSL

// ============================================================================
// 3D Value Noise
// Self-contained — no external dependencies.
// ============================================================================

// Integer hash — maps a uvec3 lattice coordinate to a pseudo-random float [0,1)
float _noiseHash(uvec3 p) {
    uint n = p.x * 1619u + p.y * 31337u + p.z * 6971u;
    n = (n << 13u) ^ n;
    n = n * (n * n * 15731u + 789221u) + 1376312589u;
    return float(n & 0x7fffffffu) / float(0x7fffffffu);
}

// Smooth Hermite interpolation (C2 continuity)
vec3 _noiseFade(vec3 t) {
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

// 3D value noise: returns a single float in roughly [0, 1].
// Trilinearly interpolates 8 lattice corners with smooth Hermite weights.
float valueNoise(vec3 p) {
    vec3  i = floor(p);
    vec3  f = fract(p);
    vec3  u = _noiseFade(f);

    uvec3 i0 = uvec3(i);
    uvec3 i1 = i0 + uvec3(1u);

    float c000 = _noiseHash(uvec3(i0.x, i0.y, i0.z));
    float c100 = _noiseHash(uvec3(i1.x, i0.y, i0.z));
    float c010 = _noiseHash(uvec3(i0.x, i1.y, i0.z));
    float c110 = _noiseHash(uvec3(i1.x, i1.y, i0.z));
    float c001 = _noiseHash(uvec3(i0.x, i0.y, i1.z));
    float c101 = _noiseHash(uvec3(i1.x, i0.y, i1.z));
    float c011 = _noiseHash(uvec3(i0.x, i1.y, i1.z));
    float c111 = _noiseHash(uvec3(i1.x, i1.y, i1.z));

    return mix(mix(mix(c000, c100, u.x), mix(c010, c110, u.x), u.y),
               mix(mix(c001, c101, u.x), mix(c011, c111, u.x), u.y),
               u.z);
}

// Fractal Brownian Motion — 4 octaves, returns value in roughly [0, 1].
float fbm(vec3 p) {
    float v   = 0.0;
    float amp = 0.5;
    float freq = 1.0;
    for (int i = 0; i < 4; i++) {
        v   += amp * valueNoise(p * freq);
        amp  *= 0.5;
        freq *= 2.0;
    }
    return v;
}

#endif // NOISE_GLSL
