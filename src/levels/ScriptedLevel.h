#pragma once

#include <memory>
#include <string>

#include "Level.h"

// A single generic Level that runs a Lua script. The script defines the per-level
// logic as global functions (load/update/render/check_win/render_hud/on_interact)
// and drives the engine through the `engine` table bound in ScriptedLevel.cpp.
// One ScriptedLevel == one .lua file; the registry lists scripted levels alongside
// the C++ ones so they can be A/B compared in the same build.
//
// sol2 is heavy, so all of it lives in the .cpp via a pimpl: this header pulls in
// no scripting headers and stays cheap to include from the registry.
class ScriptedLevel : public Level {
public:
    ScriptedLevel(std::string scriptPath, std::string displayName);
    ~ScriptedLevel() override;

    const char* name() const override { return displayName_.c_str(); }

    void load() override;
    void update(const LevelContext& ctx) override;
    void render(const LevelContext& ctx) override;
    bool checkWin() const override { return won_; }
    void renderHUD(const LevelContext& ctx) override;
    void onInteract() override;
    std::string takeSceneRequest() override;

private:
    struct Impl;                 // holds sol::state + captured hook handles
    std::unique_ptr<Impl> impl_;

    std::string scriptPath_;
    std::string displayName_;

    // Per-frame context, valid only for the duration of one forwarded hook. The
    // engine free-functions bound to Lua read it; they error if it is null (i.e.
    // if a script calls a draw/input function outside the matching hook).
    const LevelContext* curCtx_ = nullptr;

    bool won_   = false;  // flipped by engine.set_won()
    bool inert_ = false;  // a script error puts the level in a safe no-op state

    // Set by engine.goto_scene(name); the runner polls takeSceneRequest() after
    // update() and swaps to the named scene. Empty when no transition is pending.
    std::string pendingScene_;

    // When true (script sets engine.use_standard_input(true) in load), the host
    // runs the shared Level::runCameraInput before each Lua update(). Default off:
    // the script drives the camera itself (e.g. the plane's flight model).
    bool standardInput_ = false;

    // Per-level render tuning set via engine.set_scene_far(far, maxFog): relaxes the
    // depth fog/far plane (largeScene) so distant landmarks stay readable. When unset
    // the level renders with the global RenderSettings unchanged.
    bool  hasSceneOverride_ = false;
    float sceneFar_    = 0.0f;
    float sceneMaxFog_ = 0.0f;
    int   depthOverride_ = -1;  // engine.set_depth_cue: -1 = none, else DepthCue value

    // A second camera the script can drive with the shared FPS input path
    // (engine.drive_avatar) for third-person levels: it steers this avatar + the
    // physics body while the inherited cam4D_ is recomputed as the follow cam.
    Camera4D avatar_;

    // The per-frame RenderSettings the draw calls use (global vis + this level's
    // scene-far / depth-cue overrides). Valid only while curCtx_ is set.
    RenderSettings currentVis() const;

    // Builds the `engine` table into impl_->lua. Declared here (no sol types in the
    // signature) and defined in the .cpp alongside the bindings.
    void buildEngineTable();
};
