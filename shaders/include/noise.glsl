// ============================================================================
// noise.glsl — PCG random, value noise, and Perlin 3D noise with gradient
// ============================================================================

#ifndef NOISE_GLSL
#define NOISE_GLSL

// ============================================================================
// PCG Random Number Generator
// ============================================================================

uint _seed = 0;

uint pcg() {
    uint state = _seed * 747796405U + 2891336453U;
    uint tmp = ((state >> ((state >> 28U) + 4U)) ^ state) * 277803737U;
    return (_seed = (tmp >> 22U) ^ tmp);
}

float rand() { return float(pcg()) / float(0xffffffffU); }
float rand(float minVal, float maxVal) { return mix(minVal, maxVal, rand()); }
vec2 rand2() { return vec2(rand(), rand()); }
vec3 rand3(float minVal, float maxVal) { return vec3(rand(minVal, maxVal), rand(minVal, maxVal), rand(minVal, maxVal)); }

// ============================================================================
// Value Noise
// ============================================================================

vec3 quintic(vec3 p) {
    return p * p * p * (10.0 + p * (-15.0 + p * 6.0));
}

float whiteNoise3x1(vec3 p) {
    float random = dot(p, vec3(12.9898, 78.233, 37.719));
    random = sin(random) * 43758.5453;
    return fract(random);
}

float valueNoise(vec3 pos) {
    vec3 gridPos = fract(pos);
    vec3 gridId = floor(pos);
    gridPos = quintic(gridPos);

    // 8 corners of the unit cube
    float c000 = whiteNoise3x1(gridId + vec3(0, 0, 0));
    float c100 = whiteNoise3x1(gridId + vec3(1, 0, 0));
    float c010 = whiteNoise3x1(gridId + vec3(0, 1, 0));
    float c110 = whiteNoise3x1(gridId + vec3(1, 1, 0));
    float c001 = whiteNoise3x1(gridId + vec3(0, 0, 1));
    float c101 = whiteNoise3x1(gridId + vec3(1, 0, 1));
    float c011 = whiteNoise3x1(gridId + vec3(0, 1, 1));
    float c111 = whiteNoise3x1(gridId + vec3(1, 1, 1));

    // Trilinear interpolation
    float x00 = mix(c000, c100, gridPos.x);
    float x10 = mix(c010, c110, gridPos.x);
    float x01 = mix(c001, c101, gridPos.x);
    float x11 = mix(c011, c111, gridPos.x);

    float xy0 = mix(x00, x10, gridPos.y);
    float xy1 = mix(x01, x11, gridPos.y);

    return mix(xy0, xy1, gridPos.z);
}

float valueNoiseOctaves(vec3 pos, uint octaves) {
    float total = 0.0;
    float amplitude = 1.0;
    float frequency = 1.0;
    float maxValue = 0.0;

    for (uint i = 0; i < octaves; i++) {
        total += valueNoise(pos * frequency) * amplitude;
        maxValue += amplitude;
        amplitude *= 0.5;
        frequency *= 2.0;
    }

    return total / maxValue;
}

// ============================================================================
// Perlin Noise 3D with Analytical Gradient
// ============================================================================

struct PerlinNoise3D {
    float value;    // noise value in approximately [-1, 1]
    vec3 gradient;  // analytical gradient for normal perturbation
};

// Hash permutation
vec4 permute(vec4 x) {
    return mod(((x * 34.0) + 1.0) * x, 289.0);
}

float permute(float x) {
    return mod(((x * 34.0) + 1.0) * x, 289.0);
}

vec4 taylorInvSqrt(vec4 r) {
    return 1.79284291400159 - 0.85373472095314 * r;
}

// Quintic fade for smooth interpolation
vec3 fade(vec3 t) {
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

// Analytical derivative of the fade function
vec3 dFade(vec3 t) {
    return 30.0 * t * t * (t * (t - 2.0) + 1.0);
}

PerlinNoise3D perlinNoise3D(vec3 P) {
    vec3 Pi0 = floor(P);
    vec3 Pi1 = Pi0 + vec3(1.0);
    Pi0 = mod(Pi0, 289.0);
    Pi1 = mod(Pi1, 289.0);

    vec3 Pf0 = fract(P);
    vec3 Pf1 = Pf0 - vec3(1.0);

    vec4 ix = vec4(Pi0.x, Pi1.x, Pi0.x, Pi1.x);
    vec4 iy = vec4(Pi0.yy, Pi1.yy);
    vec4 iz0 = Pi0.zzzz;
    vec4 iz1 = Pi1.zzzz;

    vec4 ixy  = permute(permute(ix) + iy);
    vec4 ixy0 = permute(ixy + iz0);
    vec4 ixy1 = permute(ixy + iz1);

    vec4 gx0 = ixy0 / 7.0;
    vec4 gy0 = fract(floor(gx0) / 7.0) - 0.5;
    gx0 = fract(gx0);
    vec4 gz0 = vec4(0.5) - abs(gx0) - abs(gy0);
    vec4 sz0 = step(gz0, vec4(0.0));
    gx0 -= sz0 * (step(0.0, gx0) - 0.5);
    gy0 -= sz0 * (step(0.0, gy0) - 0.5);

    vec4 gx1 = ixy1 / 7.0;
    vec4 gy1 = fract(floor(gx1) / 7.0) - 0.5;
    gx1 = fract(gx1);
    vec4 gz1 = vec4(0.5) - abs(gx1) - abs(gy1);
    vec4 sz1 = step(gz1, vec4(0.0));
    gx1 -= sz1 * (step(0.0, gx1) - 0.5);
    gy1 -= sz1 * (step(0.0, gy1) - 0.5);

    vec3 g000 = vec3(gx0.x, gy0.x, gz0.x);
    vec3 g100 = vec3(gx0.y, gy0.y, gz0.y);
    vec3 g010 = vec3(gx0.z, gy0.z, gz0.z);
    vec3 g110 = vec3(gx0.w, gy0.w, gz0.w);
    vec3 g001 = vec3(gx1.x, gy1.x, gz1.x);
    vec3 g101 = vec3(gx1.y, gy1.y, gz1.y);
    vec3 g011 = vec3(gx1.z, gy1.z, gz1.z);
    vec3 g111 = vec3(gx1.w, gy1.w, gz1.w);

    // Normalize gradients
    vec4 norm0 = taylorInvSqrt(vec4(dot(g000, g000), dot(g010, g010), dot(g100, g100), dot(g110, g110)));
    g000 *= norm0.x; g010 *= norm0.y; g100 *= norm0.z; g110 *= norm0.w;
    vec4 norm1 = taylorInvSqrt(vec4(dot(g001, g001), dot(g011, g011), dot(g101, g101), dot(g111, g111)));
    g001 *= norm1.x; g011 *= norm1.y; g101 *= norm1.z; g111 *= norm1.w;

    // Dot products
    float n000 = dot(g000, Pf0);
    float n100 = dot(g100, vec3(Pf1.x, Pf0.yz));
    float n010 = dot(g010, vec3(Pf0.x, Pf1.y, Pf0.z));
    float n110 = dot(g110, vec3(Pf1.xy, Pf0.z));
    float n001 = dot(g001, vec3(Pf0.xy, Pf1.z));
    float n101 = dot(g101, vec3(Pf1.x, Pf0.y, Pf1.z));
    float n011 = dot(g011, vec3(Pf0.x, Pf1.yz));
    float n111 = dot(g111, Pf1);

    // Interpolation
    vec3 f = fade(Pf0);
    vec3 df = dFade(Pf0);

    // Trilinear interpolation for value
    float x00 = mix(n000, n100, f.x);
    float x10 = mix(n010, n110, f.x);
    float x01 = mix(n001, n101, f.x);
    float x11 = mix(n011, n111, f.x);

    float xy0 = mix(x00, x10, f.y);
    float xy1 = mix(x01, x11, f.y);

    float value = mix(xy0, xy1, f.z);

    // Analytical gradient via chain rule
    // d/dx
    float dx00 = n100 - n000;
    float dx10 = n110 - n010;
    float dx01 = n101 - n001;
    float dx11 = n111 - n011;
    float dxy0 = mix(dx00, dx10, f.y);
    float dxy1 = mix(dx01, dx11, f.y);
    float dxyz = mix(dxy0, dxy1, f.z);

    // d/dy
    float dy0 = x10 - x00;
    float dy1 = x11 - x01;
    float dyz = mix(dy0, dy1, f.z);

    // d/dz
    float dz = xy1 - xy0;

    vec3 grad = vec3(dxyz * df.x, dyz * df.y, dz * df.z);

    return PerlinNoise3D(value, grad);
}

#endif // NOISE_GLSL
