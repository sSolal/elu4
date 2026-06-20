#pragma once

#include <vector>
#include "Level.h"
#include "Object4D.h"
#include "ObjectBuffer.h"
#include "Renderer.h"   // ObjectInstance

// Level 10 - Big Vista.
//
// The capstone perception level: one large 4D landscape with rolling ups and
// downs across the whole X-Z-W ground volume. Somewhere far off - and NOT along
// the player's default -W forward, so it takes some turning to spot - a single
// mountain rises, with a glowing orb resting on its summit. Walk the terrain,
// find the mountain, climb it, and activate the orb.
//
// Terrain is an analytic height field h(x,z,w): the visible ground is a coarse
// grid of instanced boxes ("columns") whose tops trace the surface, but the
// player's footing is computed straight from h() (analytic terrain-following)
// rather than from per-column colliders. That makes every slope - the mountain
// included - smoothly walkable, sidesteps the cube-collider step-up limit, and
// keeps the level cheap (one mesh, ~2k instances, no thousands of colliders).
class BigVistaLevel : public Level {
public:
    BigVistaLevel();
    ~BigVistaLevel() override;

    const char* name() const override { return "10 - Big Vista"; }

    void load() override;
    void update(const LevelContext& ctx) override;
    void render(const LevelContext& ctx) override;
    bool checkWin() const override { return activated_; }
    void renderHUD(const LevelContext& ctx) override;

private:
    // --- The terrain height field (deterministic, fully 4D). ---
    static float baseHills(float x, float z, float w);
    static float mountainBump(float x, float z, float w);
    static float terrainHeight(float x, float z, float w);

    // Build the terrain as ONE tetrahedral mesh: a grid of nodes over the ground
    // 3-volume X-Z-W, each lifted to its height Y=terrainHeight(), with every
    // cubic cell split into 6 tetrahedra (Kuhn). Neighbouring nodes share heights,
    // so the surface is continuously sloped (no stair-steps), and it is a single
    // draw call. Vertices are tinted by height (valley green -> snow peak).
    static Object4D buildTerrainMesh();

    // --- Meshes + GPU buffers ---
    Object4D     terrainMesh_, sphereMesh_;
    ObjectBuffer terrainBuf_,  sphereBuf_;

    // --- Static scene instances ---
    std::vector<ObjectInstance> terrainInsts_;

    // --- Summit orb / interaction state ---
    glm::vec4 spherePos_{0.0f};
    bool      nearSphere_   = false;
    bool      activated_    = false;
    bool      spaceWasDown_ = false;

    // --- Tunables ---
    static constexpr float SURFACE_Y  = 0.0f;   // nominal sea level
    static constexpr float EYE_HEIGHT = 1.6f;   // player eye height above the ground
    static constexpr float SPEED      = 6.0f;   // brisk; the space is big

    // The terrain is one tetrahedral height-field mesh (see buildTerrainMesh):
    // an (NG+1)^3 node grid over the ground 3-volume X-Z-W, each cubic cell split
    // into 6 tets, all in a single draw call. Cost scales as NG^3 but is trivial
    // here (~3 ms at NG=16, ~360 FPS uncapped) since it is one mesh, not thousands.
    static constexpr int   NG     = 16;   // grid cells per axis; nodes = NG+1 (17^3 = 4913 verts)
    static constexpr float EXTENT = 96.0f; // half map size (X/Z/W span [-EXTENT, EXTENT])
    // node spacing = 2*EXTENT/NG = 12; cells = NG^3 = 4096, tets = 4096*6 = 24576.
    // Cost is tiny (~1-2 ms); raise NG for a finer surface, lower it for chunkier.

    static constexpr float HILL_AMP   = 3.0f;   // rolling-hill amplitude (+/-)

    // Mountain: far, dominant +X/+Z, only mild -W (so it's off the forward axis).
    static constexpr float MTN_X = 60.0f;
    static constexpr float MTN_Z = 52.0f;
    static constexpr float MTN_W = -20.0f;
    static constexpr float MTN_H = 28.0f;       // peak height above sea level
    static constexpr float MTN_R = 50.0f;       // foot radius (raised-cosine falloff)

    static constexpr float INTERACT_DIST = 4.5f;  // SPACE range at the summit orb
};
