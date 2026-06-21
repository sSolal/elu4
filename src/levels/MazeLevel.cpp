#include "levels/MazeLevel.h"

#include "Renderer.h"
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

    // The 6 axis-aligned grid steps (±X, ±Z, ±W in grid space). Shared by maze
    // emission and per-frame visibility gathering.
    const int kNI[6] = {1, -1, 0, 0, 0, 0};
    const int kNJ[6] = {0, 0, 1, -1, 0, 0};
    const int kNK[6] = {0, 0, 0, 0, 1, -1};
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

    // Shared block meshes: a full CELL cube for walls (occludes — you cannot see
    // through a 4D wall, and the PVS cull below keeps the visible wall set under the
    // occluder budget), a thin slab for floor tiles (generateGround: a floor is
    // looked across, and it is anyway externally occluded by the visible walls).
    wallMesh_  = generateBox(glm::vec4(CELL * 0.5f));
    floorMesh_ = generateGround(glm::vec4(CELL * 0.5f, 0.15f, CELL * 0.5f, CELL * 0.5f));
    wallBuf_.init(wallMesh_);
    floorBuf_.init(floorMesh_);

    // --- Carve a perfect 3D maze on the room lattice (recursive backtracker). ---
    // solid[cell] starts all-true; room cells (odd,odd,odd) are open; carving a
    // connection opens the even wall-cell between two rooms. Fixed seed => identical
    // maze every time.
    solid_.assign((size_t)G * G * G, 1);
    auto gidx = [](int i, int j, int k) { return ((size_t)i * G + j) * G + k; };
    for (int ri = 0; ri < N; ++ri)
    for (int rj = 0; rj < N; ++rj)
    for (int rk = 0; rk < N; ++rk)
        solid_[gidx(2 * ri + 1, 2 * rj + 1, 2 * rk + 1)] = 0;

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
        solid_[gidx(wi, wj, wk)] = 0;
        visited[ridx(ni, nj, nk)] = 1;
        stack.push_back({ni, nj, nk});
    }

    // --- Emit geometry + colliders from the grid. ---
    // Cell -> instance reverse maps drive the per-frame visibility cull; -1 = no
    // instance at that cell (a cell yields at most one wall OR one floor, never both).
    wallInstOfCell_.assign((size_t)G * G * G, -1);
    floorInstOfCell_.assign((size_t)G * G * G, -1);
    bfsVisited_.assign((size_t)G * G * G, 0);
    bfsQueue_.reserve((size_t)G * G * G);

    const Math4D::Rotor4D I = Math4D::Rotor4D::identity();
    auto isOpen = [&](int i, int j, int k) {
        if (i < 0 || i >= G || j < 0 || j >= G || k < 0 || k >= G) return false;  // boundary = solid
        return solid_[gidx(i, j, k)] == 0;
    };

    for (int gi = 0; gi < G; ++gi)
    for (int gj = 0; gj < G; ++gj)
    for (int gk = 0; gk < G; ++gk) {
        size_t cid = gidx(gi, gj, gk);
        if (isOpen(gi, gj, gk)) {
            // Floor tile (top at y=0) + a uniform-cube collider beneath it.
            glm::vec4 fp = cellWorld(gi, gj, gk, -0.15f);
            int parity = (gi + gj + gk) & 1;
            glm::vec3 shade = parity ? FLOOR_A : FLOOR_B;
            floorInstOfCell_[cid] = (int)floorInsts_.size();
            floorInsts_.push_back({fp, I, shade, shade});
            world_.addObject(cellWorld(gi, gj, gk, -CELL * 0.5f), CELL * 0.5f);
        } else {
            // Wall block: emit only if it borders a passage (cull invisible interior
            // fill). Visual cube spans y in [0, CELL]; matching uniform-cube collider.
            bool borders = false;
            for (int d = 0; d < 6; ++d)
                if (isOpen(gi + kNI[d], gj + kNJ[d], gk + kNK[d])) { borders = true; break; }
            if (!borders) continue;
            glm::vec4 wp = cellWorld(gi, gj, gk, CELL * 0.5f);
            float t = (float)gk / (float)(G - 1);            // W-position -> hue
            glm::vec3 base = glm::mix(WALL_LOW, WALL_HIGH, t);
            wallInstOfCell_[cid] = (int)wallInsts_.size();
            wallInsts_.push_back({wp, I, base * 0.85f, base * 1.15f});
            world_.addObject(wp, CELL * 0.5f);
        }
    }

    // --- Exit marker: a bright tesseract in the far-corner room. ---
    goalCell_ = (int)gidx(G - 2, G - 2, G - 2);
    goalPos_ = cellWorld(G - 2, G - 2, G - 2, CELL * 0.5f);
    goal_.push_back({goalPos_, Math4D::Rotor4D::fromXW(0.4f),
                     glm::vec3(1.0f, 0.2f, 0.15f), glm::vec3(1.0f, 0.6f, 0.2f)});

    // --- Minimap frame bounds (map space; W -> vertical). Frame the open extent. ---
    const float B = ((G - 2) - (G - 1) * 0.5f) * CELL + CELL * 0.5f;  // 14 for defaults
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

void MazeLevel::rebuildVisible() {
    auto gidx   = [](int i, int j, int k) { return ((size_t)i * G + j) * G + k; };
    auto inGrid = [](int i, int j, int k) {
        return i >= 0 && i < G && j >= 0 && j < G && k >= 0 && k < G;
    };
    auto clampG = [](int v) { return v < 0 ? 0 : (v >= G ? G - 1 : v); };

    // Player's grid cell = inverse of cellWorld (gi->X, gj->Z, gk->W).
    const float C = (G - 1) * 0.5f;
    const glm::vec4& p = cam4D_.pos;
    int pi = clampG((int)std::lround(p.x / CELL + C));
    int pj = clampG((int)std::lround(p.z / CELL + C));
    int pk = clampG((int)std::lround(p.w / CELL + C));

    // If we land on a solid cell (boundary rounding), snap to the nearest open
    // neighbour; if there is none, fail safe by drawing the whole maze.
    if (solid_[gidx(pi, pj, pk)]) {
        bool found = false;
        for (int d = 0; d < 6 && !found; ++d) {
            int ni = pi + kNI[d], nj = pj + kNJ[d], nk = pk + kNK[d];
            if (inGrid(ni, nj, nk) && !solid_[gidx(ni, nj, nk)]) {
                pi = ni; pj = nj; pk = nk; found = true;
            }
        }
        if (!found) {
            lastPlayerCell_ = -1;
            visWalls_   = wallInsts_;
            visFloors_  = floorInsts_;
            goalVisible_ = true;
            return;
        }
    }

    int startCell = (int)gidx(pi, pj, pk);
    if (startCell == lastPlayerCell_) return;   // cache: same cell -> same visible set
    lastPlayerCell_ = startCell;

    std::fill(bfsVisited_.begin(), bfsVisited_.end(), (unsigned char)0);
    visWalls_.clear();
    visFloors_.clear();
    bfsQueue_.clear();
    goalVisible_ = false;

    // A solid cell's wall is added at most once (bfsVisited_ doubles as the wall-added
    // mark — solid cells never enter the open-cell queue, so the bit is free to reuse).
    auto addWall = [&](int cell) {
        if (bfsVisited_[cell]) return;
        bfsVisited_[cell] = 1;
        int wi = wallInstOfCell_[cell];
        if (wi >= 0) visWalls_.push_back(wallInsts_[wi]);
    };
    // An open cell contributes its floor tile (and flags the goal if it lives there)
    // plus every solid neighbour as a bordering wall.
    auto addOpen = [&](int cell, int ci, int cj, int ck) {
        int fi = floorInstOfCell_[cell];
        if (fi >= 0) visFloors_.push_back(floorInsts_[fi]);
        if (cell == goalCell_) goalVisible_ = true;
        for (int d = 0; d < 6; ++d) {
            int ni = ci + kNI[d], nj = cj + kNJ[d], nk = ck + kNK[d];
            if (inGrid(ni, nj, nk) && solid_[gidx(ni, nj, nk)]) addWall((int)gidx(ni, nj, nk));
        }
    };

    // Local pocket: level-synchronous BFS through open cells out to VIS_DEPTH.
    bfsQueue_.push_back(startCell);
    bfsVisited_[startCell] = 1;
    addOpen(startCell, pi, pj, pk);
    size_t levelEnd = bfsQueue_.size();
    int depth = 0;
    for (size_t head = 0; head < bfsQueue_.size(); ++head) {
        if (head == levelEnd) { ++depth; levelEnd = bfsQueue_.size(); }
        int cell = bfsQueue_[head];
        int ci = cell / (G * G), cj = (cell / G) % G, ck = cell % G;
        if (depth >= VIS_DEPTH) continue;       // explored far enough; walls already added
        for (int d = 0; d < 6; ++d) {
            int ni = ci + kNI[d], nj = cj + kNJ[d], nk = ck + kNK[d];
            if (!inGrid(ni, nj, nk)) continue;
            int ncell = (int)gidx(ni, nj, nk);
            if (!solid_[ncell] && !bfsVisited_[ncell]) {
                bfsVisited_[ncell] = 1;
                addOpen(ncell, ni, nj, nk);
                bfsQueue_.push_back(ncell);
            }
        }
    }

    // Corridor line-of-sight: from the player cell, march straight down each axis
    // through open cells until a wall, revealing long straight halls (and their
    // capping wall) that the bounded local pocket would clip.
    for (int d = 0; d < 6; ++d) {
        int ci = pi, cj = pj, ck = pk;
        while (true) {
            int ni = ci + kNI[d], nj = cj + kNJ[d], nk = ck + kNK[d];
            if (!inGrid(ni, nj, nk)) break;
            int ncell = (int)gidx(ni, nj, nk);
            if (solid_[ncell]) { addWall(ncell); break; }   // wall capping the corridor
            if (!bfsVisited_[ncell]) {                       // first visit: floor + side walls
                bfsVisited_[ncell] = 1;
                addOpen(ncell, ni, nj, nk);
            }
            ci = ni; cj = nj; ck = nk;
        }
    }
}

void MazeLevel::render(const LevelContext& ctx) {
    const Math4D::Rotor4D ori = cam4D_.getOrientation();

    // Large scene: keep distant walls readable (cf. ForestFetchLevel).
    RenderSettings vis = largeScene(ctx.vis, 90.0f, 0.40f);

    // Visibility cull: gather only the walls/floors reachable by sight from the
    // player's cell, so the 4D occlusion (capped occluder budget) covers them all and
    // hides everything behind. Floor and goal are occluded by the visible WALL set so
    // they don't show through walls (occluders are otherwise per-draw-call).
    rebuildVisible();

    if (!visFloors_.empty())
        ctx.renderer.drawObjects(visFloors_, floorMesh_, floorBuf_, cam4D_, ori, focal_, ctx.innerMVP, vis,
                                 &visWalls_, &wallMesh_);
    if (!visWalls_.empty())
        ctx.renderer.drawObjects(visWalls_, wallMesh_, wallBuf_, cam4D_, ori, focal_, ctx.innerMVP, vis);
    if (goalVisible_)
        ctx.renderer.drawObjects(goal_, ctx.hyperMesh, ctx.hyperBuf, cam4D_, ori, focal_, ctx.innerMVP, vis,
                                 &visWalls_, &wallMesh_);

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
