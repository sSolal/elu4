#include "Asset.h"

#include <cstdio>
#include <fstream>
#include <sstream>

#include "primitives.h"
#include "third_party/json.hpp"

using nlohmann::json;

namespace {

// Directory portion of a path (everything up to and including the last '/'), so
// a part's "ref" resolves relative to the asset file that named it. "" if none.
std::string dirOf(const std::string& path) {
    size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? std::string() : path.substr(0, slash + 1);
}

glm::vec4 readVec4(const json& j, const glm::vec4& fallback) {
    if (!j.is_array() || j.size() < 4) return fallback;
    return glm::vec4(j[0].get<float>(), j[1].get<float>(),
                     j[2].get<float>(), j[3].get<float>());
}

glm::vec3 readVec3(const json& j, const glm::vec3& fallback) {
    if (!j.is_array() || j.size() < 3) return fallback;
    return glm::vec3(j[0].get<float>(), j[1].get<float>(), j[2].get<float>());
}

// A rotor authored as per-plane angles (radians), composed in a fixed order. Most
// assets are axis-aligned (rot absent/null → identity); this is here for the rare
// part that needs a tilt without re-baking its vertices.
Math4D::Rotor4D readRotor(const json& j) {
    using R = Math4D::Rotor4D;
    if (!j.is_object()) return R::identity();
    R r = R::identity();
    auto ang = [&](const char* k) { return j.contains(k) ? j[k].get<float>() : 0.0f; };
    r = R::fromXY(ang("xy")) * r;
    r = R::fromXZ(ang("xz")) * r;
    r = R::fromXW(ang("xw")) * r;
    r = R::fromYZ(ang("yz")) * r;
    r = R::fromYW(ang("yw")) * r;
    r = R::fromZW(ang("zw")) * r;
    return r;
}

// Build an Object4D from explicit vertices/edges/cells (same fan-triangulation as
// Object4D::loadFromJSON, kept non-occluding like every authored mesh).
Object4D meshFromArrays(const json& spec) {
    Object4D m;
    m.name = spec.value("name", std::string("Mesh"));
    if (spec.contains("vertices"))
        for (const auto& v : spec["vertices"]) m.vertices.push_back(readVec4(v, glm::vec4(0.0f)));
    if (spec.contains("edges"))
        for (const auto& e : spec["edges"])
            if (e.is_array() && e.size() >= 2) m.edges.push_back({e[0].get<int>(), e[1].get<int>()});
    if (spec.contains("cells"))
        for (const auto& c : spec["cells"]) {
            std::vector<int> cell;
            for (const auto& idx : c) cell.push_back(idx.get<int>());
            if (!cell.empty()) m.cells.push_back(std::move(cell));
        }
    for (const auto& cell : m.cells)
        for (size_t k = 1; k + 1 < cell.size(); ++k) {
            m.triangleIndices.push_back((unsigned)cell[0]);
            m.triangleIndices.push_back((unsigned)cell[k]);
            m.triangleIndices.push_back((unsigned)cell[k + 1]);
        }
    return m;
}

// Resolve one part's geometry (the local-space Object4D) from its "shape".
bool buildPartMesh(const json& part, const std::string& baseDir, Object4D& out) {
    const std::string shape = part.value("shape", std::string("box"));
    if (shape == "box") {
        out = generateBox(readVec4(part.value("half", json()), glm::vec4(0.5f)));
    } else if (shape == "ground") {
        out = generateGround(readVec4(part.value("half", json()), glm::vec4(0.5f)));
    } else if (shape == "hypercube") {
        out = generateHypercube();
    } else if (shape == "hypersphere") {
        out = generateHypersphere(part.value("radius", 0.5f));
    } else if (shape == "mesh") {
        if (part.contains("ref")) {
            if (!out.loadFromJSON(baseDir + part["ref"].get<std::string>())) return false;
        } else {
            out = meshFromArrays(part);
        }
    } else {
        std::fprintf(stderr, "loadAsset: unknown shape \"%s\"\n", shape.c_str());
        return false;
    }
    // Optional uniform scale (e.g. the gold cube = hypercube * 0.6).
    if (part.contains("scale")) {
        float f = part["scale"].get<float>();
        for (auto& v : out.vertices) v *= f;
        for (auto& d : out.hullD)    d *= f;
    }
    return true;
}

}  // namespace

bool loadAsset(const std::string& path, Asset& out) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::fprintf(stderr, "loadAsset: cannot open %s\n", path.c_str());
        return false;
    }
    std::stringstream ss;
    ss << file.rdbuf();

    json root;
    try {
        root = json::parse(ss.str(), /*cb=*/nullptr, /*allow_exceptions=*/true,
                           /*ignore_comments=*/true);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "loadAsset: JSON parse error in %s: %s\n", path.c_str(), e.what());
        return false;
    }

    out = Asset{};
    out.name = root.value("name", std::string("Asset"));
    const std::string baseDir = dirOf(path);

    // Back-compat: a bare single-mesh file (top-level geometry, no "parts").
    if (!root.contains("parts")) {
        AssetPart p;
        p.mesh = meshFromArrays(root);
        if (p.mesh.vertices.empty()) {
            std::fprintf(stderr, "loadAsset: %s has no parts and no vertices\n", path.c_str());
            return false;
        }
        out.parts.push_back(std::move(p));
        return true;
    }

    for (const auto& part : root["parts"]) {
        AssetPart p;
        if (!buildPartMesh(part, baseDir, p.mesh)) return false;
        p.pos    = readVec4(part.value("pos", json()), glm::vec4(0.0f));
        if (part.contains("rot") && part["rot"].is_object()) p.rot = readRotor(part["rot"]);
        p.colorA = readVec3(part.value("colorA", json()), glm::vec3(0.8f));
        p.colorB = part.contains("colorB") ? readVec3(part["colorB"], p.colorA) : p.colorA;
        p.solid  = part.value("solid", false);
        out.parts.push_back(std::move(p));
    }
    return true;
}
