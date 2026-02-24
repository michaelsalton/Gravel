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
// Blinn-Phong Shading
// ============================================================================

vec3 blinnPhong(vec3 worldPos, vec3 normal, vec3 lightPos, vec3 cameraPos,
                vec4 ambient, float diffuseIntensity, float specularIntensity, float shininess) {
    vec3 N = normalize(normal);
    vec3 L = normalize(lightPos - worldPos);
    vec3 V = normalize(cameraPos - worldPos);
    vec3 H = normalize(L + V);

    // Ambient
    vec3 ambientColor = ambient.rgb * ambient.a;

    // Diffuse (Lambertian)
    float NdotL = max(dot(N, L), 0.0);
    vec3 diffuseColor = vec3(diffuseIntensity) * NdotL;

    // Specular (Blinn-Phong half-vector)
    float NdotH = max(dot(N, H), 0.0);
    float spec = pow(NdotH, shininess);
    vec3 specularColor = vec3(specularIntensity) * spec;

    return ambientColor + diffuseColor + specularColor;
}

#endif // SHADING_GLSL
