# Feature 14.5: Pathway UI Controls

**Epic**: [Epic 14 - Pebble Pathway](epic-14-pebble-pathway.md)

## Goal

Add a "Pathway" section to the ResurfacingPanel with real-time controls for all pathway parameters.

## File Modified

- `src/ui/ResurfacingPanel.cpp`

## UI Layout

Add a new collapsing header in `ResurfacingPanel::render()`, shown only when third-person mode is active (or always, since the pathway only does anything in that mode):

```cpp
if (ImGui::CollapsingHeader("Pathway")) {
    ImGui::Checkbox("Enable", &r.renderPathway);
    if (r.renderPathway) {
        ImGui::SliderFloat("Radius",      &r.pathwayRadius,    0.5f, 20.0f);
        ImGui::SliderFloat("Back Scale",  &r.pathwayBackScale, 0.0f, 1.0f);
        ImGui::SliderFloat("Falloff",     &r.pathwayFalloff,   0.5f, 8.0f);
        ImGui::Separator();
        ImGui::Text("Pebble settings shared with main Pebbles panel");
    }
}
```

## Notes

- Pathway pebble appearance (extrusion, roundness, subdivision, noise) is inherited from the main `pebbleUBO` settings, so the existing Pebbles panel controls still apply.
- The Pathway panel only controls the zone shape and enable toggle.
- Future: could add preset buttons (Narrow Path, Wide Plaza, Fading Trail).
