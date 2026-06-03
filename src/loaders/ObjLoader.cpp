#include "loaders/ObjLoader.h"                   // NGonMesh, NGonFace, ObjLoader declarations
#include <fstream>                               // std::ifstream for reading the OBJ file
#include <sstream>                               // std::istringstream for parsing each line
#include <iostream>                              // Console logging
#include <stdexcept>                             // std::runtime_error on open failure
#include <map>                                   // std::map (used by the AI-generated subdivision code)
#include <set>                                   // std::set (used by the AI-generated subdivision code)
#include <unordered_map>                         // std::unordered_map for the vertex-dedup table

// =============================================================================
// ObjLoader.cpp — Wavefront OBJ parsing, vertex splitting, and triangulation
//
// Implements ObjLoader::load (parse v/vn/vt/f records → NGonMesh, then split
// vertices at UV/normal seams into a unified per-vertex layout and precompute
// per-face normal/centroid/area), ObjLoader::triangulate (fan-triangulate all
// faces), and the geometric face helpers. The lower part of the file holds the
// AI-generated subdivision routines.
// =============================================================================

// *** Michael Salton ***

NGonMesh ObjLoader::load(const std::string& filepath) { // Parse an OBJ file into an NGonMesh
    std::ifstream file(filepath);                // Open the file for reading
    if (!file.is_open()) {                       // Bail if it can't be opened
        throw std::runtime_error("Failed to open OBJ file: " + filepath);
    }

    NGonMesh mesh;                               // Output mesh (raw v/vn/vt arrays filled first)
    std::string line;                            // Current line being parsed
    uint32_t faceVertexOffset = 0;               // Running offset into faceVertexIndices

    std::cout << "Loading OBJ: " << filepath << std::endl; // Progress log

    while (std::getline(file, line)) {           // Read the file line by line
        if (line.empty() || line[0] == '#') continue; // Skip blank lines and comments

        std::istringstream iss(line);            // Tokenize the line
        std::string prefix;                      // Record type ("v", "vn", "vt", "f", ...)
        iss >> prefix;

        if (prefix == "v") {                     // Vertex position
            glm::vec3 pos;
            iss >> pos.x >> pos.y >> pos.z;
            mesh.positions.push_back(pos);

        } else if (prefix == "vn") {             // Vertex normal
            glm::vec3 normal;
            iss >> normal.x >> normal.y >> normal.z;
            mesh.normals.push_back(glm::normalize(normal)); // Store normalized

        } else if (prefix == "vt") {             // Texture coordinate
            glm::vec2 texCoord;
            iss >> texCoord.x >> texCoord.y;
            mesh.texCoords.push_back(texCoord);

        } else if (prefix == "f") {              // Face: a list of vertex references
            NGonFace face;
            std::string token;                   // One "v/vt/vn" reference

            while (iss >> token) {               // Parse each vertex reference of the face
                uint32_t vIdx = 0, vtIdx = 0, vnIdx = 0; // Position / texcoord / normal indices (0 = absent)

                // Parse face vertex: v, v/vt, v/vt/vn, or v//vn
                size_t firstSlash = token.find('/'); // Locate the first separator
                if (firstSlash == std::string::npos) {
                    // Format: v
                    vIdx = static_cast<uint32_t>(std::stoi(token)); // Position only
                } else {
                    vIdx = static_cast<uint32_t>(std::stoi(token.substr(0, firstSlash))); // Position before first slash
                    size_t secondSlash = token.find('/', firstSlash + 1); // Locate the second separator

                    if (secondSlash == std::string::npos) {
                        // Format: v/vt
                        vtIdx = static_cast<uint32_t>(std::stoi(token.substr(firstSlash + 1))); // Texcoord after first slash
                    } else if (secondSlash == firstSlash + 1) {
                        // Format: v//vn
                        vnIdx = static_cast<uint32_t>(std::stoi(token.substr(secondSlash + 1))); // Normal after empty texcoord
                    } else {
                        // Format: v/vt/vn
                        vtIdx = static_cast<uint32_t>(std::stoi(            // Texcoord between the two slashes
                            token.substr(firstSlash + 1, secondSlash - firstSlash - 1)));
                        vnIdx = static_cast<uint32_t>(std::stoi(token.substr(secondSlash + 1))); // Normal after second slash
                    }
                }

                // OBJ is 1-based, convert to 0-based
                face.vertexIndices.push_back(vIdx - 1); // Store 0-based position index
                if (vtIdx > 0) face.texCoordIndices.push_back(vtIdx - 1); // Store texcoord index if present
                if (vnIdx > 0) face.normalIndices.push_back(vnIdx - 1);   // Store normal index if present
            }

            if (face.vertexIndices.size() < 3) continue; // Skip degenerate faces (< 3 vertices)

            face.count = static_cast<uint32_t>(face.vertexIndices.size()); // Side count
            face.offset = faceVertexOffset;      // Where this face's indices begin
            face.normal = glm::vec4(computeFaceNormal(mesh.positions, face.vertexIndices), 0.0f); // Precompute normal
            face.center = glm::vec4(computeFaceCentroid(mesh.positions, face.vertexIndices), 1.0f); // Precompute centroid
            face.area = computeFaceArea(mesh.positions, face.vertexIndices); // Precompute area

            for (uint32_t idx : face.vertexIndices) { // Append this face's indices to the flat array
                mesh.faceVertexIndices.push_back(idx);
            }
            faceVertexOffset += face.count;      // Advance the flat-array offset

            mesh.faces.push_back(face);          // Store the face
        }
    }

    // Remap from OBJ's independent v/vt/vn indexing to unified per-vertex arrays.
    // Vertices at UV or normal seams are split (duplicated) so each unique
    // (position, texcoord, normal) combination gets its own vertex.
    std::vector<glm::vec3> rawPositions = std::move(mesh.positions); // Stash the raw parsed arrays...
    std::vector<glm::vec3> rawNormals = std::move(mesh.normals);
    std::vector<glm::vec2> rawTexCoords = std::move(mesh.texCoords);
    mesh.positions.clear();                      // ...then rebuild the mesh arrays as unified vertices
    mesh.normals.clear();
    mesh.texCoords.clear();
    mesh.originalVertexCount = static_cast<uint32_t>(rawPositions.size()); // Distinct OBJ positions before splitting
    mesh.originalVertexIndices.clear();

    struct VertexKey {                           // Unique vertex identity = (position, texcoord, normal) triple
        uint32_t v, vt, vn;
        bool operator==(const VertexKey& o) const { return v == o.v && vt == o.vt && vn == o.vn; }
    };
    struct VertexKeyHash {                        // Hash combiner for VertexKey (boost-style mix)
        size_t operator()(const VertexKey& k) const {
            size_t h = std::hash<uint32_t>()(k.v);
            h ^= std::hash<uint32_t>()(k.vt) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<uint32_t>()(k.vn) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };
    std::unordered_map<VertexKey, uint32_t, VertexKeyHash> vertexMap; // Dedup table: triple → unified vertex index

    // Rebuild face vertex indices and faceVertexIndices with split vertices
    mesh.faceVertexIndices.clear();              // Rebuild the flat index array against the new vertices
    uint32_t newOffset = 0;                      // Running offset into the new flat array
    for (auto& face : mesh.faces) {              // Remap every face's corners
        std::vector<uint32_t> newIndices;        // This face's new (unified) indices
        for (uint32_t i = 0; i < face.vertexIndices.size(); ++i) {
            uint32_t vi = face.vertexIndices[i]; // Original position index
            uint32_t ti = (i < face.texCoordIndices.size()) ? face.texCoordIndices[i] + 1 : 0; // Texcoord (+1 so 0 = absent)
            uint32_t ni = (i < face.normalIndices.size()) ? face.normalIndices[i] + 1 : 0;      // Normal (+1 so 0 = absent)
            VertexKey key{vi, ti, ni};           // Identity of this corner

            auto it = vertexMap.find(key);       // Have we already created this unique vertex?
            uint32_t idx;                        // Its unified index
            if (it != vertexMap.end()) {         // Seen before...
                idx = it->second;                // ...reuse it
            } else {                             // First time...
                idx = static_cast<uint32_t>(mesh.positions.size()); // ...allocate a new unified vertex
                vertexMap[key] = idx;            // Remember it
                mesh.positions.push_back(rawPositions[vi]); // Copy its position
                mesh.normals.push_back((ni > 0 && (ni - 1) < rawNormals.size()) // Copy its normal (default +Z if absent)
                    ? rawNormals[ni - 1] : glm::vec3(0.0f, 0.0f, 1.0f));
                mesh.texCoords.push_back((ti > 0 && (ti - 1) < rawTexCoords.size()) // Copy its texcoord (default 0 if absent)
                    ? rawTexCoords[ti - 1] : glm::vec2(0.0f));
                mesh.originalVertexIndices.push_back(vi); // Record the original OBJ position it came from
            }
            newIndices.push_back(idx);           // Add the unified index to this face
        }
        face.vertexIndices = newIndices;         // Replace the face's indices
        face.offset = newOffset;                 // Update its offset into the flat array
        for (uint32_t idx : newIndices)          // Append to the rebuilt flat index array
            mesh.faceVertexIndices.push_back(idx);
        newOffset += face.count;                 // Advance the offset
        // Normal/texcoord indices no longer needed after remapping
        face.normalIndices.clear();              // These were only needed for splitting
        face.texCoordIndices.clear();
    }
    mesh.colors.resize(mesh.positions.size(), glm::vec3(1.0f)); // Default all vertex colors to white

    mesh.nbVertices = static_cast<uint32_t>(mesh.positions.size()); // Final vertex count (after splitting)
    mesh.nbFaces = static_cast<uint32_t>(mesh.faces.size());        // Face count

    // Print face type distribution
    int triCount = 0, quadCount = 0, ngonCount = 0; // Tally face types for the log
    for (const auto& face : mesh.faces) {
        if (face.count == 3) triCount++;         // Triangle
        else if (face.count == 4) quadCount++;   // Quad
        else ngonCount++;                        // 5+ sides
    }

    std::cout << "Loaded OBJ: " << mesh.nbVertices << " vertices, " // Log the result
              << mesh.nbFaces << " faces" << std::endl;
    std::cout << "  Triangles: " << triCount
              << ", Quads: " << quadCount
              << ", N-gons: " << ngonCount << std::endl;

    return mesh;                                 // Return the unified, precomputed mesh
}

void ObjLoader::triangulate(NGonMesh& mesh) {    // Convert all faces to triangles in place
    std::vector<NGonFace> newFaces;              // Rebuilt face list (all triangles)
    std::vector<uint32_t> newFaceVertexIndices;  // Rebuilt flat index array
    uint32_t offset = 0;                         // Running offset into the flat array

    for (const auto& face : mesh.faces) {        // Process every face
        if (face.count <= 3) {                   // Already a triangle...
            // Already a triangle, keep as-is
            NGonFace tri = face;                 // ...copy it unchanged
            tri.offset = offset;                 // Fix up its flat-array offset
            for (uint32_t idx : tri.vertexIndices) {
                newFaceVertexIndices.push_back(idx);
            }
            offset += tri.count;
            newFaces.push_back(tri);
        } else {                                 // Polygon with 4+ sides...
            // Fan-triangulate: vertex 0, i+1, i+2
            for (uint32_t i = 0; i < face.count - 2; i++) { // Emit a fan of (count-2) triangles
                NGonFace tri;
                tri.vertexIndices = {            // Each triangle shares vertex 0
                    face.vertexIndices[0],
                    face.vertexIndices[i + 1],
                    face.vertexIndices[i + 2]
                };
                tri.count = 3;                   // Triangle
                tri.offset = offset;             // Its offset into the flat array
                tri.normal = glm::vec4(computeFaceNormal(mesh.positions, tri.vertexIndices), 0.0f); // Recompute normal
                tri.center = glm::vec4(computeFaceCentroid(mesh.positions, tri.vertexIndices), 1.0f); // Recompute centroid
                tri.area = computeFaceArea(mesh.positions, tri.vertexIndices); // Recompute area
                for (uint32_t idx : tri.vertexIndices) {
                    newFaceVertexIndices.push_back(idx);
                }
                offset += 3;
                newFaces.push_back(tri);
            }
        }
    }

    mesh.faces = std::move(newFaces);            // Swap in the triangulated faces
    mesh.faceVertexIndices = std::move(newFaceVertexIndices); // ...and flat index array
    mesh.nbFaces = static_cast<uint32_t>(mesh.faces.size());  // Update face count

    std::cout << "Triangulated: " << mesh.nbFaces << " triangles" << std::endl; // Log
}

glm::vec3 ObjLoader::computeFaceNormal(          // Geometric normal from the first three vertices
    const std::vector<glm::vec3>& positions,
    const std::vector<uint32_t>& indices) {

    if (indices.size() < 3) {                    // Need at least a triangle...
        return glm::vec3(0.0f, 0.0f, 1.0f);      // ...else default to +Z
    }

    glm::vec3 v0 = positions[indices[0]];        // First three corners define the plane
    glm::vec3 v1 = positions[indices[1]];
    glm::vec3 v2 = positions[indices[2]];

    glm::vec3 normal = glm::cross(v1 - v0, v2 - v0); // Cross of two edge vectors
    float len = glm::length(normal);             // Length (0 if the corners are collinear)

    if (len > 0.0f) {                            // Valid normal...
        return normal / len;                     // ...normalize it
    }
    return glm::vec3(0.0f, 0.0f, 1.0f);          // Degenerate face → default +Z
}

glm::vec3 ObjLoader::computeFaceCentroid(        // Average of the face's vertex positions
    const std::vector<glm::vec3>& positions,
    const std::vector<uint32_t>& indices) {

    glm::vec3 centroid(0.0f);                    // Position accumulator
    for (uint32_t idx : indices) {               // Sum all corner positions
        centroid += positions[idx];
    }
    return centroid / static_cast<float>(indices.size()); // Divide by corner count
}

float ObjLoader::computeFaceArea(                // Polygon area via a triangle fan
    const std::vector<glm::vec3>& positions,
    const std::vector<uint32_t>& indices) {

    if (indices.size() < 3) return 0.0f;         // No area for fewer than 3 vertices

    // Triangle fan from first vertex
    float totalArea = 0.0f;                      // Area accumulator
    glm::vec3 v0 = positions[indices[0]];        // Fan apex

    for (size_t i = 1; i < indices.size() - 1; ++i) { // Sum the fan triangles
        glm::vec3 edge1 = positions[indices[i]] - v0;     // Edge to the i-th corner
        glm::vec3 edge2 = positions[indices[i + 1]] - v0; // Edge to the next corner
        totalArea += glm::length(glm::cross(edge1, edge2)) * 0.5f; // Triangle area = ½|edge1 × edge2|
    }

    return totalArea;                            // Total polygon area
}

// *** ************ ***

// *** AI Generated ***

void ObjLoader::subdivideFlat(NGonMesh& mesh, int levels) {
    using Edge = std::pair<uint32_t, uint32_t>;
    auto makeEdge = [](uint32_t a, uint32_t b) -> Edge {
        return a < b ? Edge{a, b} : Edge{b, a};
    };

    for (int lvl = 0; lvl < levels; lvl++) {
        uint32_t nVerts = static_cast<uint32_t>(mesh.positions.size());
        uint32_t nFaces = static_cast<uint32_t>(mesh.faces.size());

        mesh.normals.resize(nVerts, glm::vec3(0, 1, 0));
        mesh.texCoords.resize(nVerts, glm::vec2(0));
        mesh.colors.resize(nVerts, glm::vec3(1));

        NGonMesh result;

        // Copy original vertices unchanged
        result.positions = mesh.positions;
        result.normals   = mesh.normals;
        result.texCoords = mesh.texCoords;
        result.colors    = mesh.colors;

        // Add face center vertices
        std::vector<uint32_t> facePointIdx(nFaces);
        for (uint32_t fi = 0; fi < nFaces; fi++) {
            const auto& vi = mesh.faces[fi].vertexIndices;
            uint32_t n = mesh.faces[fi].count;
            glm::vec3 p(0), c(0), nm(0); glm::vec2 uv(0);
            for (uint32_t i = 0; i < n; i++) {
                p  += mesh.positions[vi[i]];
                uv += mesh.texCoords[vi[i]];
                c  += mesh.colors[vi[i]];
                nm += mesh.normals[vi[i]];
            }
            float inv = 1.0f / float(n);
            facePointIdx[fi] = static_cast<uint32_t>(result.positions.size());
            result.positions.push_back(p * inv);
            result.normals.push_back(glm::normalize(nm * inv));
            result.texCoords.push_back(uv * inv);
            result.colors.push_back(c * inv);
        }

        // Add edge midpoint vertices
        std::map<Edge, uint32_t> edgePointIdx;
        for (uint32_t fi = 0; fi < nFaces; fi++) {
            const auto& vi = mesh.faces[fi].vertexIndices;
            uint32_t n = mesh.faces[fi].count;
            for (uint32_t i = 0; i < n; i++) {
                Edge e = makeEdge(vi[i], vi[(i + 1) % n]);
                if (edgePointIdx.count(e)) continue;
                uint32_t a = e.first, b = e.second;
                edgePointIdx[e] = static_cast<uint32_t>(result.positions.size());
                result.positions.push_back((mesh.positions[a] + mesh.positions[b]) * 0.5f);
                result.normals.push_back(glm::normalize(mesh.normals[a] + mesh.normals[b]));
                result.texCoords.push_back((mesh.texCoords[a] + mesh.texCoords[b]) * 0.5f);
                result.colors.push_back((mesh.colors[a] + mesh.colors[b]) * 0.5f);
            }
        }

        // Create quads: corner, edge_next, face_center, edge_prev
        uint32_t offset = 0;
        for (uint32_t fi = 0; fi < nFaces; fi++) {
            const auto& vi = mesh.faces[fi].vertexIndices;
            uint32_t n = mesh.faces[fi].count;
            uint32_t fp = facePointIdx[fi];

            for (uint32_t i = 0; i < n; i++) {
                uint32_t corner  = vi[i];
                uint32_t ep_next = edgePointIdx[makeEdge(vi[i], vi[(i + 1) % n])];
                uint32_t ep_prev = edgePointIdx[makeEdge(vi[(i + n - 1) % n], vi[i])];

                NGonFace quad;
                quad.vertexIndices = { corner, ep_next, fp, ep_prev };
                quad.count = 4;
                quad.offset = offset;
                quad.normal = glm::vec4(computeFaceNormal(result.positions, quad.vertexIndices), 0.0f);
                quad.center = glm::vec4(computeFaceCentroid(result.positions, quad.vertexIndices), 1.0f);
                quad.area   = computeFaceArea(result.positions, quad.vertexIndices);

                for (uint32_t idx : quad.vertexIndices)
                    result.faceVertexIndices.push_back(idx);
                offset += 4;

                result.faces.push_back(std::move(quad));
            }
        }

        result.nbVertices = static_cast<uint32_t>(result.positions.size());
        result.nbFaces    = static_cast<uint32_t>(result.faces.size());

        mesh = std::move(result);

        std::cout << "Flat subdivided (level " << (lvl + 1) << "): "
                  << mesh.nbFaces << " faces, "
                  << mesh.nbVertices << " vertices" << std::endl;
    }
}

void ObjLoader::subdivide(NGonMesh& mesh, int levels) {
    using Edge = std::pair<uint32_t, uint32_t>;
    auto makeEdge = [](uint32_t a, uint32_t b) -> Edge {
        return a < b ? Edge{a, b} : Edge{b, a};
    };

    for (int lvl = 0; lvl < levels; lvl++) {
        uint32_t nVerts = static_cast<uint32_t>(mesh.positions.size());
        uint32_t nFaces = static_cast<uint32_t>(mesh.faces.size());

        // Ensure arrays match
        mesh.normals.resize(nVerts, glm::vec3(0, 1, 0));
        mesh.texCoords.resize(nVerts, glm::vec2(0));
        mesh.colors.resize(nVerts, glm::vec3(1));

        // === Step 1: Build adjacency ===
        std::map<Edge, std::vector<uint32_t>> edgeToFaces;
        std::vector<std::vector<uint32_t>> vertexToFaces(nVerts);
        std::vector<std::set<uint32_t>> vertexNeighbors(nVerts);

        for (uint32_t fi = 0; fi < nFaces; fi++) {
            const auto& vi = mesh.faces[fi].vertexIndices;
            uint32_t n = mesh.faces[fi].count;
            for (uint32_t i = 0; i < n; i++) {
                uint32_t v0 = vi[i], v1 = vi[(i + 1) % n];
                vertexToFaces[v0].push_back(fi);
                vertexNeighbors[v0].insert(v1);
                vertexNeighbors[v1].insert(v0);
                edgeToFaces[makeEdge(v0, v1)].push_back(fi);
            }
        }

        // === Step 2: Compute face points ===
        std::vector<glm::vec3> facePoints(nFaces);
        std::vector<glm::vec2> facePointUVs(nFaces);
        std::vector<glm::vec3> facePointColors(nFaces);

        for (uint32_t fi = 0; fi < nFaces; fi++) {
            const auto& vi = mesh.faces[fi].vertexIndices;
            uint32_t n = mesh.faces[fi].count;
            glm::vec3 p(0), c(0); glm::vec2 uv(0);
            for (uint32_t i = 0; i < n; i++) {
                p += mesh.positions[vi[i]];
                uv += mesh.texCoords[vi[i]];
                c += mesh.colors[vi[i]];
            }
            float inv = 1.0f / float(n);
            facePoints[fi] = p * inv;
            facePointUVs[fi] = uv * inv;
            facePointColors[fi] = c * inv;
        }

        // === Step 3: Compute edge points ===
        std::map<Edge, glm::vec3> edgePointPos;
        std::map<Edge, glm::vec2> edgePointUV;
        std::map<Edge, glm::vec3> edgePointCol;
        std::map<Edge, bool> edgeIsBoundary;

        for (auto& [edge, faces] : edgeToFaces) {
            glm::vec3 v0 = mesh.positions[edge.first];
            glm::vec3 v1 = mesh.positions[edge.second];
            glm::vec2 uv0 = mesh.texCoords[edge.first];
            glm::vec2 uv1 = mesh.texCoords[edge.second];
            glm::vec3 c0 = mesh.colors[edge.first];
            glm::vec3 c1 = mesh.colors[edge.second];

            if (faces.size() == 2) {
                // Interior edge: average of endpoints + adjacent face points
                glm::vec3 fp = facePoints[faces[0]] + facePoints[faces[1]];
                edgePointPos[edge] = (v0 + v1 + fp) * 0.25f;
                edgePointUV[edge] = (uv0 + uv1 + facePointUVs[faces[0]] + facePointUVs[faces[1]]) * 0.25f;
                edgePointCol[edge] = (c0 + c1 + facePointColors[faces[0]] + facePointColors[faces[1]]) * 0.25f;
                edgeIsBoundary[edge] = false;
            } else {
                // Boundary edge: simple midpoint
                edgePointPos[edge] = (v0 + v1) * 0.5f;
                edgePointUV[edge] = (uv0 + uv1) * 0.5f;
                edgePointCol[edge] = (c0 + c1) * 0.5f;
                edgeIsBoundary[edge] = true;
            }
        }

        // === Step 4: Compute smoothed vertex positions ===
        std::vector<glm::vec3> smoothedPos(nVerts);
        std::vector<glm::vec2> smoothedUV(nVerts);
        std::vector<glm::vec3> smoothedCol(nVerts);

        for (uint32_t vi = 0; vi < nVerts; vi++) {
            uint32_t n = static_cast<uint32_t>(vertexNeighbors[vi].size());
            if (n == 0) {
                smoothedPos[vi] = mesh.positions[vi];
                smoothedUV[vi] = mesh.texCoords[vi];
                smoothedCol[vi] = mesh.colors[vi];
                continue;
            }

            // Check if boundary vertex
            bool isBoundary = false;
            std::vector<uint32_t> boundaryNeighbors;
            for (uint32_t nb : vertexNeighbors[vi]) {
                Edge e = makeEdge(vi, nb);
                if (edgeToFaces[e].size() == 1) {
                    isBoundary = true;
                    boundaryNeighbors.push_back(nb);
                }
            }

            if (isBoundary && boundaryNeighbors.size() == 2) {
                // Boundary vertex: (left + 6*V + right) / 8
                smoothedPos[vi] = (mesh.positions[boundaryNeighbors[0]] +
                                   6.0f * mesh.positions[vi] +
                                   mesh.positions[boundaryNeighbors[1]]) / 8.0f;
                smoothedUV[vi] = (mesh.texCoords[boundaryNeighbors[0]] +
                                  6.0f * mesh.texCoords[vi] +
                                  mesh.texCoords[boundaryNeighbors[1]]) / 8.0f;
                smoothedCol[vi] = (mesh.colors[boundaryNeighbors[0]] +
                                   6.0f * mesh.colors[vi] +
                                   mesh.colors[boundaryNeighbors[1]]) / 8.0f;
            } else {
                // Interior vertex: Q/n + 2R/n + (n-3)*V/n
                glm::vec3 Q(0); glm::vec2 Quv(0); glm::vec3 Qc(0);
                for (uint32_t fi : vertexToFaces[vi]) {
                    Q += facePoints[fi];
                    Quv += facePointUVs[fi];
                    Qc += facePointColors[fi];
                }
                float nf = float(vertexToFaces[vi].size());
                Q /= nf; Quv /= nf; Qc /= nf;

                glm::vec3 R(0); glm::vec2 Ruv(0); glm::vec3 Rc(0);
                for (uint32_t nb : vertexNeighbors[vi]) {
                    R += (mesh.positions[vi] + mesh.positions[nb]) * 0.5f;
                    Ruv += (mesh.texCoords[vi] + mesh.texCoords[nb]) * 0.5f;
                    Rc += (mesh.colors[vi] + mesh.colors[nb]) * 0.5f;
                }
                float ne = float(n);
                R /= ne; Ruv /= ne; Rc /= ne;

                smoothedPos[vi] = Q / ne + 2.0f * R / ne + (ne - 3.0f) * mesh.positions[vi] / ne;
                smoothedUV[vi] = Quv / ne + 2.0f * Ruv / ne + (ne - 3.0f) * mesh.texCoords[vi] / ne;
                smoothedCol[vi] = Qc / ne + 2.0f * Rc / ne + (ne - 3.0f) * mesh.colors[vi] / ne;
            }
        }

        // === Step 5: Build output mesh ===
        NGonMesh result;

        // Add smoothed original vertices
        for (uint32_t i = 0; i < nVerts; i++) {
            result.positions.push_back(smoothedPos[i]);
            result.normals.push_back(mesh.normals[i]);  // recomputed later
            result.texCoords.push_back(smoothedUV[i]);
            result.colors.push_back(smoothedCol[i]);
        }

        // Add face point vertices
        std::vector<uint32_t> facePointIdx(nFaces);
        for (uint32_t fi = 0; fi < nFaces; fi++) {
            facePointIdx[fi] = static_cast<uint32_t>(result.positions.size());
            result.positions.push_back(facePoints[fi]);
            result.normals.push_back(glm::vec3(0, 1, 0));
            result.texCoords.push_back(facePointUVs[fi]);
            result.colors.push_back(facePointColors[fi]);
        }

        // Add edge point vertices
        std::map<Edge, uint32_t> edgePointIdx;
        for (auto& [edge, pos] : edgePointPos) {
            edgePointIdx[edge] = static_cast<uint32_t>(result.positions.size());
            result.positions.push_back(pos);
            result.normals.push_back(glm::vec3(0, 1, 0));
            result.texCoords.push_back(edgePointUV[edge]);
            result.colors.push_back(edgePointCol[edge]);
        }

        // Create quads
        uint32_t offset = 0;
        for (uint32_t fi = 0; fi < nFaces; fi++) {
            const auto& vi = mesh.faces[fi].vertexIndices;
            uint32_t n = mesh.faces[fi].count;
            uint32_t fp = facePointIdx[fi];

            for (uint32_t i = 0; i < n; i++) {
                uint32_t corner = vi[i];
                uint32_t ep_next = edgePointIdx[makeEdge(vi[i], vi[(i + 1) % n])];
                uint32_t ep_prev = edgePointIdx[makeEdge(vi[(i + n - 1) % n], vi[i])];

                NGonFace quad;
                quad.vertexIndices = { corner, ep_next, fp, ep_prev };
                quad.count = 4;
                quad.offset = offset;
                quad.normal = glm::vec4(computeFaceNormal(result.positions, quad.vertexIndices), 0.0f);
                quad.center = glm::vec4(computeFaceCentroid(result.positions, quad.vertexIndices), 1.0f);
                quad.area   = computeFaceArea(result.positions, quad.vertexIndices);

                for (uint32_t idx : quad.vertexIndices)
                    result.faceVertexIndices.push_back(idx);
                offset += 4;

                result.faces.push_back(std::move(quad));
            }
        }

        // Recompute vertex normals from face normals
        uint32_t totalVerts = static_cast<uint32_t>(result.positions.size());
        std::vector<glm::vec3> normalAccum(totalVerts, glm::vec3(0));
        for (const auto& face : result.faces) {
            glm::vec3 fn = glm::vec3(face.normal);
            for (uint32_t idx : face.vertexIndices) {
                normalAccum[idx] += fn;
            }
        }
        for (uint32_t i = 0; i < totalVerts; i++) {
            float len = glm::length(normalAccum[i]);
            result.normals[i] = (len > 0.0001f) ? normalAccum[i] / len : glm::vec3(0, 1, 0);
        }

        result.nbVertices = totalVerts;
        result.nbFaces    = static_cast<uint32_t>(result.faces.size());

        mesh = std::move(result);

        std::cout << "Catmull-Clark subdivided (level " << (lvl + 1) << "): "
                  << mesh.nbFaces << " faces, "
                  << mesh.nbVertices << " vertices" << std::endl;
    }
}

// *** ************ ***
