#include "levels/BigVistaLevel.h"

#include "Renderer.h"
#include "primitives.h"
#include "HudWidgets.h"
#include "imgui.h"
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_set>

namespace {
    // Valley -> peak height tint. Low ground reads as green meadow; high ground
    // pales toward a grey/snow massif so elevation is legible at a glance.
    const glm::vec3 VALLEY {0.24f, 0.42f, 0.26f};
    const glm::vec3 PEAK   {0.78f, 0.80f, 0.84f};

    // The summit orb: a warm beacon, distinct from all the cool greens/greys, so
    // once the player turns toward the mountain it is unmistakable even far off.
    const glm::vec3 ORB_COL {1.00f, 0.78f, 0.18f};

    // Horizontal distance in the ground 3-volume X-Z-W (Y is eye height, so it must
    // not count against "near"). Same convention as ForestFetchLevel.
    float hdist(const glm::vec4& a, const glm::vec4& b) {
        float dx = a.x - b.x, dz = a.z - b.z, dw = a.w - b.w;
        return std::sqrt(dx * dx + dz * dz + dw * dw);
    }
}

// --- Height field ----------------------------------------------------------
// Rolling base: a few sines with non-harmonic frequencies mixing all three
// ground axes (x, z AND w) so the terrain genuinely undulates in 4D, with no
// lattice seams. Normalised so the swing is ~+/- HILL_AMP.
float BigVistaLevel::baseHills(float x, float z, float w) {
    // Frequencies kept low so the field is well sampled by the coarse, big-tile
    // grid (frequency * tile-pitch stays below ~1, so no aliasing between columns).
    float h = 0.0f;
    h += 1.00f * std::sin(0.040f * x + 0.7f) * std::cos(0.035f * z);
    h += 0.55f * std::sin(0.060f * z - 1.3f) * std::cos(0.050f * w + 2.1f);
    h += 0.30f * std::sin(0.045f * (x + w) + 0.4f) * std::cos(0.040f * z);
    return (h / 1.85f) * HILL_AMP;   // the three terms peak near 1.85
}

// Mountain: one smooth raised-cosine dome centred far away. Reaches exactly zero
// at the foot (r == MTN_R) so it blends cleanly into the hills, with no tail.
float BigVistaLevel::mountainBump(float x, float z, float w) {
    float dx = x - MTN_X, dz = z - MTN_Z, dw = w - MTN_W;
    float r = std::sqrt(dx * dx + dz * dz + dw * dw);
    if (r >= MTN_R) return 0.0f;
    float t = r / MTN_R;                                   // 0 at peak, 1 at foot
    return MTN_H * 0.5f * (1.0f + std::cos(3.14159265f * t));
}

float BigVistaLevel::terrainHeight(float x, float z, float w) {
    return SURFACE_Y + baseHills(x, z, w) + mountainBump(x, z, w);
}

// --- Terrain mesh ----------------------------------------------------------
// A genuine tetrahedral height field: grid nodes over the ground 3-volume X-Z-W,
// each lifted to Y=terrainHeight(); every cubic cell split into 6 tets (Kuhn,
// reusing kBoxKuhn from primitives.h). Because neighbouring nodes share their
// (single) height, the surface slopes continuously between samples - no boxes,
// no stairs - and the whole thing is one mesh / one draw call.
Object4D BigVistaLevel::buildTerrainMesh() {
    Object4D m;
    m.name     = "Vista Terrain";
    m.occludes = false;

    const int   N  = NG;                       // cells per axis
    const int   P  = N + 1;                     // nodes per axis
    const float sp = 2.0f * EXTENT / (float)N;  // node spacing

    const float low  = SURFACE_Y - HILL_AMP;
    const float high = SURFACE_Y + HILL_AMP + MTN_H;
    auto vid = [P](int i, int j, int k) { return (i * P + j) * P + k; };

    // Nodes, lifted to height and tinted valley->peak by elevation.
    m.vertices.reserve((size_t)P * P * P);
    m.vertexColors.reserve((size_t)P * P * P);
    for (int i = 0; i < P; ++i)
    for (int j = 0; j < P; ++j)
    for (int k = 0; k < P; ++k) {
        float x = -EXTENT + i * sp;
        float z = -EXTENT + j * sp;
        float w = -EXTENT + k * sp;
        float y = terrainHeight(x, z, w);
        m.vertices.push_back(glm::vec4(x, y, z, w));
        float t = glm::clamp((y - low) / (high - low), 0.0f, 1.0f);
        m.vertexColors.push_back(glm::mix(VALLEY, PEAK, t));
    }

    // Cube corner code -> node offset (bit0=i, bit1=j, bit2=k), matching kBoxKuhn.
    auto corner = [&](int i, int j, int k, int code) {
        return vid(i + (code & 1), j + ((code >> 1) & 1), k + ((code >> 2) & 1));
    };
    static const int faceCombo[4][3] = {{0, 1, 2}, {0, 1, 3}, {0, 2, 3}, {1, 2, 3}};

    // Each interior 2-face is shared by two tets; one copy suffices for an opaque
    // painter's render. Dedup by the sorted vertex triple (verts < 2^21).
    std::unordered_set<uint64_t> seen;
    auto faceKey = [](unsigned a, unsigned b, unsigned c) {
        unsigned lo = std::min({a, b, c}), hi = std::max({a, b, c});
        unsigned mid = a + b + c - lo - hi;
        return ((uint64_t)lo << 42) | ((uint64_t)mid << 21) | (uint64_t)hi;
    };

    for (int i = 0; i < N; ++i)
    for (int j = 0; j < N; ++j)
    for (int k = 0; k < N; ++k)
        for (int t = 0; t < 6; ++t) {
            int v[4];
            for (int c = 0; c < 4; ++c) v[c] = corner(i, j, k, kBoxKuhn[t][c]);
            m.tetrahedra.push_back(glm::ivec4(v[0], v[1], v[2], v[3]));
            for (int f = 0; f < 4; ++f) {
                unsigned a = v[faceCombo[f][0]], b = v[faceCombo[f][1]], c2 = v[faceCombo[f][2]];
                if (seen.insert(faceKey(a, b, c2)).second) {
                    m.triangleIndices.push_back(a);
                    m.triangleIndices.push_back(b);
                    m.triangleIndices.push_back(c2);
                }
            }
        }
    return m;
}

BigVistaLevel::BigVistaLevel() {
    // Spawn at the origin, standing on the terrain, facing -W (engine default).
    // The mountain is off toward +X/+Z, so it is well off the forward axis: the
    // player must turn to find it. Full, unrestricted first-person controls.
    float h0 = terrainHeight(0.0f, 0.0f, 0.0f);
    cam4D_.pos   = glm::vec4(0.0f, h0 + EYE_HEIGHT, 0.0f, 0.0f);
    cam4D_.speed = SPEED;
    body_.radius = EYE_HEIGHT;

    controls_.lockedAxes = AxisLock::NONE;
    controls_.headReturn = false;
}

BigVistaLevel::~BigVistaLevel() {
    terrainBuf_.destroy();
    sphereBuf_.destroy();
}

void BigVistaLevel::load() {
    // --- Terrain: one tetrahedral height-field mesh (smooth slopes, one draw
    // call). It is the opaque floor we look ACROSS, not through, so skip the 4D
    // hidden-surface pass for it - that per-fragment ray-occlusion is the dominant
    // cost and is unnecessary here. Standard painter's order handles the rest. ---
    terrainMesh_ = buildTerrainMesh();
    terrainMesh_.occludes = false;
    terrainBuf_.init(terrainMesh_);
    // A single instance at the origin; colour comes from the mesh's per-vertex
    // height tint, so the instance colours are unused (set to white).
    terrainInsts_.push_back({glm::vec4(0.0f), Math4D::Rotor4D::identity(),
                             glm::vec3(1.0f), glm::vec3(1.0f)});

    // --- Summit orb: a small hypersphere resting on the mountain top. ---
    sphereMesh_ = generateHypersphere(1.2f);
    sphereBuf_.init(sphereMesh_);
    float hPeak = terrainHeight(MTN_X, MTN_Z, MTN_W);
    spherePos_  = glm::vec4(MTN_X, hPeak + 1.5f, MTN_Z, MTN_W);

    // Note: no ground colliders. Footing is analytic terrain-following in update().
    loaded_ = true;
}

void BigVistaLevel::update(const LevelContext& ctx) {
    // Horizontal movement / turning. With no colliders registered, nothing blocks
    // lateral motion and gravity just drops Y (which we overwrite below).
    runCameraInput(ctx);

    if (ctx.insideMode) {
        // Keep the player on the map (cheap bounds clamp, no colliders needed).
        glm::vec4 p = cam4D_.pos;
        p.x = glm::clamp(p.x, -EXTENT, EXTENT);
        p.z = glm::clamp(p.z, -EXTENT, EXTENT);
        p.w = glm::clamp(p.w, -EXTENT, EXTENT);

        // Analytic terrain-following: stand exactly EYE_HEIGHT above the surface.
        // This walks every slope - the mountain too - smoothly, with no MTV
        // step-up limit. Gravity is neutralised so it can't accumulate.
        p.y = terrainHeight(p.x, p.z, p.w) + EYE_HEIGHT;
        cam4D_.pos  = p;
        body_.pos   = p;
        body_.velY  = 0.0f;

        nearSphere_ = hdist(p, spherePos_) < INTERACT_DIST;

        // Edge-detected Space activates the orb (and wins) when close enough.
        bool space   = glfwGetKey(ctx.window, GLFW_KEY_SPACE) == GLFW_PRESS;
        bool pressed = space && !spaceWasDown_;
        spaceWasDown_ = space;
        if (pressed && nearSphere_)
            activated_ = true;
    } else {
        nearSphere_ = false;
    }
}

void BigVistaLevel::render(const LevelContext& ctx) {
    const Math4D::Rotor4D ori = cam4D_.getOrientation();

    // Big scene: relax the fog and push the far plane so the distant mountain and
    // its orb stay readable (cf. ForestFetchLevel / PlaneLevel4D).
    RenderSettings vis = ctx.vis;
    vis.depthFar    = std::max(vis.depthFar, 260.0f);
    vis.fogStrength = std::min(vis.fogStrength, 0.22f);

    auto draw = [&](const std::vector<ObjectInstance>& insts, const Object4D& mesh, ObjectBuffer& buf) {
        if (!insts.empty())
            ctx.renderer.drawObjects(insts, mesh, buf, cam4D_, ori, focal_, ctx.innerMVP, vis);
    };

    draw(terrainInsts_, terrainMesh_, terrainBuf_);

    glm::vec3 orb = activated_ ? glm::vec3(0.4f, 1.0f, 0.5f) : ORB_COL;   // greens out once activated
    std::vector<ObjectInstance> orbInst = {{spherePos_, Math4D::Rotor4D::identity(), orb, orb * 1.3f}};
    draw(orbInst, sphereMesh_, sphereBuf_);

    ctx.renderer.drawOuterCube(ctx.outerMVP);
}

void BigVistaLevel::renderHUD(const LevelContext& ctx) {
    ImGui::SetNextWindowPos(ImVec2(140, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(470, 210), ImGuiCond_Always);
    ImGui::Begin("Big Vista", nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);

    ImGui::TextWrapped("Big Vista - a wide 4D landscape of rolling hills across the X-Z-W ground.");
    if (!ctx.insideMode) {
        ImGui::TextWrapped("Press TAB to step inside the 4D view.");
    } else {
        ImGui::TextWrapped("Move: E/A (forward/back), Q/D (strafe), W/S (along W). "
                           "Turn: J/O and U/L. Look: I/K.");
        ImGui::Separator();
        if (!activated_) {
            ImGui::TextWrapped("Somewhere far off a mountain rises, with a glowing orb at its peak. "
                               "It is not straight ahead - turn and look for it, then climb to the summit.");
        }
    }

    if (!activated_ && ctx.insideMode && nearSphere_)
        ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.3f, 1.0f), "Press SPACE to touch the orb.");
    if (activated_)
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "You reached the summit - level complete!");
    ImGui::End();

    glm::vec4 fw = glm::inverse(cam4D_.getOrientation().toMatrix()) * glm::vec4(0.0f, 0.0f, 0.0f, -1.0f);
    hud::drawFacingWidget(fw);
    hud::drawCoordWidget(cam4D_.pos, "You");
}
