#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "Object4D.h"
#include "Math4D.h"

// An asset is a named collection of parts in the asset's LOCAL space. Each part is
// either a procedural primitive (box/ground/hypercube/hypersphere) or an explicit
// vertex-defined mesh (inline or referenced from another file). A part maps 1:1 onto
// the game's (mesh, single-instance) pair, so an asset reproduces exactly what the
// scene scripts used to build inline. The same loader feeds the game (single source
// of truth for its props) and the standalone visualizer.
//
// JSON schema (see assets/*.json):
//   { "name": "Bed",
//     "parts": [ { "shape": "box", "half": [hx,hy,hz,hw], "pos": [x,y,z,w],
//                  "rot": {"xy":..,"zw":..} | null,
//                  "colorA": [r,g,b], "colorB": [r,g,b], "scale": 1.0, "solid": false },
//                { "shape": "mesh", "ref": "tree.json", ... },
//                { "shape": "hypersphere", "radius": 0.5, ... } ] }
//
// Back-compat: a file with top-level "vertices"/"edges"/"cells" and no "parts"
// (today's tree.json / hypercube.json) loads as a single mesh part.

struct AssetPart {
    Object4D        mesh;                                    // resolved geometry (local)
    glm::vec4       pos{0.0f};                               // local offset within the asset
    Math4D::Rotor4D rot = Math4D::Rotor4D::identity();       // local orientation
    glm::vec3       colorA{0.8f, 0.8f, 0.8f};
    glm::vec3       colorB{0.8f, 0.8f, 0.8f};
    bool            solid = false;                           // register a collider when placed
};

struct Asset {
    std::string            name;
    std::vector<AssetPart> parts;
};

// Load an asset from a JSON file. Returns false (and logs to stderr) on failure.
bool loadAsset(const std::string& path, Asset& out);
