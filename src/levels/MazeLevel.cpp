#include "levels/MazeLevel.h"

#include "Renderer.h"
#include "Tesseract.h"
#include "primitives.h"
#include "imgui.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <vector>

namespace {
    // Floor: a calm dark two-tone checker so the white trail and walls read clearly.
    const glm::vec3 FLOOR_A {0.20f, 0.22f, 0.27f};
    const glm::vec3 FLOOR_B {0.15f, 0.17f, 0.21f};
    // Walls: a W-graded palette (low-W bluish, high-W warm) so a block's W-position
    // is legible as a hue, reinforcing that W is a real spatial axis here.
    const glm::vec3 WALL_LOW  {0.28f, 0.32f, 0.46f};
    const glm::vec3 WALL_HIGH {0.55f, 0.48f, 0.40f};

    // Horizontal distance over the X-Z-W floor (Y is height, never counts).
    float hdist(const glm::vec4& a, const glm::vec4& b) {
        float dx = a.x - b.x, dz = a.z - b.z, dw = a.w - b.w;
        return std::sqrt(dx * dx + dz * dz + dw * dw);
    }
}

glm::vec4 MazeLevel::cellWorld(int gi, int gj, int gk, float y) const {
    const float C = (G - 1) * 0.5f;
    return glm::vec4((gi - C) * CELL, y, (gj - C) * CELL, (gk - C) * CELL);
}

MazeLevel::MazeLevel() {
    // Spawn in the entrance room (grid corner 1,1,1), eye at body rest height.
    cam4D_.pos   = cellWorld(1, 1, 1, RADIUS);
    cam4D_.speed = 3.5f;
    body_.radius = RADIUS;

    // Pure navigation: nothing locked, no head-return (you must look down side
    // passages and keep facing that way).
    controls_.lockedAxes = AxisLock::NONE;
    controls_.headReturn = false;
}

MazeLevel::~MazeLevel() {
    wallBuf_.destroy();
    floorBuf_.destroy();
}

void MazeLevel::load() {
    minimap_ = std::make_unique<Minimap>();

    // Shared block meshes: a full CELL cube for walls, a thin slab for floor tiles.
    wallMesh_  = generateBox(glm::vec4(CELL * 0.5f));
    floorMesh_ = generateBox(glm::vec4(CELL * 0.5f, 0.15f, CELL * 0.5f, CELL * 0.5f));
    wallBuf_.init(wallMesh_);
    floorBuf_.init(floorMesh_);

    // --- Carve a perfect 3D maze on the room lattice (recursive backtracker). ---
    // solid[cell] starts all-true; room cells (odd,odd,odd) are open; carving a
    // connection opens the even wall-cell between two rooms. Fixed seed => identical
    // maze every time.
    std::vector<unsigned char> solid((size_t)G * G * G, 1);
    auto gidx = [](int i, int j, int k) { return ((size_t)i * G + j) * G + k; };
    for (int ri = 0; ri < N; ++ri)
    for (int rj = 0; rj < N; ++rj)
    for (int rk = 0; rk < N; ++rk)
        solid[gidx(2 * ri + 1, 2 * rj + 1, 2 * rk + 1)] = 0;

    unsigned seed = 1337u;
    auto rnd = [&]() {
        seed = seed * 1664525u + 1013904223u;
        return (seed >> 8) & 0xFFFFFF;
    };

    std::vector<unsigned char> visited((size_t)N * N * N, 0);
    auto ridx = [](int i, int j, int k) { return ((size_t)i * N + j) * N + k; };
    struct RC { int i, j, k; };
    std::vector<RC> stack;
    visited[ridx(0, 0, 0)] = 1;
    stack.push_back({0, 0, 0});
    const int DI[6] = {1, -1, 0, 0, 0, 0};
    const int DJ[6] = {0, 0, 1, -1, 0, 0};
    const int DK[6] = {0, 0, 0, 0, 1, -1};
    while (!stack.empty()) {
        RC cur = stack.back();
        int cand[6], nc = 0;
        for (int d = 0; d < 6; ++d) {
            int ni = cur.i + DI[d], nj = cur.j + DJ[d], nk = cur.k + DK[d];
            if (ni < 0 || ni >= N || nj < 0 || nj >= N || nk < 0 || nk >= N) continue;
            if (visited[ridx(ni, nj, nk)]) continue;
            cand[nc++] = d;
        }
        if (nc == 0) { stack.pop_back(); continue; }
        int d = cand[rnd() % nc];
        int ni = cur.i + DI[d], nj = cur.j + DJ[d], nk = cur.k + DK[d];
        // Open the wall cell midway between the two rooms (grid coords).
        int wi = (2 * cur.i + 1) + DI[d], wj = (2 * cur.j + 1) + DJ[d], wk = (2 * cur.k + 1) + DK[d];
        solid[gidx(wi, wj, wk)] = 0;
        visited[ridx(ni, nj, nk)] = 1;
        stack.push_back({ni, nj, nk});
    }

    // --- Emit geometry + colliders from the grid. ---
    const Math4D::Rotor4D I = Math4D::Rotor4D::identity();
    auto isOpen = [&](int i, int j, int k) {
        if (i < 0 || i >= G || j < 0 || j >= G || k < 0 || k >= G) return false;  // boundary = solid
        return solid[gidx(i, j, k)] == 0;
    };
    const int NI[6] = {1, -1, 0, 0, 0, 0};
    const int NJ[6] = {0, 0, 1, -1, 0, 0};
    const int NK[6] = {0, 0, 0, 0, 1, -1};

    for (int gi = 0; gi < G; ++gi)
    for (int gj = 0; gj < G; ++gj)
    for (int gk = 0; gk < G; ++gk) {
        if (isOpen(gi, gj, gk)) {
            // Floor tile (top at y=0) + a uniform-cube collider beneath it.
            glm::vec4 fp = cellWorld(gi, gj, gk, -0.15f);
            int parity = (gi + gj + gk) & 1;
            glm::vec3 shade = parity ? FLOOR_A : FLOOR_B;
            floorInsts_.push_back({fp, I, shade, shade});
            world_.addObject(cellWorld(gi, gj, gk, -CELL * 0.5f), CELL * 0.5f);
        } else {
            // Wall block: emit only if it borders a passage (cull invisible interior
            // fill). Visual cube spans y in [0, CELL]; matching uniform-cube collider.
            bool borders = false;
            for (int d = 0; d < 6; ++d)
                if (isOpen(gi + NI[d], gj + NJ[d], gk + NK[d])) { borders = true; break; }
            if (!borders) continue;
            glm::vec4 wp = cellWorld(gi, gj, gk, CELL * 0.5f);
            float t = (float)gk / (float)(G - 1);            // W-position -> hue
            glm::vec3 base = glm::mix(WALL_LOW, WALL_HIGH, t);
            wallInsts_.push_back({wp, I, base * 0.85f, base * 1.15f});
            world_.addObject(wp, CELL * 0.5f);
        }
    }

    // --- Exit marker: a bright tesseract in the far-corner room. ---
    goalPos_ = cellWorld(G - 2, G - 2, G - 2, CELL * 0.5f);
    goal_.push_back({goalPos_, Math4D::Rotor4D::fromXW(0.4f),
                     glm::vec3(1.0f, 0.2f, 0.15f), glm::vec3(1.0f, 0.6f, 0.2f)});

    // --- Minimap frame bounds (map space; W -> vertical). Frame the open extent. ---
    const float B = ((G - 2) - (G - 1) * 0.5f) * CELL + CELL * 0.5f;  // 18 for defaults
    boundsMin_ = glm::vec3(-B, -B, -B);
    boundsMax_ = glm::vec3( B,  B,  B);

    loaded_ = true;
}

void MazeLevel::update(const LevelContext& ctx) {
    runCameraInput(ctx);

    if (ctx.insideMode) {
        // Append to the trajectory when the player has moved enough (bounds the
        // point count and keeps the curve smooth). Map space: W -> y (up).
        const glm::vec4& p = cam4D_.pos;
        glm::vec3 sample(p.x, p.w, p.z);
        if (trail_.empty() || glm::distance(sample, trail_.back()) > 0.3f)
            trail_.push_back(sample);

        if (!won_ && hdist(p, goalPos_) < ARRIVE_DIST)
            won_ = true;
    }
}

void MazeLevel::render(const LevelContext& ctx) {
    const Math4D::Rotor4D ori = cam4D_.getOrientation();

    // Large scene: keep distant walls readable (cf. ForestFetchLevel).
    RenderSettings vis = ctx.vis;
    vis.depthFar    = std::max(vis.depthFar, 90.0f);
    vis.fogStrength = std::min(vis.fogStrength, 0.40f);

    if (!floorInsts_.empty())
        ctx.renderer.drawObjects(floorInsts_, floorMesh_, floorBuf_, cam4D_, ori, focal_, ctx.innerMVP, vis);
    if (!wallInsts_.empty())
        ctx.renderer.drawObjects(wallInsts_, wallMesh_, wallBuf_, cam4D_, ori, focal_, ctx.innerMVP, vis);
    ctx.renderer.drawScene(goal_, ctx.tesseract.buffers, cam4D_, ori, focal_, ctx.tesseract, ctx.innerMVP, vis);

    ctx.renderer.drawOuterCube(ctx.outerMVP);

    // Minimap: a small 3D cube nestled into a bottom corner of the outer box,
    // drawn in world space so it sits in the corner of the 3D box and turns with
    // it. Shows only the player's trajectory (white curve) + a facing arrow.
    if (minimap_) {
        float aspect = ctx.projection[0][0] != 0.0f
                     ? ctx.projection[1][1] / ctx.projection[0][0] : 1.0f;
        const glm::vec4& p = cam4D_.pos;
        glm::vec3 player(p.x, p.w, p.z);                                  // map space: W -> up
        glm::vec4 fw = glm::inverse(ori.toMatrix()) * glm::vec4(0.0f, 0.0f, 0.0f, -1.0f);
        glm::vec3 facing(fw.x, fw.w, fw.z);
        const float ah = 0.14f;                                          // outer box is +/-0.5
        glm::vec3 anchor(0.5f - ah, -0.5f + ah, 0.5f - ah);              // bottom +X,+Z corner
        minimap_->drawInWorld(ctx.renderer, ctx.outerMVP, aspect, trail_,
                              boundsMin_, boundsMax_, player, facing, anchor, ah);
    }
}

void MazeLevel::renderHUD(const LevelContext& ctx) {
    ImGui::SetNextWindowPos(ImVec2(140, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(460, 170), ImGuiCond_Always);
    ImGui::Begin("Maze", nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);
    ImGui::TextWrapped("Maze - find the bright red cube at the far corner. The maze branches "
                       "in X, Z and W; there is no up.");
    if (!ctx.insideMode) {
        ImGui::TextWrapped("Press TAB to step inside the 4D view.");
    } else {
        ImGui::TextWrapped("Move: E/A (forward/back), Q/D (strafe), W/S (along W). "
                           "Turn: J/O and U/L. Look: I/K.");
        ImGui::TextWrapped("The small cube in the bottom corner of the box is your map: the "
                           "white curve is your path so far, its height = how deep in W you are, "
                           "and the cyan arrow is your heading.");
    }
    if (won_)
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "You reached the exit - level complete!");
    ImGui::End();
}
