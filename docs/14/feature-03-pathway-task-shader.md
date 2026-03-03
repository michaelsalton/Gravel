# Feature 14.3: Pathway Culling in pebble.task

**Epic**: [Epic 14 - Pebble Pathway](epic-14-pebble-pathway.md)

## Goal

Add a distance-based teardrop culling block to `pebble.task` that uses the new PebbleUBO pathway fields. Faces outside the zone are culled; faces inside have `OUT.scale` reduced for smooth edge falloff.

## File Modified

- `shaders/pebbles/pebble.task`

## Insertion Point

After the mask texture culling block (line ~149), before the patch count calculation:

```glsl
// ==================== Pathway Culling ====================
if (pebbleUbo.usePathway != 0u) {
    vec2 toFaceXZ  = center.xz - pebbleUbo.playerWorldPos.xz;
    vec2 fwdXZ     = normalize(pebbleUbo.playerForward.xz);
    float projFwd  = dot(toFaceXZ, fwdXZ);         // positive = ahead of player
    float projSide = length(toFaceXZ - projFwd * fwdXZ); // lateral distance

    // Asymmetric radius: larger ahead, smaller behind
    float radius = (projFwd >= 0.0)
                   ? pebbleUbo.pathwayRadius
                   : pebbleUbo.pathwayRadius * pebbleUbo.pathwayBackScale;

    // Elliptical normalised distance
    float dist = length(vec2(projFwd / radius,
                             projSide / pebbleUbo.pathwayRadius));

    // Smooth falloff → feeds directly into pebble scale
    OUT.scale = pow(clamp(1.0 - dist, 0.0, 1.0), pebbleUbo.pathwayFalloff);

    if (OUT.scale < 0.01) return; // cull: no mesh tasks emitted
}
```

## How OUT.scale flows

`OUT.scale` is already declared in the `Task` payload struct and read by `pebble.mesh`'s `emitVertex()`:
```glsl
vec3 desiredPos = (pos - center) * IN.scale + center;
```
So a scale of 0.5 shrinks the pebble to half size toward the face center — the falloff effect is automatic with no other changes needed.

## Edge Cases

- If `playerForward.xz` is zero-length (shouldn't happen but guard): `normalize` produces NaN — pre-normalise on CPU before upload.
- Faces directly behind player: `projFwd` is negative, `radius = pathwayRadius * pathwayBackScale` (small), cull quickly.
