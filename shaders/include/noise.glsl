// ============================================================================
// noise.glsl — PCG random, value noise, and Perlin 3D noise with gradient
// ============================================================================
//
// Randomness and noise utilities for the pebble pipeline. Provides a PCG
// pseudo-random generator (seeded per element via the global _seed) used for
// per-pebble extrusion jitter, a trilinear value-noise function, and a full 3D
// Perlin noise that also returns its analytical gradient — used by pebble.mesh
// to displace the surface and perturb normals for organic detail.
// ============================================================================

#ifndef NOISE_GLSL                               // Include guard: skip if already included this compilation unit
#define NOISE_GLSL

// *** Michael Salton ***

// ============================================================================
// PCG Random Number Generator
// ============================================================================

uint _seed = 0;                                  // Global RNG state; callers set this (e.g. to the element ID) before rand()

uint pcg() {                                     // Advance the PCG generator and return a 32-bit random uint
    uint state = _seed * 747796405U + 2891336453U; // LCG step on the current seed
    uint tmp = ((state >> ((state >> 28U) + 4U)) ^ state) * 277803737U; // PCG permutation (xorshift + multiply)
    return (_seed = (tmp >> 22U) ^ tmp);         // Final xorshift; store back into _seed and return
}

float rand() { return float(pcg()) / float(0xffffffffU); } // Random float in [0,1]
float rand(float minVal, float maxVal) { return mix(minVal, maxVal, rand()); } // Random float in [minVal, maxVal]
vec2 rand2() { return vec2(rand(), rand()); }    // Two random floats in [0,1]
vec3 rand3(float minVal, float maxVal) { return vec3(rand(minVal, maxVal), rand(minVal, maxVal), rand(minVal, maxVal)); } // Three randoms in range

// ============================================================================
// Value Noise
// ============================================================================

vec3 quintic(vec3 p) {                           // Quintic smoothstep (6t⁵-15t⁴+10t³) per component, for smooth interpolation
    return p * p * p * (10.0 + p * (-15.0 + p * 6.0));
}

float whiteNoise3x1(vec3 p) {                    // Hash a 3D point to a pseudo-random float in [0,1]
    float random = dot(p, vec3(12.9898, 78.233, 37.719)); // Project onto an arbitrary vector
    random = sin(random) * 43758.5453;           // Scramble via sin and a large multiplier
    return fract(random);                         // Keep the fractional part as the hash
}

float valueNoise(vec3 pos) {                     // Trilinearly interpolated value noise at pos
    vec3 gridPos = fract(pos);                   // Position within the current unit cell [0,1)
    vec3 gridId = floor(pos);                    // Integer cell coordinate
    gridPos = quintic(gridPos);                  // Smooth the interpolation weights

    // 8 corners of the unit cube
    float c000 = whiteNoise3x1(gridId + vec3(0, 0, 0)); // Random value at each of the 8 cell corners
    float c100 = whiteNoise3x1(gridId + vec3(1, 0, 0));
    float c010 = whiteNoise3x1(gridId + vec3(0, 1, 0));
    float c110 = whiteNoise3x1(gridId + vec3(1, 1, 0));
    float c001 = whiteNoise3x1(gridId + vec3(0, 0, 1));
    float c101 = whiteNoise3x1(gridId + vec3(1, 0, 1));
    float c011 = whiteNoise3x1(gridId + vec3(0, 1, 1));
    float c111 = whiteNoise3x1(gridId + vec3(1, 1, 1));

    // Trilinear interpolation
    float x00 = mix(c000, c100, gridPos.x);      // Interpolate the 4 cube edges along X
    float x10 = mix(c010, c110, gridPos.x);
    float x01 = mix(c001, c101, gridPos.x);
    float x11 = mix(c011, c111, gridPos.x);

    float xy0 = mix(x00, x10, gridPos.y);        // Interpolate the 2 faces along Y
    float xy1 = mix(x01, x11, gridPos.y);

    return mix(xy0, xy1, gridPos.z);             // Interpolate along Z for the final value
}

float valueNoiseOctaves(vec3 pos, uint octaves) { // Fractal (multi-octave) value noise
    float total = 0.0;                           // Accumulated noise
    float amplitude = 1.0;                       // Current octave amplitude
    float frequency = 1.0;                       // Current octave frequency
    float maxValue = 0.0;                        // Sum of amplitudes (for normalization)

    for (uint i = 0; i < octaves; i++) {         // Add successively finer, weaker octaves
        total += valueNoise(pos * frequency) * amplitude; // Sample this octave
        maxValue += amplitude;                   // Track total amplitude
        amplitude *= 0.5;                        // Halve amplitude each octave
        frequency *= 2.0;                        // Double frequency each octave
    }

    return total / maxValue;                     // Normalize back to ~[0,1]
}

// ============================================================================
// Perlin Noise 3D with Analytical Gradient
// ============================================================================

struct PerlinNoise3D {                           // Result of a Perlin noise evaluation
    float value;    // noise value in approximately [-1, 1]
    vec3 gradient;  // analytical gradient for normal perturbation
};

// Hash permutation
vec4 permute(vec4 x) {                           // Permutation polynomial (vec4) used to hash lattice indices
    return mod(((x * 34.0) + 1.0) * x, 289.0);
}

float permute(float x) {                         // Scalar overload of the permutation polynomial
    return mod(((x * 34.0) + 1.0) * x, 289.0);
}

vec4 taylorInvSqrt(vec4 r) {                      // Cheap approximate inverse sqrt (for normalizing gradients)
    return 1.79284291400159 - 0.85373472095314 * r;
}

// Quintic fade for smooth interpolation
vec3 fade(vec3 t) {                              // Perlin quintic fade curve (6t⁵-15t⁴+10t³)
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

// Analytical derivative of the fade function
vec3 dFade(vec3 t) {                             // Derivative of the fade curve (used for the analytical gradient)
    return 30.0 * t * t * (t * (t - 2.0) + 1.0);
}

PerlinNoise3D perlinNoise3D(vec3 P) {            // Classic 3D Perlin noise returning value + gradient
    vec3 Pi0 = floor(P);                         // Integer lattice corner (low)
    vec3 Pi1 = Pi0 + vec3(1.0);                  // Opposite lattice corner (high)
    Pi0 = mod(Pi0, 289.0);                       // Wrap to the permutation table period
    Pi1 = mod(Pi1, 289.0);

    vec3 Pf0 = fract(P);                         // Fractional offset from the low corner
    vec3 Pf1 = Pf0 - vec3(1.0);                  // Offset from the high corner

    vec4 ix = vec4(Pi0.x, Pi1.x, Pi0.x, Pi1.x);  // X indices of the 4 columns of the cube
    vec4 iy = vec4(Pi0.yy, Pi1.yy);              // Y indices (low,low,high,high)
    vec4 iz0 = Pi0.zzzz;                          // Z index for the near cube face
    vec4 iz1 = Pi1.zzzz;                          // Z index for the far cube face

    vec4 ixy  = permute(permute(ix) + iy);       // Hash the (x,y) lattice coordinates
    vec4 ixy0 = permute(ixy + iz0);              // Hash including near-Z → 4 corner gradient indices
    vec4 ixy1 = permute(ixy + iz1);              // Hash including far-Z → 4 corner gradient indices

    vec4 gx0 = ixy0 / 7.0;                        // Decode near-face gradient X from the hash
    vec4 gy0 = fract(floor(gx0) / 7.0) - 0.5;     // Decode gradient Y
    gx0 = fract(gx0);                             // Keep gradient X fraction
    vec4 gz0 = vec4(0.5) - abs(gx0) - abs(gy0);   // Gradient Z fills the remaining length budget
    vec4 sz0 = step(gz0, vec4(0.0));              // Sign-correction mask where gz0 < 0
    gx0 -= sz0 * (step(0.0, gx0) - 0.5);          // Flip X sign where needed (keeps gradients on the octahedron)
    gy0 -= sz0 * (step(0.0, gy0) - 0.5);          // Flip Y sign where needed

    vec4 gx1 = ixy1 / 7.0;                        // Same gradient decode for the far cube face
    vec4 gy1 = fract(floor(gx1) / 7.0) - 0.5;
    gx1 = fract(gx1);
    vec4 gz1 = vec4(0.5) - abs(gx1) - abs(gy1);
    vec4 sz1 = step(gz1, vec4(0.0));
    gx1 -= sz1 * (step(0.0, gx1) - 0.5);
    gy1 -= sz1 * (step(0.0, gy1) - 0.5);

    vec3 g000 = vec3(gx0.x, gy0.x, gz0.x);        // Assemble the 8 corner gradient vectors:
    vec3 g100 = vec3(gx0.y, gy0.y, gz0.y);        //   near face corners (z = low)
    vec3 g010 = vec3(gx0.z, gy0.z, gz0.z);
    vec3 g110 = vec3(gx0.w, gy0.w, gz0.w);
    vec3 g001 = vec3(gx1.x, gy1.x, gz1.x);        //   far face corners (z = high)
    vec3 g101 = vec3(gx1.y, gy1.y, gz1.y);
    vec3 g011 = vec3(gx1.z, gy1.z, gz1.z);
    vec3 g111 = vec3(gx1.w, gy1.w, gz1.w);

    // Normalize gradients
    vec4 norm0 = taylorInvSqrt(vec4(dot(g000, g000), dot(g010, g010), dot(g100, g100), dot(g110, g110))); // Near-face inv lengths
    g000 *= norm0.x; g010 *= norm0.y; g100 *= norm0.z; g110 *= norm0.w; // Normalize near-face gradients
    vec4 norm1 = taylorInvSqrt(vec4(dot(g001, g001), dot(g011, g011), dot(g101, g101), dot(g111, g111))); // Far-face inv lengths
    g001 *= norm1.x; g011 *= norm1.y; g101 *= norm1.z; g111 *= norm1.w; // Normalize far-face gradients

    // Dot products
    float n000 = dot(g000, Pf0);                  // Influence of each corner gradient on the sample point:
    float n100 = dot(g100, vec3(Pf1.x, Pf0.yz));  //   dot(corner gradient, offset from that corner)
    float n010 = dot(g010, vec3(Pf0.x, Pf1.y, Pf0.z));
    float n110 = dot(g110, vec3(Pf1.xy, Pf0.z));
    float n001 = dot(g001, vec3(Pf0.xy, Pf1.z));
    float n101 = dot(g101, vec3(Pf1.x, Pf0.y, Pf1.z));
    float n011 = dot(g011, vec3(Pf0.x, Pf1.yz));
    float n111 = dot(g111, Pf1);

    // Interpolation
    vec3 f = fade(Pf0);                           // Smoothed interpolation weights
    vec3 df = dFade(Pf0);                         // Their derivatives (for the gradient)

    // Trilinear interpolation for value
    float x00 = mix(n000, n100, f.x);             // Interpolate corner influences along X
    float x10 = mix(n010, n110, f.x);
    float x01 = mix(n001, n101, f.x);
    float x11 = mix(n011, n111, f.x);

    float xy0 = mix(x00, x10, f.y);               // Interpolate along Y
    float xy1 = mix(x01, x11, f.y);

    float value = mix(xy0, xy1, f.z);             // Interpolate along Z → final noise value

    // Analytical gradient via chain rule
    // d/dx
    float dx00 = n100 - n000;                     // Partial differences of corner influence along X
    float dx10 = n110 - n010;
    float dx01 = n101 - n001;
    float dx11 = n111 - n011;
    float dxy0 = mix(dx00, dx10, f.y);            // Interpolate the X-differences along Y
    float dxy1 = mix(dx01, dx11, f.y);
    float dxyz = mix(dxy0, dxy1, f.z);            // ...then along Z → core dV/dx term

    // d/dy
    float dy0 = x10 - x00;                        // Differences along Y at the two Z faces
    float dy1 = x11 - x01;
    float dyz = mix(dy0, dy1, f.z);               // Interpolate along Z → core dV/dy term

    // d/dz
    float dz = xy1 - xy0;                         // Difference along Z → core dV/dz term

    vec3 grad = vec3(dxyz * df.x, dyz * df.y, dz * df.z); // Scale each term by the fade derivative (chain rule)

    return PerlinNoise3D(value, grad);            // Return noise value and its analytical gradient
}

#endif // NOISE_GLSL

// *** ************ ***
