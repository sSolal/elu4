#include "levels/CorridorLevel.h"

#include "Renderer.h"
#include "primitives.h"
#include "imgui.h"
#include <cmath>

// The corridor is a row of big cubes, each as wide as the corridor itself. Per
// segment along W there's a ring of 6 cubes (floor, ceiling, and the four side
// walls) around an open bore one cube wide; consecutive segments alternate
// colour. The floor row also extends an equal length behind the entrance, with
// no walls, so the player can back out and observe the tube from outside.
namespace {
    constexpr float WIDTH  = 2.0f;        // corridor width = cube side length (spacing)
    constexpr float H      = WIDTH * 0.5f;// cube half-extent in X/Y/Z (placement/physics)
    constexpr float W_HALF = H;           // cube half-extent in W = H (placement)
    // The drawn/occluder cube is shrunk by GAP so neighbouring segments no longer
    // share faces. Coincident surfaces make the 4D occlusion test tie ambiguously
    // (flicker as the camera moves); a hair of empty space resolves it cleanly and
    // also reads as a seam between segments.
    constexpr float GAP    = 0.04f;
    constexpr int   N      = 5;           // segments in the corridor (and in the rear floor)
    // Segment centres run -0.5*WIDTH, -1.5*WIDTH, ... into the corridor (negative W
    // is forward), and +0.5*WIDTH, +1.5*WIDTH, ... onto the rear platform.
    constexpr float REARMOST = (N - 0.5f) * WIDTH;   // centre of the rear-most floor cube
}

CorridorLevel::CorridorLevel() {
    // Spawn at the mouth of the corridor, facing down it (identity yaw, camera
    // looks along -W). Forward (W key) decreases w toward the goal; back (S key)
    // increases w, out onto the rear platform.
    cam4D_.pos = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
    body_.radius = 0.75f;

    // The mechanic this level teaches: only W-motion is allowed, and the head
    // springs back to looking straight down the corridor.
    controls_.lockedAxes         = AxisLock::X | AxisLock::Z;  // strafe locked
    controls_.lockIsWorldSpace   = true;
    controls_.headReturn         = true;
    controls_.headReturnStrength = 7.0f;
    controls_.defaultYaw         = Math4D::Rotor4D::identity();
    controls_.defaultPitch       = 0.0f;

    goalPos_ = glm::vec4(0.0f, 0.0f, 0.0f, GOAL_W);
}

CorridorLevel::~CorridorLevel() {
    tileBuf_.destroy();
}

void CorridorLevel::load() {
    // One near-perfect cube, reused for every segment cube via per-instance colour.
    // Slightly smaller than the WIDTH spacing so segments have a thin seam (see GAP).
    tileMesh_ = generateBox({H - GAP, H - GAP, H - GAP, W_HALF - GAP});
    tileBuf_.init(tileMesh_);

    // Two-tone palettes; alternation along the corridor makes each cube distinct.
    // Saturated, distinctly-hued surfaces (warm floor / cool ceiling / teal walls)
    // so the depth fog — which desaturates with distance — produces a clear
    // near-vivid → far-grey gradient along (and within) each cube.
    const glm::vec3 floorShade[2] = {{0.62f, 0.34f, 0.13f}, {0.48f, 0.24f, 0.08f}};  // warm orange-brown
    const glm::vec3 ceilShade [2] = {{0.28f, 0.47f, 0.78f}, {0.20f, 0.36f, 0.64f}};  // cool blue
    const glm::vec3 wallShade [2] = {{0.24f, 0.56f, 0.48f}, {0.16f, 0.43f, 0.36f}};  // teal
    const Math4D::Rotor4D I = Math4D::Rotor4D::identity();

    // Parity (and therefore colour) increases from the rear-most cube forward, so
    // the alternation is continuous across the whole floor.
    auto parityAt = [](float w) { return (int)std::lround((REARMOST - w) / WIDTH) & 1; };

    auto addCube = [&](glm::vec4 pos, const glm::vec3& shade, bool isFloor) {
        ObjectInstance inst{pos, I, shade, shade};
        (isFloor ? floorInsts_ : wallInsts_).push_back(inst);
        if (isFloor) world_.addObject(pos, H);  // only the floor is solid (X/Z locked)
    };

    // Floor row: rear platform (w > 0) plus the corridor (w < 0), one cube per segment.
    for (int s = 0; s < 2 * N; ++s) {
        float w = REARMOST - s * WIDTH;   // rear-most -> front-most
        addCube({0.0f, -WIDTH, 0.0f, w}, floorShade[s & 1], true);
    }

    // Walls + ceiling: corridor segments only, so the rear platform stays open.
    for (int s = 0; s < N; ++s) {
        float w = -(s + 0.5f) * WIDTH;    // -1*H, -3*H, ... into the corridor
        int p = parityAt(w);
        addCube({0.0f,  WIDTH, 0.0f, w}, ceilShade[p], false);  // ceiling
        addCube({ WIDTH, 0.0f, 0.0f, w}, wallShade[p], false);  // +X
        addCube({-WIDTH, 0.0f, 0.0f, w}, wallShade[p], false);  // -X
        addCube({0.0f, 0.0f,  WIDTH, w}, wallShade[p], false);  // +Z
        addCube({0.0f, 0.0f, -WIDTH, w}, wallShade[p], false);  // -Z
    }

    // The red hypercube goal at the far end (no AABB — you walk right up to it).
    goal_.push_back({goalPos_, Math4D::Rotor4D::fromXW(0.4f),
                     glm::vec3(1.0f, 0.15f, 0.15f), glm::vec3(1.0f, 0.55f, 0.2f)});

    loaded_ = true;
}

void CorridorLevel::update(const LevelContext& ctx) {
    runCameraInput(ctx);

    // H toggles every wall except the floor (edge-detected).
    bool hDown = glfwGetKey(ctx.window, GLFW_KEY_H) == GLFW_PRESS;
    if (hDown && !hKeyWasDown_) showWalls_ = !showWalls_;
    hKeyWasDown_ = hDown;

    // You can only interact with the goal from up close, and only inside the 4D
    // FPS view (where W-movement happens).
    float dw = std::abs(cam4D_.pos.w - goalPos_.w);
    nearGoal_ = ctx.insideMode && dw < 1.5f;
    if (nearGoal_ && glfwGetKey(ctx.window, GLFW_KEY_SPACE) == GLFW_PRESS)
        won_ = true;
}

void CorridorLevel::render(const LevelContext& ctx) {
    Math4D::Rotor4D ori = cam4D_.getOrientation();
    ctx.renderer.drawObjects(floorInsts_, tileMesh_, tileBuf_, cam4D_, ori, focal_, ctx.innerMVP, ctx.vis);
    if (showWalls_)
        ctx.renderer.drawObjects(wallInsts_, tileMesh_, tileBuf_, cam4D_, ori, focal_, ctx.innerMVP, ctx.vis);
    ctx.renderer.drawObjects(goal_, ctx.hyperMesh, ctx.hyperBuf, cam4D_, ori, focal_, ctx.innerMVP, ctx.vis);
    ctx.renderer.drawOuterCube(ctx.outerMVP);
}

void CorridorLevel::renderHUD(const LevelContext& ctx) {
    ImGui::SetNextWindowPos(ImVec2(140, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(400, 130), ImGuiCond_Always);
    ImGui::Begin("Corridor", nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);
    ImGui::TextWrapped("Corridor - moving forward zooms in.");
    if (!ctx.insideMode) {
        ImGui::TextWrapped("Press TAB to step inside the 4D view.");
    } else {
        ImGui::TextWrapped("W/S: move forward/back along W. Strafing is locked.");
        ImGui::TextWrapped("Back out (S) onto the rear floor to observe the tube.");
        ImGui::TextWrapped("H: hide/show the walls (the floor stays).");
    }
    if (won_)
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Level complete!");
    else if (nearGoal_)
        ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.3f, 1.0f), "Press SPACE to interact with the red cube.");
    ImGui::End();
}
