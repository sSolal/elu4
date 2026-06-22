// 4D asset visualizer — a side tool that shares the game's rendering pipeline.
//
// It loads an asset from the shared library (assets/*.json), renders every part with
// the SAME Renderer::drawObjects path the game uses (4D occlusion, depth cues, the
// W-gradient inner shader) under the SAME Elua UI theme, and lets you orbit AROUND
// the object: the camera always looks at it and slides over the 4-sphere that
// surrounds it. u/l, j/o, i/k rotate the orbit; a camera-angle widget on the right
// has eight "face from ±axis" snap buttons.
//
// Run from the project root (so shaders/ and assets/ resolve), e.g. ./build/visualizer

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include "Asset.h"
#include "Camera.h"
#include "CameraWidget.h"
#include "Math4D.h"
#include "Object4D.h"
#include "ObjectBuffer.h"
#include "Renderer.h"
#include "RenderSettings.h"
#include "Scene.h"
#include "UiTheme.h"

extern "C" {
#include "third_party/tinyfiledialogs.h"
}

using Math4D::Rotor4D;

namespace {

constexpr float kPi = 3.14159265358979f;

// A loaded asset ready to render: the parts (geometry) + one GPU buffer and one placed
// instance per part, with the bounding sphere used to frame the orbit camera.
struct VizAsset {
    Asset                                      asset;   // owns the per-part Object4D meshes
    std::vector<std::unique_ptr<ObjectBuffer>> buffers;
    std::vector<ObjectInstance>                insts;
    glm::vec4 center{0.0f};
    float     radius = 1.0f;
    bool      valid  = false;
};

VizAsset buildViz(Asset a) {
    VizAsset v;
    v.asset = std::move(a);
    glm::vec4 lo(1e9f), hi(-1e9f);
    for (const auto& p : v.asset.parts) {
        auto buf = std::make_unique<ObjectBuffer>();
        buf->init(p.mesh);
        v.buffers.push_back(std::move(buf));
        // The asset sits at the world origin; the part's local transform IS its world
        // placement (instance = orientation then + pos, as in Level2.cpp).
        ObjectInstance inst;
        inst.pos = p.pos;
        inst.orientation = p.rot;
        inst.colorA = p.colorA;
        inst.colorB = p.colorB;
        v.insts.push_back(inst);
        for (const auto& vert : p.mesh.vertices) {
            glm::vec4 w = p.rot.rotate(vert) + p.pos;
            lo = glm::min(lo, w);
            hi = glm::max(hi, w);
        }
    }
    if (!v.asset.parts.empty()) {
        v.center = (lo + hi) * 0.5f;
        v.radius = 0.0f;
        for (const auto& p : v.asset.parts)
            for (const auto& vert : p.mesh.vertices)
                v.radius = std::max(v.radius, glm::length(p.rot.rotate(vert) + p.pos - v.center));
        if (v.radius < 1e-3f) v.radius = 1.0f;
        v.valid = true;
    }
    return v;
}

// Orbit orientation that places the camera on a given face axis looking at the object.
// camOffsetDir = Q.reverse() * (+W); the eight faces invert that to a fixed rotor.
Rotor4D faceRotor(int faceIndex) {
    switch (faceIndex) {
        case 0: return Rotor4D::fromXW(-kPi * 0.5f);  // +X
        case 1: return Rotor4D::fromXW( kPi * 0.5f);  // -X
        case 2: return Rotor4D::fromYW(-kPi * 0.5f);  // +Y
        case 3: return Rotor4D::fromYW( kPi * 0.5f);  // -Y
        case 4: return Rotor4D::fromZW(-kPi * 0.5f);  // +Z
        case 5: return Rotor4D::fromZW( kPi * 0.5f);  // -Z
        case 6: return Rotor4D::identity();           // +W
        default:return Rotor4D::fromZW( kPi);         // -W
    }
}

bool keyDown(GLFWwindow* w, int k) { return glfwGetKey(w, k) == GLFW_PRESS; }

// Dump the default framebuffer to a binary PPM (no image library needed). Mirrors the
// game's SCREENSHOT path; handy for headless verification of a frame.
void writePPM(const char* path, int w, int h) {
    std::vector<unsigned char> px((size_t)w * h * 3);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px.data());
    if (FILE* f = std::fopen(path, "wb")) {
        std::fprintf(f, "P6\n%d %d\n255\n", w, h);
        for (int y = h - 1; y >= 0; --y) std::fwrite(&px[(size_t)y * w * 3], 1, (size_t)w * 3, f);
        std::fclose(f);
    }
}

// Settings toolbar: returns nothing; mutates vis/focal/dist and triggers Open.
void segmented(const char* label, int* value, const char* const* opts, int n) {
    ImGui::TextUnformatted(label);
    ImGui::SameLine(120.0f);
    for (int i = 0; i < n; ++i) {
        if (i) ImGui::SameLine();
        bool on = (*value == i);
        if (on) {
            ImGui::PushStyleColor(ImGuiCol_Button, elua::colLamp());
            ImGui::PushStyleColor(ImGuiCol_Text, elua::colNight800());
        }
        ImGui::PushID(i);
        if (ImGui::Button(opts[i])) *value = i;
        ImGui::PopID();
        if (on) ImGui::PopStyleColor(2);
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);  // GLEW needs GLX (see game)
    if (!glfwInit()) { std::fprintf(stderr, "Failed to init GLFW\n"); return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 800, "4D Asset Visualizer", nullptr, nullptr);
    if (!window) { std::fprintf(stderr, "Failed to create window\n"); glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) { std::fprintf(stderr, "Failed to init GLEW\n"); return -1; }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // ImGui + the shared Elua look (same style/fonts as the game).
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    elua::applyEluaStyle();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    elua::loadEluaFonts();

    Renderer renderer;            // loads shaders/inner.* etc. (run from project root)
    RenderSettings vis;
    vis.bg = 1;                   // dusk backdrop suits the lamplit theme
    Camera3D view3D;              // fixed 3D observer; the object always projects to origin
    Camera4D cam;                 // only cam.pos is read by drawObjects

    Rotor4D orbit = Rotor4D::identity();
    float focal = 2.0f;
    float dist  = 4.0f;

    VizAsset current;
    auto loadPath = [&](const std::string& path) {
        Asset a;
        if (!loadAsset(path, a)) { tinyfd_messageBox("Load failed",
                                       ("Could not load:\n" + path).c_str(), "ok", "error", 1);
            return; }
        for (auto& b : current.buffers) if (b) b->destroy();   // free old GPU buffers
        current = buildViz(std::move(a));
        if (current.valid) {
            dist  = std::max(2.0f, current.radius * 2.2f + 1.0f);  // frame the bound sphere
            orbit = Rotor4D::identity();
        }
    };
    if (argc > 1) loadPath(argv[1]);

    // Optional headless capture: SCREENSHOT=<path> dumps a frame after SHOT_FRAME frames.
    const char* shotPath  = getenv("SCREENSHOT");
    const int   shotFrame = getenv("SHOT_FRAME") ? atoi(getenv("SHOT_FRAME")) : 60;
    long long   frameNo   = 0;

    double lastTime = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float dt = (float)(now - lastTime);
        lastTime = now;
        glfwPollEvents();
        vis.time += dt;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        const bool uiWantsKbd = ImGui::GetIO().WantCaptureKeyboard;
        const bool uiWantsMouse = ImGui::GetIO().WantCaptureMouse;

        // --- Orbit input: u/l (ZW), j/o (XW), i/k (YW); always re-aimed at object ---
        if (!uiWantsKbd) {
            float a = 1.6f * dt;
            if (keyDown(window, GLFW_KEY_U)) orbit = orbit * Rotor4D::fromZW( a);
            if (keyDown(window, GLFW_KEY_L)) orbit = orbit * Rotor4D::fromZW(-a);
            if (keyDown(window, GLFW_KEY_J)) orbit = orbit * Rotor4D::fromXW( a);
            if (keyDown(window, GLFW_KEY_O)) orbit = orbit * Rotor4D::fromXW(-a);
            if (keyDown(window, GLFW_KEY_I)) orbit = orbit * Rotor4D::fromYW( a);
            if (keyDown(window, GLFW_KEY_K)) orbit = orbit * Rotor4D::fromYW(-a);
            if (keyDown(window, GLFW_KEY_ESCAPE)) glfwSetWindowShouldClose(window, true);
        }
        orbit.normalize();
        if (!uiWantsMouse) {
            float wheel = ImGui::GetIO().MouseWheel;
            if (wheel != 0.0f) dist = glm::clamp(dist - wheel * 0.4f, 0.3f, 60.0f);
        }

        // Camera sits on the 4-sphere around the object's centre, looking inward.
        cam.pos = current.center + orbit.reverse().rotate(glm::vec4(0, 0, 0, dist));
        glm::vec4 offsetDir = orbit.reverse().rotate(glm::vec4(0, 0, 0, 1));
        if (glm::length(offsetDir) > 1e-5f) offsetDir = glm::normalize(offsetDir);

        // --- Render ---
        int fbW, fbH;
        glfwGetFramebufferSize(window, &fbW, &fbH);
        glViewport(0, 0, fbW, fbH);
        glm::vec3 bg = vis.bgColor();
        glClearColor(bg.r, bg.g, bg.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float aspect = fbH > 0 ? (float)fbW / (float)fbH : 1.0f;
        glm::mat4 innerMVP = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f)
                           * view3D.getViewMatrix();

        if (current.valid) {
            // Each part is drawn with its own self-occluding call — exactly how the
            // game draws each inline box — through the shared Renderer pipeline.
            for (size_t i = 0; i < current.insts.size(); ++i) {
                std::vector<ObjectInstance> one{current.insts[i]};
                renderer.drawObjects(one, current.asset.parts[i].mesh, *current.buffers[i],
                                     cam, orbit, focal, innerMVP, vis, nullptr, nullptr);
            }
        }

        // --- ImGui overlays ---
        // Toolbar
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(360, 0), ImGuiCond_FirstUseEver);
        ImGui::Begin("Asset", nullptr, ImGuiWindowFlags_NoCollapse);
        if (ImGui::Button("Open asset...", ImVec2(-1, 0))) {
            const char* filters[1] = {"*.json"};
            const char* picked = tinyfd_openFileDialog("Open 4D asset", "assets/", 1,
                                                       filters, "JSON assets", 0);
            if (picked) loadPath(picked);
        }
        ImGui::Separator();
        if (current.valid) {
            ImGui::Text("%s", current.asset.name.c_str());
            ImGui::TextDisabled("%d parts", (int)current.insts.size());
        } else {
            ImGui::TextDisabled("No asset loaded — Open one from assets/.");
        }
        ImGui::Separator();
        static const char* geomOpts[3]  = {"Solid", "Wire", "Borders"};
        static const char* alphaOpts[4] = {"Mid", "Light", "Heavy", "Glass"};
        static const char* depthOpts[3] = {"Fog", "Flat", "Dark"};
        static const char* bgOpts[3]    = {"Sky", "Dusk", "Night"};
        int geom = (int)vis.geom, alpha = (int)vis.alpha, depth = (int)vis.depth, bgv = vis.bg;
        segmented("Geometry", &geom, geomOpts, 3);   vis.geom = (GeomMode)geom;
        segmented("Alpha", &alpha, alphaOpts, 4);    vis.alpha = (AlphaMode)alpha;
        segmented("Depth", &depth, depthOpts, 3);    vis.depth = (DepthCue)depth;
        segmented("Background", &bgv, bgOpts, 3);    vis.bg = bgv;
        ImGui::Checkbox("4D occlusion", &vis.occlude4D);
        ImGui::SliderFloat("Focal", &focal, 0.5f, 8.0f, "%.2f");
        ImGui::SliderFloat("Distance", &dist, 0.5f, 40.0f, "%.2f");
        ImGui::Separator();
        ImGui::TextDisabled("Orbit: u/l  j/o  i/k   |   wheel: zoom");
        ImGui::End();

        // Camera-angle widget with the 8 face buttons.
        int face = camwidget::draw(offsetDir, vis.time);
        if (face >= 0) orbit = faceRotor(face);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (shotPath && frameNo == shotFrame) {
            writePPM(shotPath, fbW, fbH);
            glfwSetWindowShouldClose(window, true);
        }
        ++frameNo;
        glfwSwapBuffers(window);
    }

    for (auto& b : current.buffers) if (b) b->destroy();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
    return 0;
}
