#pragma once

#include <vector>
#include "Level.h"
#include "Scene.h"
#include "Level2.h"        // ObjectInstance
#include "Object4D.h"
#include "ObjectBuffer.h"

// Level 1 — "Corridor". Teaches W-translation = zoom in isolation:
//   * strafe (X/Z) is locked, so the only meaningful motion is forward/back
//     along W, which reads visually as zooming in and out;
//   * the head can turn but gently springs back to centre;
//   * a tiled tube runs along W toward a red hypercube goal; reach it and press
//     Space to win.
//
// The tube is built from cube tiles: full size in X/Y/Z but THIN in W, so each
// reads as a clean cube rather than a nested hypercube. Tiles abut across the
// cross-section (touching their wall and corner neighbours); a small gap between
// rings along W plus a checkerboard shade make the tiling legible. Press H to
// hide every wall except the floor.
class CorridorLevel : public Level {
public:
    CorridorLevel();
    ~CorridorLevel() override;

    const char* name() const override { return "1 - Corridor"; }

    void load() override;
    void update(const LevelContext& ctx) override;
    void render(const LevelContext& ctx) override;
    bool checkWin() const override { return won_; }
    void renderHUD(const LevelContext& ctx) override;

private:
    Object4D     tileMesh_;          // one shared flat-cube tile
    ObjectBuffer tileBuf_;
    std::vector<ObjectInstance> floorInsts_;  // the ground (always shown)
    std::vector<ObjectInstance> wallInsts_;   // ceiling + side walls (H toggles)

    std::vector<ObjectInstance> goal_;        // the red hypercube goal
    glm::vec4 goalPos_;
    bool      won_           = false;
    bool      nearGoal_      = false;
    bool      showWalls_     = true;
    bool      hKeyWasDown_   = false;

    static constexpr float GOAL_W = -9.0f;    // goal sits at the far (forward) end
};
