#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <string>

#include "Camera.h"
#include "physics.h"
#include "LevelControls.h"
#include "RenderSettings.h"

class Renderer;       // fwd — defined in Renderer.h
struct Object4D;      // fwd — defined in Object4D.h
struct ObjectBuffer;  // fwd — defined in ObjectBuffer.h

// Everything a level needs each frame, supplied by the runner in main.cpp so
// levels never reach for globals. The runner updates the level first, then
// computes the camera MVPs and calls render(); update-before-render ordering
// matters because the MVPs derive from the (just-updated) outside camera.
struct LevelContext {
    GLFWwindow* window;
    Renderer&   renderer;
    Object4D&     hyperMesh;  // shared hypercube mesh (marker/goal cubes)
    ObjectBuffer& hyperBuf;   // its GPU buffers
    float       dt;
    bool        insideMode;  // TAB state (3D observer vs 4D FPS), owned by runner
    glm::mat4   projection;
    glm::mat4   outerMVP;
    glm::mat4   innerMVP;
    RenderSettings& vis;     // global visualization toggles, owned by runner
    bool        worldHUD = false;  // VR: HUD lives in the 3D world, not screen-space
};

// Abstract base for every level. Concrete levels add only their scene data and
// override the hooks; the common camera/physics/control state lives here so the
// per-level boilerplate that used to fill main.cpp collapses into the framework.
class Level {
public:
    virtual ~Level() = default;

    virtual const char* name() const = 0;

    // One-time setup: build the scene, register physics AABBs, allocate GPU
    // resources. Split from the constructor because it needs a live GL context
    // and runs lazily on first entry. Sets loaded_ = true.
    virtual void load() = 0;
    bool isLoaded() const { return loaded_; }

    // Per-frame input + physics + game logic.
    virtual void update(const LevelContext& ctx) = 0;

    // Per-frame draw calls (4D content + outer cube).
    virtual void render(const LevelContext& ctx) = 0;

    // Returns true once the win condition is met; the runner then returns to menu.
    virtual bool checkWin() const { return false; }

    // ImGui HUD / instructional text (screen-space).
    virtual void renderHUD(const LevelContext& ctx) {}

    // 3D world-space HUD markers, drawn in the scene pass with the per-eye MVP in
    // ctx.innerMVP. Used in VR for things that must live on the cube (e.g. the
    // direction dot on a cube face) instead of as a flat ImGui overlay. Called
    // only when ctx.worldHUD is true. Default: nothing.
    virtual void renderWorldHUD(const LevelContext& ctx) {}

    // Space-to-interact hook.
    virtual void onInteract() {}

    // Scene-graph hook: a level (e.g. a Lua scene) may ask the runner to swap to
    // another scene. Returns the requested scene name and clears the request, or
    // "" if none. C++ levels never request a transition (default empty).
    virtual std::string takeSceneRequest() { return {}; }

    // The runner needs these to drive the outer-cube camera and focal length.
    Camera3D& cam3D()       { return cam3D_; }
    Camera4D& cam4D()       { return cam4D_; }
    float&    focalLength() { return focal_; }

protected:
    // The standard input path shared by most levels: 3D observer when outside,
    // 4D FPS (with this level's control config) when TABbed inside.
    void runCameraInput(const LevelContext& ctx) {
        if (ctx.insideMode)
            cam4D_.processInput(ctx.window, ctx.dt, body_, world_, controls_);
        else
            cam3D_.processInput(ctx.window, ctx.dt);
    }

    bool          loaded_ = false;
    Camera3D      cam3D_;
    Camera4D      cam4D_;
    PhysicsWorld  world_;
    PhysicsBody   body_;
    float         focal_ = 1.5f;
    LevelControls controls_;
};
