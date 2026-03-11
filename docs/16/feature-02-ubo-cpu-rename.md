# Feature 16.2: UBO and CPU-Side Rename

**Epic**: [Epic 16 - PBR Lighting](epic-16-pbr-lighting.md)

## Goal

Repurpose the existing `GlobalShadingUBO` fields for PBR parameters. The struct stays the same size (48 bytes) so no descriptor set, buffer, or pipeline layout changes are needed.

## Files Modified

- `include/renderer/renderer.h` — struct definition + member variable defaults
- `src/renderer/renderer.cpp` — UBO upload code
- `src/renderer/renderer_imgui.cpp` — ImGui sliders, preset functions, debug label
- `src/ui/ResurfacingPanel.cpp` — remove chainmail metallic sliders
- `include/level/LevelPreset.h` — struct field renames
- `src/level/LevelPreset.cpp` — preset value updates

## GlobalShadingUBO Struct (renderer.h:60-71)

```cpp
struct GlobalShadingUBO {
    glm::vec4 lightPosition;   // xyz = position
    glm::vec4 ambient;         // rgb = color, a = intensity
    float roughness;           // was: diffuse
    float metallic;            // was: specular
    float ao;                  // was: shininess
    float dielectricF0;        // was: metalF0
    float envReflection;       // unchanged
    float lightIntensity;      // was: metalDiffuse
    float padding1;
    float padding2;
};
```

## Renderer Member Variables (renderer.h:263-271)

```cpp
// Lighting config
glm::vec3 lightPosition = glm::vec3(5.0f, 5.0f, 5.0f);
glm::vec3 ambientColor = glm::vec3(0.2f, 0.2f, 0.25f);
float ambientIntensity = 1.0f;
float roughness = 0.5f;           // was: diffuseIntensity = 0.7f
float metallic = 0.0f;            // was: specularIntensity = 0.5f
float ao = 1.0f;                  // was: shininess = 32.0f
float dielectricF0 = 0.04f;       // was: metalF0 = 0.65f
float envReflection = 0.35f;      // unchanged
float lightIntensity = 3.0f;      // was: metalDiffuse = 0.3f
```

## UBO Upload (renderer.cpp ~line 352)

```cpp
GlobalShadingUBO shadingData{};
shadingData.lightPosition = glm::vec4(lightPosition, 0.0f);
shadingData.ambient = glm::vec4(ambientColor, ambientIntensity);
shadingData.roughness = roughness;
shadingData.metallic = metallic;
shadingData.ao = ao;
shadingData.dielectricF0 = dielectricF0;
shadingData.envReflection = envReflection;
shadingData.lightIntensity = lightIntensity;
```

## ImGui Sliders (renderer_imgui.cpp:443-450)

Replace with:

```cpp
if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::DragFloat3("Light Position", &lightPosition.x, 0.1f, -20.0f, 20.0f);
    ImGui::SliderFloat("Light Intensity", &lightIntensity, 0.0f, 10.0f);
    ImGui::ColorEdit3("Ambient Color", &ambientColor.x);
    ImGui::SliderFloat("Ambient Intensity", &ambientIntensity, 0.0f, 1.0f);
    ImGui::Separator();
    ImGui::Text("PBR Material");
    ImGui::SliderFloat("Roughness", &roughness, 0.05f, 1.0f);
    ImGui::SliderFloat("Metallic", &metallic, 0.0f, 1.0f);
    ImGui::SliderFloat("AO", &ao, 0.0f, 1.0f);
    ImGui::SliderFloat("Dielectric F0", &dielectricF0, 0.0f, 0.2f, "%.3f");
    ImGui::SliderFloat("Env Reflection", &envReflection, 0.0f, 1.0f);
}
```

Also update the debug mode label from `"Shading (Blinn-Phong)"` to `"Shading (PBR)"` (line 455).

## Remove Chainmail Metallic Sliders (ResurfacingPanel.cpp:111-114)

Delete these lines from the chainmail section:

```cpp
ImGui::Text("Metallic Shading:");
ImGui::SliderFloat("Metal Reflectance", &r.metalF0, 0.0f, 1.0f, "%.2f");
ImGui::SliderFloat("Env Reflection", &r.envReflection, 0.0f, 1.0f, "%.2f");
ImGui::SliderFloat("Metal Diffuse", &r.metalDiffuse, 0.0f, 1.0f, "%.2f");
```

## Chainmail Preset (renderer_imgui.cpp:551-560)

Update `applyPresetChainMail()` to use PBR values:

```cpp
roughness      = 0.3f;
metallic       = 1.0f;
ao             = 1.0f;
dielectricF0   = 0.04f;
envReflection  = 0.35f;
lightIntensity = 3.0f;
```

## LevelPreset (LevelPreset.h + LevelPreset.cpp)

Rename struct fields:

```cpp
struct LevelPreset {
    // ... existing fields ...

    // Lighting
    glm::vec3 lightPosition;
    glm::vec3 ambientColor;
    float ambientIntensity;
    float roughness;           // was: diffuseIntensity
    float metallic;            // was: specularIntensity
    float ao;                  // was: shininess
    float dielectricF0;        // was: metalF0
    float envReflection;
    float lightIntensity;      // was: metalDiffuse
};
```

Preset values:

| Preset | roughness | metallic | ao | dielectricF0 | lightIntensity |
|--------|-----------|----------|-----|--------------|----------------|
| Sphere World | 0.5 | 0.0 | 1.0 | 0.04 | 3.0 |
| Chainmail Man | 0.3 | 1.0 | 1.0 | 0.04 | 3.0 |

## Apply Preset (renderer_imgui.cpp:610-620)

Rename all field assignments to match new names.
