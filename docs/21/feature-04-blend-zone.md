# Feature 21.4: Blend Zone Crossfade

**Epic**: [Epic 21 - Proxy Shading](epic-21-proxy-shading.md)

## Goal

Create a smooth visual transition between full procedural geometry and full proxy shading. Faces in the blend zone receive both real geometry (at reduced alpha) and proxy shading (at increasing opacity), crossfading so the transition is invisible.

## Blend Zone Mechanics

```
Screen size:  [large] ←————————————→ [small]

                proxyStart          proxyEnd
                    |                   |
Full geometry ======|=====blend zone====|====== Full proxy
alpha = 1.0         |  alpha decreases  |       alpha = 0.0
proxyBlend = 0.0    |  blends 0→1       |       proxyBlend = 1.0
```

In the blend zone:
- **Procedural geometry** renders with `screenAlpha = 1.0 - proxyBlend` (fading out via alpha-to-coverage)
- **Base mesh proxy** renders with `blend = proxyBlend` (fading in)
- The two layers composite naturally through the depth buffer and alpha blending

## UI Controls

- **Proxy Start** slider: NDC size where blending begins (default 0.015)
- **Proxy End** slider: NDC size where geometry is fully replaced (default 0.005)
- **Enable Proxy** toggle

## Files Modified

- `src/ui/AdvancedPanel.cpp` — proxy toggle + threshold sliders
- `include/renderer/renderer.h` — `enableProxy`, `proxyStartThreshold`, `proxyEndThreshold`
- `shaders/include/shaderInterface.h` — UBO fields for proxy thresholds

## Verification

1. Set base mesh to "Off", enable proxy — distant faces should show proxy color, close faces show geometry
2. Slowly zoom out — watch elements dissolve into proxy (should be seamless)
3. Adjust Proxy Start/End sliders — transition zone moves closer/farther
4. Set Start = End — hard cut (no blend), should still look acceptable
5. Performance: distant faces cost nearly zero (no mesh shader invocations), just base mesh fragment shading
