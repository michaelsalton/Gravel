#ifndef SHADING_GLSL
#define SHADING_GLSL

// ============================================================================
// Color Utilities
// ============================================================================

// Convert HSV to RGB (golden ratio hashing for per-element debug colors)
vec3 hsv2rgb(vec3 hsv) {
    vec3 rgb = clamp(abs(mod(hsv.x * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    return hsv.z * mix(vec3(1.0), rgb, hsv.y);
}

// Generate a unique color for a given ID using golden ratio distribution
vec3 getDebugColor(uint id) {
    float hue = fract(float(id) * 0.618033988749895);
    return hsv2rgb(vec3(hue, 0.7, 0.9));
}

// ============================================================================
// Geometric Specular Antialiasing (Tokuyoshi & Kaplanyan, JCGT 2021)
// Projected-space NDF filtering — widens roughness based on screen-space
// normal derivatives to suppress specular fireflies from sub-pixel geometry.
// ============================================================================

// Per-triangle NDF filtering (Tokuyoshi & Kaplanyan 2021, Listing 5 / Eq. 13)
// Widens roughness based on screen-space normal derivatives within a triangle.
float filterRoughnessSpecularAA(vec3 worldNormal, float roughness) {
    const float SIGMA2 = 0.15915494;  // 1/(2*pi)
    const float KAPPA = 0.18;         // clamping threshold to prevent overfiltering
    vec3 dndu = dFdx(worldNormal);
    vec3 dndv = dFdy(worldNormal);
    float kernelRoughness2 = 2.0 * SIGMA2 * (dot(dndu, dndu) + dot(dndv, dndv));
    float clampedKernel = min(kernelRoughness2, KAPPA);
    float roughness2 = roughness * roughness;
    return sqrt(min(roughness2 + clampedKernel, 1.0));
}

// Extended specular AA for procedural mesh shader geometry.
// The standard Tokuyoshi filter captures normal variation within a triangle,
// but for procedural elements the dominant aliasing is cross-element: adjacent
// elements have wildly different normals, but dFdx/dFdy can't see across
// element boundaries. We solve this by measuring the angular divergence between
// the procedural element's surface normal and the underlying base mesh face
// normal. At distance, a pixel covers many elements whose normals span this
// angular range, so we widen roughness proportionally.
float filterRoughnessProceduralAA(vec3 worldNormal, vec3 faceNormal, float roughness, float strength) {
    const float SIGMA2 = 0.15915494;
    const float KAPPA = 0.18;

    // Standard Tokuyoshi per-triangle kernel (intra-element normal variation)
    vec3 dndu = dFdx(worldNormal);
    vec3 dndv = dFdy(worldNormal);
    float tokuyoshiKernel = 2.0 * SIGMA2 * (dot(dndu, dndu) + dot(dndv, dndv));

    // Cross-element kernel: angular divergence between procedural normal and face normal.
    // 1 - dot(N, faceN) is 0 when aligned, ~1 when perpendicular, 2 when opposite.
    float divergence = 1.0 - max(dot(worldNormal, faceNormal), 0.0);

    // The divergence directly represents the angular spread of normals within
    // this element. Multiply by strength to control the effect.
    float crossElementKernel = divergence * divergence * strength;

    float kernelRoughness2 = tokuyoshiKernel + crossElementKernel;
    float clampedKernel = min(kernelRoughness2, KAPPA);
    float roughness2 = roughness * roughness;
    return sqrt(min(roughness2 + clampedKernel, 1.0));
}

// ============================================================================
// Cook-Torrance PBR
// ============================================================================

// Trowbridge-Reitz GGX normal distribution function
float distributionGGX(float NdotH, float roughness) {
    float a  = roughness * roughness;
    float a2 = a * a;
    float denom = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

// Schlick-GGX geometry term (single direction)
float geometrySchlickGGX(float NdotX, float roughness) {
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    return NdotX / (NdotX * (1.0 - k) + k);
}

// Smith geometry term (both view and light directions)
float geometrySmith(float NdotV, float NdotL, float roughness) {
    return geometrySchlickGGX(NdotV, roughness)
         * geometrySchlickGGX(NdotL, roughness);
}

// Schlick Fresnel approximation
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Fake sky/ground environment reflection — roughness widens the blend band
vec3 fakeEnvReflection(vec3 R, float roughness) {
    vec3 skyColor    = vec3(0.4, 0.45, 0.55);
    vec3 groundColor = vec3(0.15, 0.12, 0.1);
    float blend = smoothstep(-0.5 * (1.0 - roughness), 0.5 * (1.0 - roughness), R.y);
    return mix(groundColor, skyColor, blend);
}

// Cook-Torrance microfacet BRDF — drop-in replacement for blinnPhong()
vec3 cookTorrancePBR(vec3 worldPos, vec3 N, vec3 lightPos, vec3 cameraPos,
                     vec3 albedo, float roughness, float metallic,
                     float dielectricF0, vec4 ambient,
                     float envReflection, float lightIntensity) {
    vec3 V = normalize(cameraPos - worldPos);
    vec3 L = normalize(lightPos - worldPos);
    vec3 H = normalize(V + L);
    vec3 R = reflect(-V, N);

    float NdotV = max(dot(N, V), 0.001);
    float NdotL = max(dot(N, L), 0.0);
    float NdotH = max(dot(N, H), 0.0);

    // F0: dielectric base reflectance, lerp to albedo for metals
    vec3 F0 = mix(vec3(dielectricF0), albedo, metallic);

    // Cook-Torrance specular BRDF
    float D = distributionGGX(NdotH, roughness);
    float G = geometrySmith(NdotV, NdotL, roughness);
    vec3  F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator    = D * G * F;
    float denominator = 4.0 * NdotV * NdotL + 0.0001;
    vec3 specular     = numerator / denominator;

    // Energy conservation: diffuse only for non-reflected, non-metallic light
    vec3 kD      = (vec3(1.0) - F) * (1.0 - metallic);
    vec3 diffuse = kD * albedo / PI;

    // Direct lighting
    vec3 radiance = vec3(lightIntensity);
    vec3 Lo = (diffuse + specular) * radiance * NdotL;

    // Ambient
    vec3 ambientColor = ambient.rgb * ambient.a * albedo;

    // Environment reflection
    vec3 envF = fresnelSchlick(NdotV, F0);
    vec3 env  = envF * fakeEnvReflection(R, roughness) * envReflection;

    return ambientColor + Lo + env;
}

// ACES filmic tone mapping
vec3 toneMapACES(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

#endif // SHADING_GLSL
