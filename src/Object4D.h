#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <utility>
#include <iostream>

// A 4D object as a tetrahedral surface mesh. Mirroring 3D exactly: in 3D you store
// vertices + triangles, each triangle carrying a normal; in 4D you store vertices +
// TETRAHEDRA (the unit brick of a 4D object's boundary 3-manifold), each tetrahedron
// carrying a 4D normal — because a tet is flat (a 3-flat) in 4D, so it must say which
// way it faces. Occlusion happens at the tetrahedron level (see Renderer/inner.frag):
// a tet is drawn when it faces the 4D eye, and nearer surface hides farther surface
// along the same 4D line of sight.
struct Object4D {
    std::string name;
    std::vector<glm::vec4> vertices;
    std::vector<std::pair<int, int>> edges;             // wireframe 1-skeleton
    std::vector<std::vector<int>> cells;                // legacy 2-faces (quads): JSON / Visualizer

    // --- Tetrahedral surface (canonical geometry) ---------------------------------
    std::vector<glm::ivec4> tetrahedra;                 // boundary 3-cells (4 vertex idx)
    std::vector<glm::vec4>  tetNormal;                  // per-tet 4D outward unit normal
    std::vector<int>        tetFace;                    // per-tet facet index -> hull plane

    // --- Rasterised surface (the visible 2-skeleton) ------------------------------
    // triangleIndices is grouped in 3s; triFace has ONE entry per triangle giving the
    // (up to) two facets the triangle bounds — a triangle is drawn when EITHER facet
    // faces the 4D eye (.y == -1 when the triangle bounds only one facet).
    std::vector<unsigned int> triangleIndices;
    std::vector<glm::ivec2>   triFace;

    // --- 4D occlusion -------------------------------------------------------------
    // The object is the convex solid { x : dot(hullN[i], x) <= hullD[i] } (outward
    // unit normals, local space). The renderer feeds these hyperplanes to the shader
    // so surfaces genuinely behind the solid in 4D are hidden. occludes=false meshes
    // (polylines) neither occlude nor self-cull.
    bool occludes = false;
    std::vector<glm::vec4> hullN;                       // local outward unit normals
    std::vector<float>     hullD;                       // local offsets

    Object4D() = default;
    Object4D(const std::string& filePath);

    bool loadFromJSON(const std::string& filePath);
    bool saveToJSON(const std::string& filePath) const;
};
