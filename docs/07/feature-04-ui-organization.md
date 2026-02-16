# Feature 7.4: UI Organization and Presets

**Epic**: [Epic 7 - Control Maps and Polish](../../07/epic-07-control-maps.md)
**Estimated Time**: 2-3 hours
**Prerequisites**: [Feature 7.3 - Base Mesh Rendering](feature-03-base-mesh-rendering.md)

## Goal

Reorganize ImGui interface into logical collapsible sections and implement preset system for common configurations.

## Implementation

```cpp
void renderImGui() {
    ImGui::Begin("Gravel - GPU Mesh Shader Resurfacing");

    // ========================================================================
    // Pipeline Selection
    // ========================================================================
    if (ImGui::CollapsingHeader("Pipeline Selection", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* modes[] = {"Parametric Surfaces", "Pebble Generation"};
        int mode = static_cast<int>(renderMode);
        ImGui::Combo("Render Mode", &mode, modes, 2);
        renderMode = static_cast<RenderMode>(mode);

        ImGui::Checkbox("Show Base Mesh", &showBaseMesh);
        if (showBaseMesh) {
            ImGui::SameLine();
            ImGui::RadioButton("Solid", &wireframeMode, 0);
            ImGui::SameLine();
            ImGui::RadioButton("Wireframe", &wireframeMode, 1);
        }
    }

    // ========================================================================
    // Surface Parameters
    // ========================================================================
    if (renderMode == RENDER_MODE_PARAMETRIC) {
        if (ImGui::CollapsingHeader("Surface Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
            const char* types[] = {"Torus", "Sphere", "Cone", "Cylinder", "...", "B-Spline", "Bézier"};
            ImGui::Combo("Element Type", &elementType, types, 10);

            if (elementType == 0) {
                ImGui::SliderFloat("Major Radius", &torusMajorR, 0.5f, 2.0f);
                ImGui::SliderFloat("Minor Radius", &torusMinorR, 0.1f, 1.0f);
            }
            // ... (other types)

            ImGui::SliderFloat("Global Scale", &userScaling, 0.05f, 0.5f);
            ImGui::SliderInt("Resolution M", &resolutionM, 2, 32);
        }
    } else {
        if (ImGui::CollapsingHeader("Pebble Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderInt("Subdivision", &subdivLevel, 0, 6);
            ImGui::SliderFloat("Extrusion", &extrusionAmount, 0.05f, 0.5f);
            ImGui::SliderFloat("Roundness", &roundness, 1.0f, 3.0f);

            ImGui::Checkbox("Enable Noise", &doNoise);
            if (doNoise) {
                ImGui::SliderFloat("Amplitude", &noiseAmplitude, 0.01f, 0.2f);
                ImGui::SliderFloat("Frequency", &noiseFrequency, 1.0f, 20.0f);
            }
        }
    }

    // ========================================================================
    // Control Maps
    // ========================================================================
    if (ImGui::CollapsingHeader("Control Maps")) {
        ImGui::Text("Current: %s", controlMapFile.c_str());
        if (ImGui::Button("Load Control Map")) {
            loadControlMap("assets/control_maps/example.png");
        }
        ImGui::Checkbox("Use Control Map", &useControlMap);
    }

    // ========================================================================
    // Performance (Culling & LOD)
    // ========================================================================
    if (ImGui::CollapsingHeader("Performance")) {
        ImGui::Checkbox("Enable Culling", &enableCulling);
        if (enableCulling) {
            ImGui::Checkbox("  Frustum Culling", &enableFrustum);
            ImGui::Checkbox("  Back-Face Culling", &enableBackface);
            ImGui::SliderFloat("  Threshold", &cullingThreshold, -1.0f, 1.0f);
        }

        ImGui::Checkbox("Enable LOD", &enableLod);
        if (enableLod) {
            ImGui::SliderFloat("  LOD Factor", &lodFactor, 0.1f, 5.0f);
        }

        ImGui::Text("Statistics:");
        ImGui::Text("  Visible: %u / %u", visibleElements, totalElements);
        ImGui::Text("  FPS: %.1f", ImGui::GetIO().Framerate);
    }

    // ========================================================================
    // Rendering (Lighting & Debug)
    // ========================================================================
    if (ImGui::CollapsingHeader("Rendering")) {
        ImGui::SliderFloat3("Light Position", &lightPos.x, -10.0f, 10.0f);
        ImGui::SliderFloat("Ambient", &ambient, 0.0f, 1.0f);
        ImGui::SliderFloat("Diffuse", &diffuse, 0.0f, 1.0f);
        ImGui::SliderFloat("Specular", &specular, 0.0f, 1.0f);

        const char* debugModes[] = {"Shading", "Normals", "UV", "Primitive ID", "LOD Level"};
        ImGui::Combo("Debug Mode", &debugMode, debugModes, 5);
    }

    // ========================================================================
    // Presets
    // ========================================================================
    if (ImGui::CollapsingHeader("Presets")) {
        if (ImGui::Button("Chain Mail")) applyPresetChainMail();
        ImGui::SameLine();
        if (ImGui::Button("Dragon Scales")) applyPresetDragonScales();
        ImGui::SameLine();
        if (ImGui::Button("Cobblestone")) applyPresetCobblestone();
    }

    ImGui::End();
}

// Preset implementations
void applyPresetChainMail() {
    renderMode = RENDER_MODE_PARAMETRIC;
    elementType = 0;  // Torus
    torusMajorR = 1.0f;
    torusMinorR = 0.3f;
    userScaling = 0.15f;
    resolutionM = resolutionN = 8;
}

void applyPresetDragonScales() {
    renderMode = RENDER_MODE_PEBBLE;
    subdivLevel = 3;
    extrusionAmount = 0.2f;
    roundness = 2.0f;
    doNoise = true;
    noiseAmplitude = 0.08f;
}

void applyPresetCobblestone() {
    renderMode = RENDER_MODE_PEBBLE;
    subdivLevel = 2;
    extrusionAmount = 0.1f;
    roundness = 2.0f;
    doNoise = true;
    noiseAmplitude = 0.05f;
}
```

## Acceptance Criteria

- [ ] UI organized into logical sections
- [ ] All sections collapsible
- [ ] Presets instantly change appearance
- [ ] Professional, clean layout

## Next Steps

Next feature: **[Feature 7.5 - Debug Visualization Modes](feature-05-debug-modes.md)**
