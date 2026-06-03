#version 460                                    // Use GLSL version 4.60
#extension GL_EXT_mesh_shader : require          // Enable mesh shader extension (this frag consumes a mesh-shader pipeline's output)

// =============================================================================
// pebble_cage.frag — Pebble control-cage wireframe fragment shader
//
// Fragment shader for the cage lines emitted by pebble_cage.mesh. It does no
// lighting: each line is drawn in the ring-coded color computed by the mesh
// shader (red/green/blue per cage ring) and passed straight through here.
// =============================================================================

// *** Michael Salton ***

layout(location = 0) in vec4 inColor;           // Interpolated cage line color from the mesh shader (ring-coded)

layout(location = 0) out vec4 outColor;         // Final fragment color written to the color attachment

void main() {                                   // Fragment shader entry point (runs once per covered pixel)
    outColor = inColor;                         // Output the cage color directly (no shading)
}                                               // End of main()

// *** ************ ***
