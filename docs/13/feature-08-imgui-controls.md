# Feature 13.8: ImGui Controls & Pipeline Toggle

**Epic**: [Epic 13 - Pebble Generation](epic-13-pebble-generation.md)
**Estimated Time**: 2-3 hours
**Prerequisites**: [Feature 13.1](feature-01-pebble-ubo-pipeline.md)

## Goal

Add a comprehensive ImGui panel for pebble parameter control, pipeline mode switching between parametric and pebble modes, and quick-apply presets.

## What You'll Build

- Pipeline mode toggle (Parametric / Pebble)
- Full pebble parameter controls (subdivision, extrusion, roundness, noise, LOD, culling)
- Preset buttons for common configurations
- Info display (resolution, vertex count estimates)

## Files Modified

- `include/ui/ResurfacingPanel.h` -- Add pebble control declarations
- `src/ui/ResurfacingPanel.cpp` -- Implement pebble ImGui controls
- `include/renderer/renderer.h` -- Add pebble state variables (if not already from 13.1)

## Implementation Details

### Step 1: Pipeline Mode Toggle

Add a combo or radio button at the top of the resurfacing panel:

```cpp
const char* pipelineModes[] = { "Parametric", "Pebble" };
int currentMode = renderer.renderPebbles ? 1 : 0;
if (ImGui::Combo("Pipeline Mode", &currentMode, pipelineModes, 2)) {
    renderer.renderPebbles = (currentMode == 1);
    renderer.renderResurfacing = (currentMode == 0);
}
```

When switching modes, the appropriate pipeline and descriptor set are used in the draw path.

### Step 2: Pebble Parameter Controls

Shown only when pebble mode is active:

```cpp
if (renderer.renderPebbles) {
    auto& ubo = renderer.pebbleUBO;

    if (ImGui::CollapsingHeader("Pebble Generation", ImGuiTreeNodeFlags_DefaultOpen)) {

        // Subdivision
        int subdiv = static_cast<int>(ubo.subdivisionLevel);
        if (ImGui::SliderInt("Subdivision Level", &subdiv, 0, 8)) {
            ubo.subdivisionLevel = static_cast<uint32_t>(subdiv);
            ubo.subdivOffset = std::min(ubo.subdivOffset, ubo.subdivisionLevel);
        }
        int subdivOff = static_cast<int>(ubo.subdivOffset);
        ImGui::SliderInt("Subdiv Offset", &subdivOff, 0,
                         std::min(static_cast<int>(ubo.subdivisionLevel), 3));
        ubo.subdivOffset = static_cast<uint32_t>(subdivOff);

        uint32_t resolution = 1 << ubo.subdivisionLevel;
        ImGui::Text("Resolution: %ux%u (%u vertices/patch)",
                    resolution, resolution, (resolution+1)*(resolution+1));

        ImGui::Separator();

        // Extrusion
        ImGui::SliderFloat("Extrusion", &ubo.extrusionAmount, 0.0f, 1.0f, "%.3f");
        ImGui::SliderFloat("Variation", &ubo.extrusionVariation, 0.0f, 1.0f, "%.3f");
        ImGui::SliderFloat("Roundness", &ubo.roundness, 0.0f, 2.0f, "%.2f");

        ImGui::Separator();

        // Noise
        bool doNoise = ubo.doNoise != 0;
        ImGui::Checkbox("Noise", &doNoise);
        ubo.doNoise = doNoise ? 1 : 0;
        if (doNoise) {
            ImGui::SliderFloat("Amplitude", &ubo.noiseAmplitude, 0.0f, 1.0f,
                               "%.3f", ImGuiSliderFlags_Logarithmic);
            ImGui::SliderFloat("Frequency", &ubo.noiseFrequency, 0.0f, 100.0f, "%.1f");
        }

        ImGui::Separator();

        // Culling
        bool useCulling = ubo.useCulling != 0;
        ImGui::Checkbox("Culling", &useCulling);
        ubo.useCulling = useCulling ? 1 : 0;
        if (useCulling) {
            ImGui::SameLine();
            ImGui::SliderFloat("Threshold", &ubo.cullingThreshold, 0.0f, 1.0f, "%.2f");
        }

        // LOD
        bool useLod = ubo.useLod != 0;
        ImGui::Checkbox("LOD", &useLod);
        ubo.useLod = useLod ? 1 : 0;
        if (useLod) {
            ImGui::SameLine();
            ImGui::SliderFloat("Factor", &ubo.lodFactor, 0.0f, 10.0f, "%.2f");
            bool allowLow = ubo.allowLowLod != 0;
            ImGui::Checkbox("Allow Low LOD (N=0)", &allowLow);
            ubo.allowLowLod = allowLow ? 1 : 0;
        }
    }
}
```

### Step 3: Presets

```cpp
if (ImGui::CollapsingHeader("Presets")) {
    if (ImGui::Button("Smooth Pebbles")) {
        ubo.subdivisionLevel = 3;
        ubo.extrusionAmount = 0.15f;
        ubo.extrusionVariation = 0.3f;
        ubo.roundness = 2.0f;
        ubo.doNoise = 0;
    }
    ImGui::SameLine();
    if (ImGui::Button("Rocky Pebbles")) {
        ubo.subdivisionLevel = 4;
        ubo.extrusionAmount = 0.2f;
        ubo.extrusionVariation = 0.5f;
        ubo.roundness = 1.0f;
        ubo.doNoise = 1;
        ubo.noiseAmplitude = 0.08f;
        ubo.noiseFrequency = 5.0f;
    }
    ImGui::SameLine();
    if (ImGui::Button("Cobblestone")) {
        ubo.subdivisionLevel = 2;
        ubo.extrusionAmount = 0.1f;
        ubo.extrusionVariation = 0.2f;
        ubo.roundness = 2.0f;
        ubo.doNoise = 1;
        ubo.noiseAmplitude = 0.05f;
        ubo.noiseFrequency = 8.0f;
    }
}
```

### Step 4: UBO Update Per Frame

In the renderer's frame update, write the PebbleUBO to GPU:
```cpp
if (renderPebbles && pebbleUBOMapped) {
    pebbleUBO.time += deltaTime;
    memcpy(pebbleUBOMapped, &pebbleUBO, sizeof(PebbleUBO));
}
```

## UI Layout

```
Pipeline Mode: [Parametric ▼] / [Pebble ▼]

--- When Pebble is selected ---

Pebble Generation:
  Subdivision Level: [====3====]
  Subdiv Offset:     [====0====]
  Resolution: 8x8 (81 vertices/patch)
  ---
  Extrusion:  [===0.100===]
  Variation:  [===0.500===]
  Roundness:  [===2.00====]
  ---
  [x] Noise
    Amplitude: [===0.010===]
    Frequency: [===50.0====]
  ---
  [ ] Culling  Threshold: [===0.10===]
  [ ] LOD      Factor:    [===1.00===]
    [ ] Allow Low LOD (N=0)

Presets:
  [Smooth Pebbles] [Rocky Pebbles] [Cobblestone]
```

## Acceptance Criteria

- [ ] Pipeline mode toggle switches between parametric and pebble rendering
- [ ] All pebble parameters are adjustable in real-time
- [ ] Subdivision level slider correctly bounds subdivOffset
- [ ] Resolution info text updates with subdivision level
- [ ] Noise controls only appear when noise is enabled
- [ ] LOD/Culling controls only appear when their toggles are on
- [ ] Presets apply correct parameter sets
- [ ] UBO is updated each frame when pebble mode is active

## Next Steps

**[Feature 13.9 - Integration & Polish](feature-09-integration-polish.md)**
