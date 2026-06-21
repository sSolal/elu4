#include "levels/ForestFetchLevel.h"

#include "Renderer.h"
#include "primitives.h"
#include "mesh_merge.h"
#include "HudWidgets.h"
#include "imgui.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace {
    // Ground: a calm two-green checkerboard so the trees and the golden cube read
    // clearly against it.
    const glm::vec3 GROUND_A {0.30f, 0.40f, 0.30f};
    const glm::vec3 GROUND_B {0.22f, 0.33f, 0.24f};
    // Ordinary trees: a green W-gradient (low-W darker, high-W lighter) so each
    // tree's W-extent is legible as depth.
    const glm::vec3 TREE_LO {0.10f, 0.30f, 0.14f};
    const glm::vec3 TREE_HI {0.22f, 0.50f, 0.26f};
    // The NPC marker: a bright cyan pillar that stands out from the foliage.
    const glm::vec3 NPC_COL {0.30f, 0.90f, 0.95f};
    // The fetch objective.
    const glm::vec3 GOLD_COL {1.00f, 0.82f, 0.10f};

    // Horizontal distance: the floor is the 3-volume X-Z-W, so proximity ignores Y
    // (the player's eye height above the ground must not count against "near").
    float hdist(const glm::vec4& a, const glm::vec4& b) {
        float dx = a.x - b.x, dz = a.z - b.z, dw = a.w - b.w;
        return std::sqrt(dx * dx + dz * dz + dw * dw);
    }

    // A scaled copy of a mesh: uniformly scale every vertex (all four axes) so the
    // object stays a consistent 4D shape, just larger. Used to grow the JSON tree
    // into forest-sized trees and taller landmark trees.
    Object4D scaledMesh(const Object4D& src, float f) {
        Object4D out = src;
        for (auto& v : out.vertices) v *= f;
        for (auto& d : out.hullD) d *= f;  // scale occluder facet offsets with the mesh
        return out;
    }
}

ForestFetchLevel::ForestFetchLevel() {
    // Spawn at the NPC hub, eye at standing height. Full, unrestricted first-person
    // controls: this is a navigation test, so no axis locks and no head-return.
    cam4D_.pos   = glm::vec4(0.0f, EYE_HEIGHT, 0.0f, 0.0f);
    cam4D_.speed = 4.5f;                 // roomy forest, so move briskly
    body_.radius = EYE_HEIGHT;

    controls_.lockedAxes  = AxisLock::NONE;
    controls_.headReturn  = false;
}

ForestFetchLevel::~ForestFetchLevel() {
    treeBuf_.destroy();
    landmarkBuf_.destroy();
    groundBuf_.destroy();
    npcBuf_.destroy();
    goldBuf_.destroy();
}

void ForestFetchLevel::load() {
    // --- Tree asset (reused from the project's objects). Grown to forest scale, and
    // a taller variant for the named landmark trees. The forest trees are merged
    // into one mesh below (decorative scenery, looked across), so keep the scaled
    // single-tree as a local base. The landmark trees stay as their own instances. ---
    Object4D rawTree;
    rawTree.loadFromJSON("objects/tree.json");
    Object4D treeBase = scaledMesh(rawTree, TREE_SCALE);
    landmarkMesh_ = scaledMesh(rawTree, LM_SCALE);
    landmarkBuf_.init(landmarkMesh_);

    // --- Ground base tile: thin slab in Y, occludes=false (a floor is looked
    // ACROSS, not through). Merged into one mesh below. ---
    Object4D groundTile = generateGround(glm::vec4(TILE, TILE_Y, TILE, TILE));

    // --- NPC marker: a tall cyan pillar; gold cube: a small bright hypercube. ---
    npcMesh_ = generateBox(glm::vec4(0.6f, 1.2f, 0.6f, 0.6f));
    npcBuf_.init(npcMesh_);
    goldMesh_ = scaledMesh(generateHypercube(), 0.6f);
    goldBuf_.init(goldMesh_);

    // --- Quest landmarks (named, distinctly coloured, taller than the forest). The
    // directions in the HUD refer to these so the player navigates by them. ---
    npcPos_   = glm::vec4(0.0f, 1.2f, 0.0f, 0.0f);
    goldPos_  = glm::vec4(2.0f, 1.4f, -3.0f, 24.0f);   // deep toward +W, past the Red Pine
    finalPos_ = glm::vec4(-24.0f, SURFACE_Y, 6.0f, -3.0f); // toward -X, past the Blue Tree

    landmarks_ = {
        { glm::vec4(0.0f,   LM_SCALE,  0.0f,  16.0f), "Tall Red Pine",    {0.85f, 0.18f, 0.15f} },
        { glm::vec4(-16.0f, LM_SCALE,  5.0f,   0.0f), "Crooked Blue Tree",{0.20f, 0.40f, 0.95f} },
        { glm::vec4(13.0f,  LM_SCALE, -13.0f, -9.0f), "Amber Willow",     {0.90f, 0.65f, 0.12f} },
    };
    for (const auto& lm : landmarks_)
        landmarkInsts_.push_back({lm.pos, Math4D::Rotor4D::identity(), lm.color, lm.color * 1.3f});

    // --- Ground tiles. The floor is a 3-volume: tiles tile X, Z and W at the
    // surface. They are baked into ONE merged mesh (one project/upload/sort/draw,
    // no per-fragment occlusion) instead of (2*NT+1)^3 separate occluding instances,
    // and a SINGLE flat collider replaces the per-tile collider grid. ---
    const float visualY = SURFACE_Y - TILE_Y;  // slab top at SURFACE_Y
    const Math4D::Rotor4D I = Math4D::Rotor4D::identity();
    std::vector<ObjectInstance> tiles;
    for (int ix = -NT; ix <= NT; ++ix)
    for (int iz = -NT; iz <= NT; ++iz)
    for (int iw = -NT; iw <= NT; ++iw) {
        float x = ix * STEP, z = iz * STEP, w = iw * STEP;
        int parity = (ix + iz + iw) & 1;
        glm::vec3 shade = parity ? GROUND_A : GROUND_B;
        tiles.push_back({glm::vec4(x, visualY, z, w), I, shade, shade});
    }
    groundMesh_  = mergeInstances(groundTile, tiles);
    groundBuf_.init(groundMesh_);
    groundInsts_ = mergedInstance();
    world_.addFlatGround(SURFACE_Y, NT * STEP + TILE);   // ground extent in X/Z/W

    // --- Scatter ordinary trees through the X-Z-W volume, kept out of the hub and
    // the gold/final clearings so the quest points stay reachable and readable. ---
    const float REGION = NT * STEP + TILE - 2.0f;  // a little inside the ground edge
    unsigned seed = 1337u;
    auto rnd = [&]() {
        seed = seed * 1664525u + 1013904223u;
        return (float)((seed >> 8) & 0xFFFFFF) / (float)0x1000000;
    };
    auto blocked = [&](const glm::vec4& p) {
        if (hdist(p, npcPos_)   < CLEARING_R) return true;
        if (hdist(p, goldPos_)  < CLEARING_R) return true;
        if (hdist(p, finalPos_) < CLEARING_R) return true;
        for (const auto& lm : landmarks_)
            if (hdist(p, lm.pos) < CLEARING_R) return true;
        return false;
    };
    int placed = 0, guard = 0;
    while (placed < TREE_N && guard++ < TREE_N * 20) {
        glm::vec4 p(-REGION + 2.0f * REGION * rnd(),
                    TREE_SCALE,                       // base on the ground
                    -REGION + 2.0f * REGION * rnd(),
                    -REGION + 2.0f * REGION * rnd());
        if (blocked(p)) continue;
        treeInsts_.push_back({p, Math4D::Rotor4D::identity(), TREE_LO, TREE_HI});
        ++placed;
    }

    // Bake the scattered forest into one mesh: TREE_N separate projects+uploads+
    // draws collapse to a single one (decorative, non-occluding).
    treeMesh_ = mergeInstances(treeBase, treeInsts_);
    treeBuf_.init(treeMesh_);
    treeInsts_ = mergedInstance();

    loaded_ = true;
}

void ForestFetchLevel::update(const LevelContext& ctx) {
    runCameraInput(ctx);

    const glm::vec4& p = cam4D_.pos;
    nearNpc_  = ctx.insideMode && hdist(p, npcPos_)  < INTERACT_DIST;
    nearGold_ = ctx.insideMode && !held_ && quest_ == Q::Fetching
                                 && hdist(p, goldPos_) < INTERACT_DIST;
    atFinal_  = ctx.insideMode && quest_ == Q::Directed
                                 && hdist(p, finalPos_) < ARRIVE_DIST;

    // Edge-detected Space drives the quest state machine.
    bool space = glfwGetKey(ctx.window, GLFW_KEY_SPACE) == GLFW_PRESS;
    bool pressed = space && !spaceWasDown_;
    spaceWasDown_ = space;

    if (ctx.insideMode && pressed) {
        switch (quest_) {
            case Q::Ask:       if (nearNpc_)  quest_ = Q::Fetching;                 break;
            case Q::Fetching:  if (nearGold_) { held_ = true;  quest_ = Q::Returning; } break;
            case Q::Returning: if (nearNpc_)  { held_ = false; quest_ = Q::Directed;  } break;
            default: break;
        }
    }
    // Final leg completes automatically on arrival at the clearing.
    if (quest_ == Q::Directed && atFinal_) {
        quest_ = Q::Done;
        won_   = true;
    }
}

void ForestFetchLevel::render(const LevelContext& ctx) {
    const Math4D::Rotor4D ori = cam4D_.getOrientation();

    // Large scene: relax the fog so distant landmarks stay readable (cf. PlaneLevel4D).
    RenderSettings vis = largeScene(ctx.vis, 120.0f, 0.30f);

    auto draw = [&](const std::vector<ObjectInstance>& insts, const Object4D& mesh, ObjectBuffer& buf) {
        if (!insts.empty())
            ctx.renderer.drawObjects(insts, mesh, buf, cam4D_, ori, focal_, ctx.innerMVP, vis);
    };

    draw(groundInsts_,   groundMesh_,   groundBuf_);
    draw(treeInsts_,     treeMesh_,     treeBuf_);
    draw(landmarkInsts_, landmarkMesh_, landmarkBuf_);

    std::vector<ObjectInstance> npcInst = {{npcPos_, Math4D::Rotor4D::identity(), NPC_COL, NPC_COL * 0.7f}};
    draw(npcInst, npcMesh_, npcBuf_);

    // The golden cube: in hand while carried, at its hiding spot before pickup, or
    // resting by the NPC after delivery.
    glm::vec4 goldAt;
    bool showGold = true;
    if (held_) {
        glm::vec4 fw = glm::inverse(ori.toMatrix()) * glm::vec4(0.0f, 0.0f, 0.0f, -1.0f);
        goldAt = cam4D_.pos + fw * HOLD_DIST + glm::vec4(0.0f, -0.4f, 0.0f, 0.0f);
    } else if (quest_ == Q::Ask || quest_ == Q::Fetching) {
        goldAt = goldPos_;
    } else {
        goldAt = npcPos_ + glm::vec4(1.2f, 0.2f, 0.0f, 0.0f);  // delivered
    }
    if (showGold) {
        std::vector<ObjectInstance> goldInst = {{goldAt, Math4D::Rotor4D::identity(), GOLD_COL, GOLD_COL}};
        draw(goldInst, goldMesh_, goldBuf_);
    }

    ctx.renderer.drawOuterCube(ctx.outerMVP);
}

void ForestFetchLevel::renderHUD(const LevelContext& ctx) {
    ImGui::SetNextWindowPos(ImVec2(140, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(460, 210), ImGuiCond_Always);
    ImGui::Begin("Forest Fetch", nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);

    ImGui::TextWrapped("Forest Fetch - the ground is the X-Z-W volume; navigate by the landmark trees.");
    if (!ctx.insideMode) {
        ImGui::TextWrapped("Press TAB to step inside the 4D view.");
    } else {
        ImGui::TextWrapped("Move: E/A (forward/back), Q/D (strafe), W/S (along W). "
                           "Turn: J/O and U/L. Look: I/K. Use the Facing/Position gauges (bottom-right).");
        ImGui::Separator();
        // NPC dialogue / current objective, phrased in axes + landmark names.
        switch (quest_) {
            case Q::Ask:
                ImGui::TextWrapped("NPC: \"Fetch me the golden cube. It lies deep toward +W, "
                                   "past the Tall Red Pine.\"");
                break;
            case Q::Fetching:
                ImGui::TextWrapped("Objective: find the golden cube (+W, past the Tall Red Pine) "
                                   "and pick it up.");
                break;
            case Q::Returning:
                ImGui::TextWrapped("Objective: carry the cube back to the NPC at the hub (-W).");
                break;
            case Q::Directed:
                ImGui::TextWrapped("NPC: \"Well done. Now seek the clearing toward -X, "
                                   "past the Crooked Blue Tree.\"");
                break;
            case Q::Done:
                break;
        }
    }

    // Contextual interaction prompt.
    if (!won_ && ctx.insideMode) {
        if (quest_ == Q::Ask && nearNpc_)
            ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.3f, 1.0f), "Press SPACE to accept the task.");
        else if (quest_ == Q::Fetching && nearGold_)
            ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.3f, 1.0f), "Press SPACE to pick up the golden cube.");
        else if (quest_ == Q::Returning && nearNpc_)
            ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.3f, 1.0f), "Press SPACE to deliver the cube.");
    }
    if (won_)
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "You reached the clearing - level complete!");
    ImGui::End();

    glm::vec4 fw = glm::inverse(cam4D_.getOrientation().toMatrix()) * glm::vec4(0.0f, 0.0f, 0.0f, -1.0f);
    hud::drawFacingWidget(fw);
    hud::drawCoordWidget(cam4D_.pos, "You");
}
