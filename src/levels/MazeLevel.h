#pragma once

#include <vector>
#include <memory>
#include "Level.h"
#include "Object4D.h"
#include "ObjectBuffer.h"
#include "Renderer.h"   // ObjectInstance (via Scene.h)
#include "Minimap.h"

// Level 9 - Maze.
//
// Pure wayfinding: a procedurally generated (fixed-seed) block maze laid out across
// the X-Z-W floor 3-volume. Passages turn in all three horizontal axes (X, Z and W)
// but never up - Y is height only. Walls are solid cube-blocks (one mesh, many
// instances) whose collision reuses the engine's uniform-cube AABBs. The player has
// full, unrestricted first-person controls and must reach the goal at the far corner.
//
// A corner minimap shows a small 3D cube (the floor volume, with W mapped to the
// cube's vertical) containing only the player's whole trajectory as a white curve,
// plus a facing arrow - it never reveals the maze itself.
class MazeLevel : public Level {
public:
    MazeLevel();
    ~MazeLevel() override;

    const char* name() const override { return "9 - Maze"; }

    void load() override;
    void update(const LevelContext& ctx) override;
    void render(const LevelContext& ctx) override;
    bool checkWin() const override { return won_; }
    void renderHUD(const LevelContext& ctx) override;

private:
    // World position of grid cell (gi,gj,gk): gi->X, gj->Z, gk->W (symmetric); y given.
    glm::vec4 cellWorld(int gi, int gj, int gk, float y) const;

    // --- Meshes + GPU buffers ---
    Object4D     wallMesh_, floorMesh_;
    ObjectBuffer wallBuf_,  floorBuf_;

    // --- Static scene instances ---
    std::vector<ObjectInstance> wallInsts_;
    std::vector<ObjectInstance> floorInsts_;
    std::vector<ObjectInstance> goal_;          // bright hypercube marker at the exit

    glm::vec4 goalPos_{0.0f};
    bool      won_ = false;

    // --- Trajectory (map space: x=worldX, y=worldW (up), z=worldZ) ---
    std::vector<glm::vec3> trail_;

    std::unique_ptr<Minimap> minimap_;
    glm::vec3 boundsMin_{0.0f}, boundsMax_{0.0f};  // minimap frame, map space

    // --- Tunables ---
    static constexpr int   N      = 5;          // rooms per axis
    static constexpr int   G      = 2 * N + 1;  // grid dimension (11): odd=rooms, even=walls
    static constexpr float CELL   = 4.0f;       // block size (passage width)
    static constexpr float RADIUS = 1.0f;       // player half-extent (< CELL/2 => fits)
    static constexpr float ARRIVE_DIST = 3.0f;  // win range at the exit
};
