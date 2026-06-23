#include "levels/ScriptedLevel.h"

#include <cstdio>
#include <memory>
#include <utility>
#include <vector>

#include <sol/sol.hpp>

#include "imgui.h"
#include "Renderer.h"
#include "Minimap.h"
#include "Object4D.h"
#include "ObjectBuffer.h"
#include "Scene.h"
#include "primitives.h"
#include "mesh_merge.h"
#include "Asset.h"
#include "HudWidgets.h"
#include "RenderSettings.h"
#include "lua/LuaBindings.h"

namespace {

// A reusable batch of placed instances. Lua builds one once and refills it across
// frames (clear/add), so the heavy per-instance data stays C++-side and never
// round-trips through a Lua table on the per-frame draw path.
struct InstanceSet {
    std::vector<ObjectInstance> items;
};

// Special mesh handle: draw against the shared hypercube mesh/buffer that the runner
// threads through LevelContext (used for goal markers), without the script having to
// create its own.
constexpr int kHyperMeshHandle = -1;

// Call a (possibly absent) nullary Lua hook under protection. Returns false if the
// script raised an error, so the caller can drop the level into the inert state.
bool callHook(sol::protected_function& fn, const char* hookName,
              const std::string& levelName) {
    if (!fn.valid()) return true;  // optional hook simply not defined
    sol::protected_function_result r = fn();
    if (!r.valid()) {
        sol::error err = r;
        std::fprintf(stderr, "[ScriptedLevel %s] error in %s(): %s\n",
                     levelName.c_str(), hookName, err.what());
        return false;
    }
    return true;
}

// Bind the engine value/state types shared across levels. Kept here (not in the
// headless LuaBindings.cpp) because Camera.h pulls in GL headers.
void bindEngineTypes(sol::state& L) {
    L.new_usertype<Camera4D>(
        "Camera4D", sol::no_constructor,
        "pos", &Camera4D::pos,            // class-type members bind by reference, so
        "yaw", &Camera4D::yaw,            // `cam4d.pos = vec4(...)` writes through to
        "pitch", &Camera4D::pitch,        // the real camera (no value-copy trap).
        "speed", &Camera4D::speed,
        "look_speed", &Camera4D::lookSpeed,
        "get_orientation", &Camera4D::getOrientation);

    L.new_usertype<Camera3D>(
        "Camera3D", sol::no_constructor,
        "pos", &Camera3D::pos,
        "yaw", &Camera3D::yaw,
        "pitch", &Camera3D::pitch,
        "speed", &Camera3D::speed);

    L.new_usertype<PhysicsBody>(
        "PhysicsBody", sol::no_constructor,
        "pos", &PhysicsBody::pos,
        "velY", &PhysicsBody::velY,
        "on_ground", &PhysicsBody::onGround,
        "radius", &PhysicsBody::radius,
        "gravityScale", &PhysicsBody::gravityScale);

    L.new_usertype<PhysicsWorld>(
        "PhysicsWorld", sol::no_constructor,
        "add_object", &PhysicsWorld::addObject,
        "add_box", &PhysicsWorld::addBox,
        "add_flat_ground",
        sol::overload(
            [](PhysicsWorld& w, float y, float ext) { w.addFlatGround(y, ext); },
            [](PhysicsWorld& w, float y, float ext, float th) {
                w.addFlatGround(y, ext, th);
            }));

    L.new_usertype<LevelControls>(
        "LevelControls", sol::no_constructor,
        "lockedAxes", &LevelControls::lockedAxes,
        "lockIsWorldSpace", &LevelControls::lockIsWorldSpace,
        "turnXW", &LevelControls::turnXW,
        "turnZW", &LevelControls::turnZW,
        "pitch", &LevelControls::pitch,
        "softLockMax", &LevelControls::softLockMax,
        "softLockReturn", &LevelControls::softLockReturn,
        "headReturn", &LevelControls::headReturn,
        "headReturnStrength", &LevelControls::headReturnStrength,
        "headReturnSnap", &LevelControls::headReturnSnap,
        "defaultYaw", &LevelControls::defaultYaw,
        "defaultPitch", &LevelControls::defaultPitch);

    L.new_usertype<InstanceSet>(
        "InstanceSet", sol::no_constructor,
        "clear", [](InstanceSet& s) { s.items.clear(); },
        "add",
        [](InstanceSet& s, const glm::vec4& pos, const Math4D::Rotor4D& ori,
           const glm::vec3& colorA, const glm::vec3& colorB) {
            s.items.push_back({pos, ori, colorA, colorB});
        },
        "size", [](InstanceSet& s) { return (int)s.items.size(); });
}

}  // namespace

// ---------------------------------------------------------------------------
// pimpl: the sol::state, captured hooks, and the host-owned GL resources keyed by
// integer handle. Keeping sol2 + GL buffers here keeps ScriptedLevel.h light.
// ---------------------------------------------------------------------------
struct ScriptedLevel::Impl {
    sol::state lua;
    sol::protected_function fnLoad, fnUpdate, fnRender, fnCheckWin, fnHud, fnInteract;

    std::vector<std::unique_ptr<Object4D>>     meshes;
    std::vector<std::unique_ptr<ObjectBuffer>> buffers;

    // Loaded assets: each part is already registered as a (mesh, buffer) above; this
    // records the per-part local transform + colours so draw_asset / asset_colliders
    // can place the whole prop with one call. (Single source of truth for the props
    // the scene scripts used to build inline.)
    struct AssetPartInst {
        int             meshHandle;
        glm::vec4       pos;
        Math4D::Rotor4D rot;
        glm::vec3       colorA, colorB;
        glm::vec4       half;     // AABB half-extents, for solid colliders
        bool            solid;
    };
    std::vector<std::vector<AssetPartInst>> assets;

    std::unique_ptr<Minimap> minimap;
    bool                     minimapTrace = false;
    std::vector<glm::vec3>   trail;  // map space (W -> vertical), owned here
};

ScriptedLevel::ScriptedLevel(std::string scriptPath, std::string displayName)
    : impl_(new Impl()),
      scriptPath_(std::move(scriptPath)),
      displayName_(std::move(displayName)) {}

ScriptedLevel::~ScriptedLevel() {
    // GL context is still current at level teardown (the runner resets the level
    // before shutting GL down), so release the buffers we allocated.
    for (auto& b : impl_->buffers)
        if (b) b->destroy();
}

RenderSettings ScriptedLevel::currentVis() const {
    RenderSettings vis = curCtx_->vis;
    if (hasSceneOverride_) vis = largeScene(vis, sceneFar_, sceneMaxFog_);
    if (depthOverride_ >= 0) vis.depth = (DepthCue)depthOverride_;
    return vis;
}

void ScriptedLevel::load() {
    sol::state& L = impl_->lua;
    L.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string,
                     sol::lib::table, sol::lib::os);
    lua::bindCoreTypes(L);
    bindEngineTypes(L);
    buildEngineTable();

    sol::protected_function_result loaded =
        L.safe_script_file(scriptPath_, sol::script_pass_on_error);
    if (!loaded.valid()) {
        sol::error err = loaded;
        std::fprintf(stderr, "[ScriptedLevel %s] failed to load %s: %s\n",
                     displayName_.c_str(), scriptPath_.c_str(), err.what());
        inert_  = true;
        loaded_ = true;
        return;
    }

    impl_->fnLoad     = L["load"];
    impl_->fnUpdate   = L["update"];
    impl_->fnRender   = L["render"];
    impl_->fnCheckWin = L["check_win"];
    impl_->fnHud      = L["render_hud"];
    impl_->fnInteract = L["on_interact"];

    if (!callHook(impl_->fnLoad, "load", displayName_)) inert_ = true;
    loaded_ = true;
}

void ScriptedLevel::update(const LevelContext& ctx) {
    if (inert_) return;
    curCtx_ = &ctx;
    if (standardInput_) runCameraInput(ctx);
    if (!callHook(impl_->fnUpdate, "update", displayName_)) inert_ = true;

    if (!inert_ && impl_->fnCheckWin.valid()) {
        sol::protected_function_result r = impl_->fnCheckWin();
        if (r.valid()) {
            won_ = won_ || r.get<bool>();
        } else {
            sol::error err = r;
            std::fprintf(stderr, "[ScriptedLevel %s] error in check_win(): %s\n",
                         displayName_.c_str(), err.what());
            inert_ = true;
        }
    }
    curCtx_ = nullptr;
}

void ScriptedLevel::render(const LevelContext& ctx) {
    if (inert_) return;
    curCtx_ = &ctx;
    if (!callHook(impl_->fnRender, "render", displayName_)) inert_ = true;
    curCtx_ = nullptr;
}

void ScriptedLevel::renderHUD(const LevelContext& ctx) {
    if (inert_) return;
    curCtx_ = &ctx;
    if (!callHook(impl_->fnHud, "render_hud", displayName_)) inert_ = true;
    curCtx_ = nullptr;
}

void ScriptedLevel::onInteract() {
    if (inert_) return;
    callHook(impl_->fnInteract, "on_interact", displayName_);
}

std::string ScriptedLevel::takeSceneRequest() {
    std::string req = pendingScene_;
    pendingScene_.clear();
    return req;
}

// ---------------------------------------------------------------------------
// The `engine` table — the full surface level scripts drive the engine through.
// ---------------------------------------------------------------------------
void ScriptedLevel::buildEngineTable() {
    sol::state& L = impl_->lua;
    sol::table engine = L.create_named_table("engine");

    // --- Shared state (bound by reference: scripts mutate the real objects) ------
    engine["cam4d"]    = &cam4D_;
    engine["cam3d"]    = &cam3D_;
    engine["avatar"]   = &avatar_;
    engine["body"]     = &body_;
    engine["world"]    = &world_;
    engine["controls"] = &controls_;

    engine["AxisLock"] = L.create_table_with(
        "NONE", (unsigned)AxisLock::NONE, "X", (unsigned)AxisLock::X,
        "Y", (unsigned)AxisLock::Y, "Z", (unsigned)AxisLock::Z,
        "W", (unsigned)AxisLock::W);

    // Allow-listed key codes (the keys the levels actually use). Scripts never see
    // raw GLFW integers; they index engine.keys by name.
    engine["keys"] = L.create_table_with(
        "E", GLFW_KEY_E, "A", GLFW_KEY_A, "Q", GLFW_KEY_Q, "D", GLFW_KEY_D,
        "W", GLFW_KEY_W, "S", GLFW_KEY_S, "J", GLFW_KEY_J, "O", GLFW_KEY_O,
        "U", GLFW_KEY_U, "L", GLFW_KEY_L, "I", GLFW_KEY_I, "K", GLFW_KEY_K,
        "H", GLFW_KEY_H, "SPACE", GLFW_KEY_SPACE, "TAB", GLFW_KEY_TAB);

    engine["hyper_mesh"] = kHyperMeshHandle;

    // --- Diagnostics / flags -----------------------------------------------------
    engine.set_function("log", [this](const std::string& msg) {
        std::printf("[lua %s] %s\n", displayName_.c_str(), msg.c_str());
    });
    engine.set_function("set_won", [this]() { won_ = true; });
    engine.set_function("use_standard_input", [this](bool on) { standardInput_ = on; });

    // Request a transition to another scene (scripts/scenes/<name>.lua). Only
    // records the name; the runner performs the swap after update() returns, so
    // the current scene tears down cleanly between frames.
    engine.set_function("goto_scene", [this](const std::string& name) {
        pendingScene_ = name;
    });

    engine.set_function("set_focal", [this](float f) { focal_ = f; });
    engine.set_function("get_focal", [this]() { return focal_; });

    // Run the standard 3D observer-camera input (the !inside_mode branch). Levels
    // that drive their own inside-mode movement (e.g. the plane's flight model) call
    // this from update() so the TAB-out observer view still works. Update-only.
    engine.set_function("observer_input", [this]() {
        if (curCtx_) cam3D_.processInput(curCtx_->window, curCtx_->dt);
    });

    engine.set_function("set_scene_far", [this](float far, float maxFog) {
        hasSceneOverride_ = true;
        sceneFar_    = far;
        sceneMaxFog_ = maxFog;
    });
    // Override the depth cue (e.g. "normal" turns the distance fog off so far
    // beacons stay vivid). Names match RenderSettings::DepthCue.
    engine.set_function("set_depth_cue", [this](const std::string& name) {
        if      (name == "fog")    depthOverride_ = (int)DepthCue::Fog;
        else if (name == "normal") depthOverride_ = (int)DepthCue::Normal;
        else if (name == "darken") depthOverride_ = (int)DepthCue::DarkenFar;
        else                       depthOverride_ = -1;
    });

    // Drive the avatar (second camera) + physics body with the shared FPS input
    // path, for third-person levels. Update-only. cam4D_ stays the follow cam.
    engine.set_function("drive_avatar", [this]() {
        if (curCtx_)
            avatar_.processInput(curCtx_->window, curCtx_->dt, body_, world_, controls_);
    });

    // --- Per-frame queries (valid only inside a hook) ----------------------------
    engine.set_function("dt", [this]() -> float {
        return curCtx_ ? curCtx_->dt : 0.0f;
    });
    engine.set_function("inside_mode", [this]() -> bool {
        return curCtx_ ? curCtx_->insideMode : false;
    });
    engine.set_function("time", [this]() -> float {
        return curCtx_ ? curCtx_->vis.time : 0.0f;  // accumulated seconds (for pulses)
    });
    engine.set_function("aspect", [this]() -> float {
        if (!curCtx_) return 1.0f;
        const glm::mat4& p = curCtx_->projection;
        return p[0][0] != 0.0f ? p[1][1] / p[0][0] : 1.0f;
    });
    engine.set_function("key_down", [this](int key) -> bool {
        if (!curCtx_) return false;
        return glfwGetKey(curCtx_->window, key) == GLFW_PRESS;
    });

    // --- Mesh creation (reuse primitives.h); returns an integer handle -----------
    auto addMesh = [this](Object4D mesh) -> int {
        int handle = (int)impl_->meshes.size();
        impl_->meshes.push_back(std::make_unique<Object4D>(std::move(mesh)));
        auto buf = std::make_unique<ObjectBuffer>();
        buf->init(*impl_->meshes.back());
        impl_->buffers.push_back(std::move(buf));
        return handle;
    };
    engine.set_function("make_box", [addMesh](const glm::vec4& half) {
        return addMesh(generateBox(half));
    });
    engine.set_function("make_ground", [addMesh](const glm::vec4& half) {
        return addMesh(generateGround(half));
    });
    engine.set_function("make_hypercube", [addMesh]() {
        return addMesh(generateHypercube());
    });
    engine.set_function("make_hypersphere", [addMesh](float radius) {
        return addMesh(generateHypersphere(radius));
    });
    engine.set_function("make_polyline", [addMesh](int points) {
        return addMesh(generatePolyline(points));
    });
    // Load a 4D mesh from a project JSON asset (e.g. "assets/tree.json").
    engine.set_function("load_mesh", [addMesh](const std::string& path) {
        Object4D m;
        m.loadFromJSON(path);
        return addMesh(std::move(m));
    });
    // Uniformly scale an existing mesh (all four axes) into a new mesh handle.
    engine.set_function("make_scaled", [this, addMesh](int handle, float f) {
        if (handle < 0 || handle >= (int)impl_->meshes.size())
            return -1;
        Object4D m = *impl_->meshes[handle];
        for (auto& v : m.vertices) v *= f;
        for (auto& d : m.hullD)    d *= f;
        return addMesh(std::move(m));
    });
    // Bake a set of placed instances of a base mesh into ONE static mesh (mesh_merge.h):
    // for non-occluding scenery (floors, forests) this collapses N draws into one.
    engine.set_function("make_merged",
        [this, addMesh](int baseHandle, InstanceSet& set, sol::optional<bool> occludes) {
            if (baseHandle < 0 || baseHandle >= (int)impl_->meshes.size())
                return -1;
            return addMesh(mergeInstances(*impl_->meshes[baseHandle], set.items,
                                          occludes.value_or(false)));
        });
    // Build a raw mesh from explicit geometry: vertices (list of vec4), triangles
    // (flat list of 0-based vertex indices), optional per-vertex colors (list of
    // vec3), and an occludes flag. Used for procedural surfaces (e.g. terrain).
    engine.set_function("make_mesh", [this, addMesh](sol::table spec) {
        Object4D m;
        m.name = "ScriptedMesh";
        m.occludes = spec.get_or("occludes", false);
        sol::table verts = spec["vertices"];
        size_t nv = verts.size();
        m.vertices.reserve(nv);
        for (size_t i = 1; i <= nv; ++i) m.vertices.push_back(verts.get<glm::vec4>(i));
        sol::optional<sol::table> colors = spec["colors"];
        if (colors) {
            m.vertexColors.reserve(nv);
            for (size_t i = 1; i <= nv; ++i)
                m.vertexColors.push_back(colors->get<glm::vec3>(i));
        }
        sol::optional<sol::table> tris = spec["triangles"];
        if (tris) {
            size_t nt = tris->size();
            m.triangleIndices.reserve(nt);
            for (size_t i = 1; i <= nt; ++i)
                m.triangleIndices.push_back((unsigned)tris->get<int>(i));
        }
        return addMesh(std::move(m));
    });

    engine.set_function("instance_set", []() { return InstanceSet{}; });

    // --- Assets (composite props loaded from assets/*.json) ----------------------
    // load_asset registers every part as a (mesh, buffer) and returns an asset handle.
    // Composite static props (bed, closet, ...) are drawn with draw_asset; single-part
    // props that the script instances dynamically (npc, ball, gold cube, plane) grab
    // the underlying mesh handle with asset_mesh.
    engine.set_function("load_asset", [this, addMesh](const std::string& path) -> int {
        Asset a;
        if (!loadAsset(path, a)) {
            luaL_error(impl_->lua.lua_state(), "load_asset: failed to load %s", path.c_str());
            return -1;
        }
        std::vector<Impl::AssetPartInst> parts;
        parts.reserve(a.parts.size());
        for (auto& p : a.parts) {
            glm::vec4 half(0.0f);
            for (const auto& v : p.mesh.vertices) half = glm::max(half, glm::abs(v));
            int h = addMesh(p.mesh);
            parts.push_back({h, p.pos, p.rot, p.colorA, p.colorB, half, p.solid});
        }
        int handle = (int)impl_->assets.size();
        impl_->assets.push_back(std::move(parts));
        return handle;
    });
    engine.set_function("asset_mesh", [this](int assetH, sol::optional<int> partIdx) -> int {
        if (assetH < 0 || assetH >= (int)impl_->assets.size()) return -1;
        const auto& parts = impl_->assets[assetH];
        int i = partIdx.value_or(0);
        if (i < 0 || i >= (int)parts.size()) return -1;
        return parts[i].meshHandle;
    });
    // Register solid-part colliders at a world placement (call in load()).
    engine.set_function("asset_colliders",
        [this](int assetH, const glm::vec4& pos, sol::optional<Math4D::Rotor4D> rot) {
            if (assetH < 0 || assetH >= (int)impl_->assets.size()) return;
            Math4D::Rotor4D R = rot.value_or(Math4D::Rotor4D::identity());
            for (const auto& p : impl_->assets[assetH]) {
                if (!p.solid) continue;
                world_.addBox(R.rotate(p.pos) + pos, p.half);
            }
        });

    // --- Drawing (render hook only) ---------------------------------------------
    auto drawInst = [this](int meshH, InstanceSet& set, InstanceSet* occ, int occH) {
        if (!curCtx_) {
            luaL_error(impl_->lua.lua_state(),
                       "engine.draw_instances() called outside render()");
            return;
        }
        Object4D*     mesh = nullptr;
        ObjectBuffer* buf  = nullptr;
        if (meshH == kHyperMeshHandle) {
            mesh = &curCtx_->hyperMesh;
            buf  = &curCtx_->hyperBuf;
        } else if (meshH >= 0 && meshH < (int)impl_->meshes.size()) {
            mesh = impl_->meshes[meshH].get();
            buf  = impl_->buffers[meshH].get();
        } else {
            luaL_error(impl_->lua.lua_state(), "draw_instances: bad mesh handle %d", meshH);
            return;
        }
        const std::vector<ObjectInstance>* occInsts = nullptr;
        const Object4D* occObj = nullptr;
        if (occ && occH >= 0 && occH < (int)impl_->meshes.size()) {
            occInsts = &occ->items;
            occObj   = impl_->meshes[occH].get();
        }
        RenderSettings vis = currentVis();
        Math4D::Rotor4D ori = cam4D_.getOrientation();
        curCtx_->renderer.drawObjects(set.items, *mesh, *buf, cam4D_, ori, focal_,
                                      curCtx_->innerMVP, vis, occInsts, occObj);
    };
    engine.set_function("draw_instances", sol::overload(
        [drawInst](int meshH, InstanceSet& set) { drawInst(meshH, set, nullptr, -1); },
        [drawInst](int meshH, InstanceSet& set, int occH, InstanceSet& occ) {
            drawInst(meshH, set, &occ, occH);
        }));

    // Draw a whole asset at a world placement. Each part renders exactly as the old
    // inline box did (its own self-occluding draw call), so behaviour is preserved.
    // Optional colorA/colorB tint every part (single-colour props); omit to use the
    // asset's baked per-part colours (composite props like the bed).
    engine.set_function("draw_asset",
        [this](int assetH, const glm::vec4& pos, sol::optional<Math4D::Rotor4D> rot,
               sol::optional<glm::vec3> cA, sol::optional<glm::vec3> cB) {
            if (!curCtx_) {
                luaL_error(impl_->lua.lua_state(),
                           "engine.draw_asset() called outside render()");
                return;
            }
            if (assetH < 0 || assetH >= (int)impl_->assets.size()) {
                luaL_error(impl_->lua.lua_state(), "draw_asset: bad asset handle %d", assetH);
                return;
            }
            Math4D::Rotor4D R = rot.value_or(Math4D::Rotor4D::identity());
            RenderSettings vis = currentVis();
            Math4D::Rotor4D camOri = cam4D_.getOrientation();
            for (const auto& p : impl_->assets[assetH]) {
                ObjectInstance inst;
                inst.pos         = R.rotate(p.pos) + pos;
                inst.orientation = R * p.rot;
                inst.colorA      = cA.value_or(p.colorA);
                inst.colorB      = cB.value_or(p.colorB);
                std::vector<ObjectInstance> one{inst};
                curCtx_->renderer.drawObjects(one, *impl_->meshes[p.meshHandle],
                                              *impl_->buffers[p.meshHandle], cam4D_, camOri,
                                              focal_, curCtx_->innerMVP, vis, nullptr, nullptr);
            }
        });

    engine.set_function("draw_polyline",
        [this](int meshH, sol::table pts, const glm::vec3& color, float width) {
            if (!curCtx_) {
                luaL_error(impl_->lua.lua_state(),
                           "engine.draw_polyline() called outside render()");
                return;
            }
            if (meshH < 0 || meshH >= (int)impl_->meshes.size()) {
                luaL_error(impl_->lua.lua_state(), "draw_polyline: bad mesh handle %d", meshH);
                return;
            }
            Object4D&     mesh = *impl_->meshes[meshH];
            ObjectBuffer& buf  = *impl_->buffers[meshH];
            size_t n = pts.size();
            if (n < 2) return;
            mesh.vertices.assign(n, glm::vec4(0.0f));
            for (size_t i = 0; i < n; ++i)
                mesh.vertices[i] = pts[i + 1];  // Lua arrays are 1-based
            RenderSettings vis = currentVis();
            Math4D::Rotor4D ori = cam4D_.getOrientation();
            curCtx_->renderer.drawPolyline(mesh, buf, cam4D_, ori, focal_,
                                           curCtx_->innerMVP, vis, color, width);
        });

    engine.set_function("draw_outer_cube", [this]() {
        if (!curCtx_) {
            luaL_error(impl_->lua.lua_state(),
                       "engine.draw_outer_cube() called outside render()");
            return;
        }
        curCtx_->renderer.drawOuterCube(curCtx_->outerMVP);
    });

    // Draw a screen-facing dot at a 3D point projected with the inner-scene MVP
    // (constant on-screen size). Used for direction hints on the inner cube face.
    engine.set_function("draw_marker",
        [this](const glm::vec3& center, float sizeNDC, const glm::vec3& color, float alpha) {
            if (!curCtx_) return;
            float aspect = 1.0f;
            const glm::mat4& p = curCtx_->projection;
            if (p[0][0] != 0.0f) aspect = p[1][1] / p[0][0];
            curCtx_->renderer.drawMarker(center, sizeNDC, aspect, color, alpha,
                                         curCtx_->innerMVP);
        });

    // --- HUD widgets (render_hud hook only) -------------------------------------
    engine.set_function("hud_window", [](const std::string& title, sol::table lines) {
        ImGui::SetNextWindowPos(ImVec2(140, 10), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(460, 180), ImGuiCond_Always);
        ImGui::Begin(title.c_str(), nullptr,
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoTitleBar);
        for (size_t i = 1; i <= lines.size(); ++i)
            ImGui::TextWrapped("%s", lines.get<std::string>(i).c_str());
        ImGui::End();
    });
    engine.set_function("hud_facing", [](const glm::vec4& forward) {
        hud::drawFacingWidget(forward);
    });
    engine.set_function("hud_coord", [](const glm::vec4& pos, const std::string& label) {
        hud::drawCoordWidget(pos, label.c_str());
    });

    // --- Minimap module (opt-in component; "trace" is the only option for now) ---
    engine.set_function("use_minimap", [this](sol::table opts) {
        impl_->minimap = std::make_unique<Minimap>();
        impl_->minimapTrace = opts.get_or("trace", false);
    });
    // Append the player's map-space position to the trace (call from update). No-op
    // unless tracing is enabled. Mirrors the maze's 0.3-unit sampling tolerance.
    engine.set_function("minimap_trace", [this](const glm::vec3& mapPos) {
        if (!impl_->minimap || !impl_->minimapTrace) return;
        if (impl_->trail.empty() ||
            glm::distance(mapPos, impl_->trail.back()) > 0.3f)
            impl_->trail.push_back(mapPos);
    });
    // Draw the minimap (call from render). player/facing/bounds are in MAP space
    // (caller maps world (x,z,w) -> (x, w, z); W is up).
    engine.set_function("minimap_draw",
        [this](const glm::vec3& player, const glm::vec3& facing,
               const glm::vec3& anchor, float anchorHalf,
               const glm::vec3& boundsMin, const glm::vec3& boundsMax) {
            if (!impl_->minimap || !curCtx_) return;
            float aspect = 1.0f;
            const glm::mat4& p = curCtx_->projection;
            if (p[0][0] != 0.0f) aspect = p[1][1] / p[0][0];
            impl_->minimap->drawInWorld(curCtx_->renderer, curCtx_->outerMVP, aspect,
                                        impl_->trail, boundsMin, boundsMax,
                                        player, facing, anchor, anchorHalf);
        });
}
