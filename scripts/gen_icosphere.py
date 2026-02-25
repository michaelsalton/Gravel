"""
Generate a high-poly icosphere OBJ file.

Algorithm:
  1. Start from a regular icosahedron (12 vertices, 20 faces).
  2. Subdivide 4 times (20 -> 80 -> 320 -> 1280 -> 5120 triangles).
  3. After each subdivision, normalize all new vertices to the unit sphere.
  4. Write a valid OBJ with v, vn, and f (v//vn) lines.
"""

import math

# ---------------------------------------------------------------------------
# Step 1: Build the regular icosahedron
# ---------------------------------------------------------------------------

phi = (1.0 + math.sqrt(5.0)) / 2.0  # golden ratio

# 12 vertices of a regular icosahedron (will be normalized to unit sphere)
ico_vertices = [
    (-1,  phi,  0),
    ( 1,  phi,  0),
    (-1, -phi,  0),
    ( 1, -phi,  0),
    ( 0, -1,  phi),
    ( 0,  1,  phi),
    ( 0, -1, -phi),
    ( 0,  1, -phi),
    ( phi,  0, -1),
    ( phi,  0,  1),
    (-phi,  0, -1),
    (-phi,  0,  1),
]

# Normalize to unit sphere
def normalize(v):
    length = math.sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2])
    return (v[0]/length, v[1]/length, v[2]/length)

vertices = [normalize(v) for v in ico_vertices]

# 20 triangular faces (0-indexed), counter-clockwise winding viewed from outside
faces = [
    # 5 faces around vertex 0
    (0, 11,  5),
    (0,  5,  1),
    (0,  1,  7),
    (0,  7, 10),
    (0, 10, 11),
    # 5 adjacent faces
    (1,  5,  9),
    (5, 11,  4),
    (11, 10,  2),
    (10,  7,  6),
    (7,  1,  8),
    # 5 faces around vertex 3
    (3,  9,  4),
    (3,  4,  2),
    (3,  2,  6),
    (3,  6,  8),
    (3,  8,  9),
    # 5 adjacent faces
    (4,  9,  5),
    (2,  4, 11),
    (6,  2, 10),
    (8,  6,  7),
    (9,  8,  1),
]

# ---------------------------------------------------------------------------
# Step 2: Subdivision
# ---------------------------------------------------------------------------

def midpoint_key(i, j):
    """Canonical key for an edge so that (i,j) and (j,i) map to the same midpoint."""
    return (min(i, j), max(i, j))

def subdivide(verts, tris):
    """
    Subdivide each triangle into 4 sub-triangles.
    New midpoint vertices are normalized onto the unit sphere.
    Midpoints are cached so shared edges produce exactly one new vertex.
    """
    cache = {}
    new_verts = list(verts)
    new_tris = []

    def get_midpoint(i, j):
        key = midpoint_key(i, j)
        if key in cache:
            return cache[key]
        v1 = new_verts[i]
        v2 = new_verts[j]
        mid = normalize(((v1[0]+v2[0])/2.0,
                         (v1[1]+v2[1])/2.0,
                         (v1[2]+v2[2])/2.0))
        idx = len(new_verts)
        new_verts.append(mid)
        cache[key] = idx
        return idx

    for (a, b, c) in tris:
        ab = get_midpoint(a, b)
        bc = get_midpoint(b, c)
        ca = get_midpoint(c, a)
        # 4 sub-triangles preserving winding order
        new_tris.append((a,  ab, ca))
        new_tris.append((ab,  b, bc))
        new_tris.append((ca, bc,  c))
        new_tris.append((ab, bc, ca))

    return new_verts, new_tris

SUBDIVISIONS = 4

for i in range(SUBDIVISIONS):
    vertices, faces = subdivide(vertices, faces)
    print(f"  Subdivision {i+1}: {len(vertices)} vertices, {len(faces)} faces")

# ---------------------------------------------------------------------------
# Step 3: Write OBJ file
# ---------------------------------------------------------------------------

out_path = "d:/Development/Projects/Gravel/assets/icosphere.obj"

with open(out_path, "w") as f:
    f.write("# Icosphere - 4 subdivisions of a regular icosahedron\n")
    f.write(f"# Vertices: {len(vertices)}  Faces: {len(faces)}\n\n")

    # Write vertices
    for (x, y, z) in vertices:
        f.write(f"v {x:.8f} {y:.8f} {z:.8f}\n")

    f.write("\n")

    # Write normals (for a unit sphere, normal == position)
    for (x, y, z) in vertices:
        f.write(f"vn {x:.8f} {y:.8f} {z:.8f}\n")

    f.write("\n")

    # Write faces (1-based indices, v//vn format)
    for (a, b, c) in faces:
        a1, b1, c1 = a + 1, b + 1, c + 1
        f.write(f"f {a1}//{a1} {b1}//{b1} {c1}//{c1}\n")

print(f"\nDone. Wrote {out_path}")
print(f"  Total vertices: {len(vertices)}")
print(f"  Total faces:    {len(faces)}")
