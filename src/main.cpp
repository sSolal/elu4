#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <memory>
#include <vector>
#include <string>
#include "GameState.h"
#include "Menu.h"
#include "MenuBackground.h"
#include "UiTheme.h"
#include "primitives.h"
#include "ObjectBuffer.h"
#include "Renderer.h"
#include "Level.h"
#include "LevelRegistry.h"
#include "RenderSettings.h"
#include "VR.h"
#include "VRPanel.h"
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

const unsigned int SCR_WIDTH = 1920;
const unsigned int SCR_HEIGHT = 1080;

// Small read-only legend listing every visualization toggle and its state. The
// desktop path uses the interactive drawSettings() overlay instead; this remains
// for the VR panel, which has no mouse to drive the overlay.
static void drawLegend(const RenderSettings& vis) {
    static const char* kGeom[]  = {"Solid", "Wireframe", "Solid + borders"};
    static const char* kOsc[]   = {"Static", "Horizontal", "Circle", "Random"};
    static const char* kDepth[] = {"Fog (near vivid)", "Normal", "Darken far"};
    static const char* kAlpha[] = {"Mid 0.35", "Light 0.2", "Heavy 0.85", "Near-transparent"};
    static const char* kBg[]    = {"Warm white", "Deep blue", "Black"};
    static const char* kPulse[] = {"off", "Sine (slow)", "Noise"};
    static const char* kAid[]   = {"None", "Vignette", "Reflection"};

    const ImVec2 disp = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(10.0f, disp.y - 194.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(232.0f, 184.0f), ImGuiCond_Always);
    ImGui::Begin("Legend", nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);
    ImGui::Text("P  Geometry: %s", kGeom[(int)vis.geom]);
    ImGui::Text("M  Cam sway: %s", kOsc[(int)vis.osc]);
    ImGui::Text("N  Depth:    %s", kDepth[(int)vis.depth]);
    ImGui::Text("B  Backgnd:  %s", kBg[vis.bg]);
    ImGui::Text("F  Alpha:    %s", kAlpha[(int)vis.alpha]);
    ImGui::Text("V  X-ray pulse: %s", kPulse[(int)vis.pulse]);
    ImGui::Text("T  Depth aid: %s", kAid[(int)vis.depthAid]);
    ImGui::Text("X  4D occlude: %s", vis.occlude4D ? "On" : "Off");
    ImGui::End();
}

// One row of a "tri-selector": `count` buttons side by side, the active one
// highlighted. Returns true and writes the picked index into `value` on a click.
static bool segmented(const char* id, const char* const* opts, int count, int& value) {
    bool changed = false;
    ImGui::PushID(id);
    for (int i = 0; i < count; ++i) {
        if (i > 0) ImGui::SameLine();
        const bool active = (i == value);
        if (active) {
            // Lamp amber for the selected segment — Elua's accent, not ImGui blue.
            const ImVec4 lamp = elua::colLamp();
            ImGui::PushStyleColor(ImGuiCol_Button,        lamp);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, lamp);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  lamp);
            ImGui::PushStyleColor(ImGuiCol_Text,          elua::colNight800());
        }
        ImGui::PushID(i);
        if (ImGui::Button(opts[i])) { value = i; changed = true; }
        ImGui::PopID();
        if (active) ImGui::PopStyleColor(4);
    }
    ImGui::PopID();
    return changed;
}

// Settings overlay: visual controls for every visualization setting, each labelled
// with its Ctrl+key shortcut. Opened from the in-level Settings button; `open` is
// cleared when the player presses Close. Replaces the old bottom-left legend.
static void drawSettings(RenderSettings& vis, bool& open) {
    static const char* kGeom[]  = {"Solid", "Wireframe", "Solid + borders"};
    static const char* kOsc[]   = {"Static", "Horizontal", "Circle", "Random"};
    static const char* kDepth[] = {"Fog (near vivid)", "Normal", "Darken far"};
    static const char* kAlpha[] = {"Mid 0.35", "Light 0.2", "Heavy 0.85", "Near-transparent"};
    static const char* kBg[]    = {"Warm white", "Deep blue", "Black"};
    static const char* kPulse[] = {"Off", "Sine (slow)", "Noise"};
    static const char* kAid[]   = {"None", "Vignette", "Reflection"};

    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(560, 0), ImGuiCond_Always);
    ImGui::Begin("Settings", nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);

    ImGui::Text("Settings");
    ImGui::TextDisabled("Shortcuts: hold Ctrl and press the key shown.");
    ImGui::Separator();

    // Each row: a left-aligned name in a fixed column, the control, then the
    // Ctrl+key shortcut right-aligned at the window edge.
    auto label = [](const char* name) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(name);
        ImGui::SameLine();
        ImGui::SetCursorPosX(110.0f);
    };
    auto shortcut = [](const char* keys) {
        ImGui::SameLine();
        const float x = ImGui::GetWindowWidth() - ImGui::CalcTextSize(keys).x - 16.0f;
        if (x > ImGui::GetCursorPosX()) ImGui::SetCursorPosX(x);
        ImGui::TextDisabled("%s", keys);
    };

    int v;
    label("Geometry");    v = (int)vis.geom;     if (segmented("geom",  kGeom,  3, v)) vis.geom     = (GeomMode)v;  shortcut("Ctrl+P");
    label("Cam sway");    v = (int)vis.osc;      if (segmented("osc",   kOsc,   4, v)) vis.osc      = (CamOsc)v;    shortcut("Ctrl+M");
    label("Depth cue");   v = (int)vis.depth;    if (segmented("depth", kDepth, 3, v)) vis.depth    = (DepthCue)v;  shortcut("Ctrl+N");
    label("Background");  v = vis.bg;            if (segmented("bg",    kBg,    3, v)) vis.bg       = v;            shortcut("Ctrl+B");
    label("Alpha");       v = (int)vis.alpha;    if (segmented("alpha", kAlpha, 4, v)) vis.alpha    = (AlphaMode)v; shortcut("Ctrl+F");
    label("X-ray pulse"); v = (int)vis.pulse;    if (segmented("pulse", kPulse, 3, v)) vis.pulse    = (PulseMode)v; shortcut("Ctrl+V");
    label("Depth aid");   v = (int)vis.depthAid; if (segmented("aid",   kAid,   3, v)) vis.depthAid = (DepthAid)v;  shortcut("Ctrl+T");
    label("4D occlude");  ImGui::Checkbox("##occlude", &vis.occlude4D);                                             shortcut("Ctrl+X");

    ImGui::Separator();
    if (ImGui::Button("Close", ImVec2(100, 32)))
        open = false;

    ImGui::End();
}

// One eye to render this frame: a viewport rectangle plus the view/projection
// matrices for it. In Phase 2 (VR) these come straight from OpenXR's
// xrLocateViews; here buildDesktopEyes() synthesizes them so the per-eye
// rendering path can be exercised on the desktop with no headset attached.
struct EyeView {
    int x, y, w, h;     // viewport in framebuffer pixels
    glm::mat4 view;
    glm::mat4 proj;
};

// fakeStereo=false → a single full-window mono eye (pixel-identical to the
// original one-view path). true → two half-width eyes with a small lateral
// separation, the desktop stand-in for VR stereo (left half = left eye).
static std::vector<EyeView> buildDesktopEyes(int fbW, int fbH,
                                             const glm::mat4& baseView,
                                             bool fakeStereo) {
    const float zNear = 0.1f, zFar = 100.0f, fovY = glm::radians(45.0f);
    if (!fakeStereo) {
        float aspect = fbH > 0 ? (float)fbW / (float)fbH : 1.0f;
        return {{0, 0, fbW, fbH, baseView,
                 glm::perspective(fovY, aspect, zNear, zFar)}};
    }
    // A lateral eye offset in world space is a translation in view space along
    // its X axis: shift the eye right by d ⇔ translate the world by -d in X.
    const float sep = 0.15f;                 // world-unit eye separation
    const int   hw  = fbW / 2;
    const float aspect = fbH > 0 ? (float)hw / (float)fbH : 1.0f;
    const glm::mat4 proj = glm::perspective(fovY, aspect, zNear, zFar);
    glm::mat4 lView = glm::translate(glm::mat4(1.0f), glm::vec3( sep * 0.5f, 0, 0)) * baseView;
    glm::mat4 rView = glm::translate(glm::mat4(1.0f), glm::vec3(-sep * 0.5f, 0, 0)) * baseView;
    return {{0,  0, hw,      fbH, lView, proj},
            {hw, 0, fbW - hw, fbH, rView, proj}};
}

int main(int argc, char** argv) {
    // --fake-stereo: render two side-by-side eyes on the desktop (a VR-stereo
    // stand-in) instead of one mono view. Default (no flag) is mono, which is
    // pixel-identical to the original single-view behaviour.
    bool fakeStereo = false;
    bool vrMode = false;        // --vr: render to an OpenXR headset (Route A)
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--fake-stereo") fakeStereo = true;
        if (std::string(argv[i]) == "--vr")          vrMode = true;
    }

    // Init GLFW
    // Force the X11 backend: GLEW queries OpenGL through GLX, which a native
    // Wayland/EGL context doesn't provide, so glewInit() fails on Wayland
    // sessions. XWayland supplies GLX, so this works everywhere.
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    // Create window
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Elua", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    // Vsync on by default; NO_VSYNC=1 uncaps the framerate (dev: measure true
    // per-frame cost / headroom instead of being pinned to the refresh rate).
    glfwSwapInterval(getenv("NO_VSYNC") ? 0 : 1);

    // Init GLEW
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return -1;
    }

    glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CLIP_DISTANCE0);
    glEnable(GL_CLIP_DISTANCE1);
    glEnable(GL_CLIP_DISTANCE2);
    glEnable(GL_CLIP_DISTANCE3);
    glEnable(GL_CLIP_DISTANCE4);
    glEnable(GL_CLIP_DISTANCE5);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    elua::applyEluaStyle();              // Elua's lamplit theme (replaces dark default)

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    elua::loadEluaFonts();               // Fraunces (wordmark) + Public Sans (body)

    // Create game systems
    GameState state = GameState::MENU;
    Menu menu;
    // Shared hypercube mesh for marker/goal cubes, on the single tetrahedral render
    // path. Sized to the historical Tesseract::HS so markers keep their look.
    Object4D hyperMesh = generateBox(glm::vec4(kHyperHalf));
    ObjectBuffer hyperBuf;
    hyperBuf.init(hyperMesh);
    Renderer renderer;
    MenuBackground menuBg;   // animated lamplit title-screen background

    // The active level (null in the menu). Constructed lazily from the registry
    // when the player picks a level; reset() on Back / win frees its resources.
    std::unique_ptr<Level> level;
    bool insideMode = true;      // TAB: 3D observer vs 4D FPS — default to 4D FPS
    bool tabWasPressed = false;
    bool settingsOpen = false;   // in-level Settings overlay; freezes gameplay input

    // F11 toggles fullscreen; remember the windowed placement so we can restore it.
    bool f11Was = false;
    int savedX = 0, savedY = 0, savedW = (int)SCR_WIDTH, savedH = (int)SCR_HEIGHT;

    // Global visualization toggles (P/M/N/B/F/V/G), with per-key edge-detection.
    RenderSettings vis;
    // OCC_OFF=1 : start with 4D hidden-surface removal disabled (Ctrl+X still toggles
    // it live). The per-fragment occlusion pass dominates GPU cost, so pairing this
    // with NO_VSYNC isolates how much of the frame it accounts for.
    if (getenv("OCC_OFF")) vis.occlude4D = false;
    bool pWas = false, mWas = false, nWas = false, bWas = false,
         fWas = false, vWas = false, tWas = false, xWas = false;

    float deltaTime = 0.0f;
    float lastFrame = 0.0f;

#ifdef HAVE_OPENXR
    // ---- VR path (Route A) ----------------------------------------------
    // Drives the frame loop from OpenXR. The 4D scene-box floats in the room;
    // the menu and in-level HUD render to an offscreen ImGui canvas shown on a
    // world-space panel so they're visible in the headset. ImGui's keyboard
    // navigation (arrows + Enter) drives the menu; the in-level keyboard does
    // everything it does on the desktop. The flat window mirrors the left eye.
    if (vrMode) {
        VRSystem vr;
        if (!vr.init(window)) {
            std::cerr << "[VR] init failed — is the WiVRn server running and the "
                         "headset connected? Falling back to desktop.\n";
        } else {
            VRPanel panel(1280, 800);
            ImGuiIO& vio = ImGui::GetIO();

            GameState vrState = GameState::MENU;
            glm::mat4 obsV0(1.0f);   // observer-camera view captured at level entry

            // Edge-detect helper + per-key state (mirrors the desktop hotkeys).
            bool escWas = false, tabWas = false, backWas = false;
            bool pW=false,mW=false,nW=false,bW=false,fW=false,vW=false,tW=false,xW=false;
            auto edge = [&](int key, bool& was) {
                bool now = glfwGetKey(window, key) == GLFW_PRESS;
                bool fired = now && !was; was = now; return fired;
            };

            std::vector<VREye> eyes;
            while (!glfwWindowShouldClose(window) && vr.running()) {
                float currentFrame = (float)glfwGetTime();
                deltaTime = currentFrame - lastFrame;
                lastFrame = currentFrame;

                glfwPollEvents();
                vr.pollEvents();

                // Esc: leave a level back to the menu, or quit from the menu.
                if (edge(GLFW_KEY_ESCAPE, escWas)) {
                    if (vrState == GameState::IN_LEVEL) { level.reset(); vrState = GameState::MENU; }
                    else break;
                }

                // ImGui keyboard nav drives the menu; off in-level so the arrow
                // keys stay free for focal-length control.
                if (vrState == GameState::MENU) vio.ConfigFlags |=  ImGuiConfigFlags_NavEnableKeyboard;
                else                            vio.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;

                // In-level hotkeys + update (same controls as the desktop loop).
                if (vrState == GameState::IN_LEVEL && level) {
                    if (edge(GLFW_KEY_TAB, tabWas))       insideMode = !insideMode;
                    if (edge(GLFW_KEY_BACKSPACE, backWas)) { level.reset(); vrState = GameState::MENU; }
                }
                if (vrState == GameState::IN_LEVEL && level) {
                    if (edge(GLFW_KEY_P,pW)) vis.geom =(GeomMode)(((int)vis.geom +1)%3);
                    if (edge(GLFW_KEY_M,mW)) vis.osc  =(CamOsc)(((int)vis.osc  +1)%4);
                    if (edge(GLFW_KEY_N,nW)) vis.depth=(DepthCue)(((int)vis.depth+1)%3);
                    if (edge(GLFW_KEY_B,bW)) vis.bg   =(vis.bg+1)%3;
                    if (edge(GLFW_KEY_F,fW)) vis.alpha=(AlphaMode)(((int)vis.alpha+1)%4);
                    if (edge(GLFW_KEY_V,vW)) vis.pulse=(PulseMode)(((int)vis.pulse+1)%3);
                    if (edge(GLFW_KEY_T,tW)) vis.depthAid=(DepthAid)(((int)vis.depthAid+1)%3);
                    if (edge(GLFW_KEY_X,xW)) vis.occlude4D = !vis.occlude4D;
                    if (glfwGetKey(window,GLFW_KEY_UP)==GLFW_PRESS)
                        level->focalLength()=glm::min(10.0f, level->focalLength()+2.0f*deltaTime);
                    if (glfwGetKey(window,GLFW_KEY_DOWN)==GLFW_PRESS)
                        level->focalLength()=glm::max(0.1f, level->focalLength()-2.0f*deltaTime);

                    level->cam3D().oscMode = 0;  // head motion is the parallax in VR — no sway
                    LevelContext uctx{window, renderer, hyperMesh, hyperBuf, deltaTime, insideMode,
                                      glm::mat4(1.0f), glm::mat4(1.0f), glm::mat4(1.0f), vis};
                    level->update(uctx);
                    vis.time += deltaTime;
                    if (level->checkWin()) { level.reset(); vrState = GameState::MENU; }
                }

                // ---- Build the ImGui canvas for this frame (menu or HUD) ----
                ImGui_ImplOpenGL3_NewFrame();
                ImGui_ImplGlfw_NewFrame();
                vio.DisplaySize = ImVec2((float)panel.width(), (float)panel.height());
                ImGui::NewFrame();
                if (vrState == GameState::MENU) {
                    int sel = menu.renderMainMenu();
                    if (sel >= 0) {
                        level = levelRegistry()[sel].factory();
                        level->load();
                        insideMode = false;
                        level->cam3D().oscMode = 0;
                        obsV0 = level->cam3D().getViewMatrix();
                        vrState = GameState::IN_LEVEL;
                        std::cout << "[VR] entered level: " << level->name() << std::endl;
                    }
                } else if (level) {
                    // worldHUD=true so levels skip screen-space 3D overlays (the
                    // direction dot) here — those are drawn in 3D per eye below.
                    LevelContext hctx{window, renderer, hyperMesh, hyperBuf, deltaTime, insideMode,
                                      glm::mat4(1.0f), glm::mat4(1.0f), glm::mat4(1.0f), vis, true};
                    level->renderHUD(hctx);
                    drawLegend(vis);
                }
                ImGui::Render();
                panel.bindForImGui();
                ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
                glDisable(GL_SCISSOR_TEST);  // defensive: ImGui leaves scissor rects

                // ---- Render the eyes ----
                if (vr.beginFrame(eyes)) {
                    glm::mat4 room = vr.roomTransform();
                    // Observer camera "moves the box": fold the keyboard-driven
                    // Camera3D view in as a manipulation of the box, anchored to
                    // start neutral at level entry. (Composition sign tunable.)
                    glm::mat4 boxModel = room;
                    if (vrState == GameState::IN_LEVEL && level)
                        boxModel = room * level->cam3D().getViewMatrix() * glm::inverse(obsV0);

                    // Panel placement. Menu: big & centred in front. In-level:
                    // hung from the cube's bottom edge and following it, so it's
                    // always reachable yet never covers the cube wherever you put it.
                    glm::mat4 panelModel;
                    if (vrState == GameState::MENU) {
                        panelModel = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.05f, -1.4f));
                        panelModel = glm::scale(panelModel, glm::vec3(0.95f*panel.aspect(), 0.95f, 1.0f));
                    } else {
                        const float cubeHalf = 0.5f * 0.8f;   // unit cube × room scale
                        const float sideLen  = 2.0f * cubeHalf;  // cube side length in the room
                        const float hudH     = 0.42f;         // panel height (m)
                        glm::vec3 boxC = glm::vec3(boxModel[3]);   // cube centre in the room
                        // Float the panel above the cube: centre ~half a side length
                        // above the top edge (cube top = boxC.y + cubeHalf).
                        glm::vec3 hudC = boxC + glm::vec3(0.0f, cubeHalf + 0.5f*sideLen, 0.0f);
                        panelModel = glm::translate(glm::mat4(1.0f), hudC);
                        panelModel = glm::scale(panelModel, glm::vec3(hudH*panel.aspect(), hudH, 1.0f));
                    }

                    glm::vec3 bg = vis.bgColor();
                    for (const VREye& eye : eyes) {
                        glBindFramebuffer(GL_FRAMEBUFFER, eye.framebuffer);
                        glViewport(0, 0, eye.width, eye.height);
                        glClearColor(bg.r, bg.g, bg.b, 1.0f);
                        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                        if (vrState == GameState::IN_LEVEL && level) {
                            glm::mat4 mvp = eye.proj * eye.view * boxModel;
                            LevelContext rctx{window, renderer, hyperMesh, hyperBuf, deltaTime,
                                              insideMode, eye.proj, mvp, mvp, vis, true};
                            level->render(rctx);
                            level->renderWorldHUD(rctx);  // 3D markers (e.g. the dot)
                        }
                        panel.drawInWorld(eye.proj * eye.view * panelModel);
                    }

                    if (!eyes.empty()) {
                        int fbW, fbH; glfwGetFramebufferSize(window, &fbW, &fbH);
                        glBindFramebuffer(GL_READ_FRAMEBUFFER, eyes[0].framebuffer);
                        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
                        glBlitFramebuffer(0,0,eyes[0].width,eyes[0].height, 0,0,fbW,fbH,
                                          GL_COLOR_BUFFER_BIT, GL_LINEAR);
                    }
                }
                vr.endFrame();
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                glfwSwapBuffers(window);
            }
            level.reset();
            vr.shutdown();
        }

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
        return 0;
    }
#else
    if (vrMode)
        std::cerr << "[VR] this binary was built without VR support "
                     "(configure with -DBUILD_VR=ON).\n";
#endif

    // --- Dev helpers (env-driven, harmless when unset) ------------------------
    // START_LEVEL=<n>  : skip the menu and boot straight into level n (1-based,
    //                    matching the menu numbering). Useful for iterating on one
    //                    level without clicking through the menu each launch.
    // FPS_LOG=1        : also print the running FPS to stdout once per second.
    // The window title always shows the live FPS (updated once per second).
    const char* startLevelEnv = getenv("START_LEVEL");
    const int   startLevel     = startLevelEnv ? atoi(startLevelEnv) : 0;  // 1-based; 0 = menu
    const bool  fpsLog         = getenv("FPS_LOG") != nullptr;
    double fpsAccum = 0.0; int fpsFrames = 0;

    // SCREENSHOT=<path> : after SHOT_FRAME frames (default 90, to let the level settle)
    //                     dump the framebuffer to a binary PPM and quit. Headless-ish
    //                     capture for verifying a frame; PPM needs no image library.
    const char* shotPath  = getenv("SCREENSHOT");
    const int   shotFrame = getenv("SHOT_FRAME") ? atoi(getenv("SHOT_FRAME")) : 90;
    long long   frameNo   = 0;

    // Main game loop
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Auto-enter a level on the first menu frame when START_LEVEL is set.
        if (startLevel > 0 && state == GameState::MENU &&
            startLevel <= (int)levelRegistry().size()) {
            level = levelRegistry()[startLevel - 1].factory();
            level->load();
            insideMode = true;
            state = GameState::IN_LEVEL;
            std::cout << "Entered level: " << level->name() << std::endl;
        }

        // Live FPS readout: window title, plus optional stdout log.
        fpsAccum += deltaTime; ++fpsFrames;
        if (fpsAccum >= 1.0) {
            double fps = fpsFrames / fpsAccum;
            char title[128];
            snprintf(title, sizeof(title), "Elua  —  %.0f FPS (%.1f ms)",
                     fps, 1000.0 * fpsAccum / fpsFrames);
            glfwSetWindowTitle(window, title);
            if (fpsLog) std::cout << title << std::endl;
            fpsAccum = 0.0; fpsFrames = 0;
        }

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        // F11: toggle between windowed and fullscreen on the primary monitor.
        bool f11Now = glfwGetKey(window, GLFW_KEY_F11) == GLFW_PRESS;
        if (f11Now && !f11Was) {
            if (glfwGetWindowMonitor(window) == nullptr) {
                // Going fullscreen: remember the current windowed placement first.
                glfwGetWindowPos(window, &savedX, &savedY);
                glfwGetWindowSize(window, &savedW, &savedH);
                GLFWmonitor* mon = glfwGetPrimaryMonitor();
                const GLFWvidmode* mode = glfwGetVideoMode(mon);
                glfwSetWindowMonitor(window, mon, 0, 0,
                                     mode->width, mode->height, mode->refreshRate);
            } else {
                // Back to windowed at the saved placement.
                glfwSetWindowMonitor(window, nullptr, savedX, savedY, savedW, savedH, 0);
            }
            glfwSwapInterval(1);  // re-assert vsync (some drivers drop it on mode switch)
        }
        f11Was = f11Now;

        // ImGui new frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Clear screen (background colour follows the B toggle).
        glm::vec3 bg = vis.bgColor();
        glClearColor(bg.r, bg.g, bg.b, 1.0f);

        // Follow the live framebuffer size so the scene fills the window at any
        // size (resize / fullscreen / HiDPI). Guard h==0 while minimized.
        int fbW, fbH;
        glfwGetFramebufferSize(window, &fbW, &fbH);
        glViewport(0, 0, fbW, fbH);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float aspect = fbH > 0 ? (float)fbW / (float)fbH : 1.0f;
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);

        if (state == GameState::MENU) {
            // Animated lamplit background behind the title screen (drawn over the
            // clear, before ImGui). The hand-drawn drift + mark layer on top of it.
            menuBg.render((float)glfwGetTime(), fbW, fbH);
            int sel = menu.renderMainMenu();
            if (sel >= 0) {
                level = levelRegistry()[sel].factory();
                level->load();
                insideMode = true;   // default to 4D FPS view; TAB drops to 3D observer
                settingsOpen = false;
                state = GameState::IN_LEVEL;
                std::cout << "Entered level: " << level->name() << std::endl;
            }
        }
        else if (state == GameState::IN_LEVEL && level) {
            // Edge detector reused for TAB and the Ctrl+key visualization toggles.
            auto edge = [&](int key, bool& was) {
                bool now = glfwGetKey(window, key) == GLFW_PRESS;
                bool fired = now && !was;
                was = now;
                return fired;
            };

            // Build the per-eye render context up front so it's available whether or
            // not gameplay input ran this frame (it's frozen while Settings is open).
            LevelContext uctx{window, renderer, hyperMesh, hyperBuf, deltaTime, insideMode,
                              projection, glm::mat4(1.0f), glm::mat4(1.0f), vis};

            // All gameplay input is suppressed while the Settings overlay is open, so
            // clicking/adjusting it doesn't move the camera. The scene still renders.
            if (!settingsOpen) {
                // TAB toggles 3D observer vs 4D FPS (universal across levels).
                if (edge(GLFW_KEY_TAB, tabWasPressed)) insideMode = !insideMode;

                // Visualization toggles, now bound to Ctrl+key. Evaluate each edge
                // first (so the per-key state always tracks the raw key), then gate
                // the action on Ctrl being held.
                bool ctrl = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL)  == GLFW_PRESS ||
                            glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
                bool pF = edge(GLFW_KEY_P, pWas), mF = edge(GLFW_KEY_M, mWas);
                bool nF = edge(GLFW_KEY_N, nWas), bF = edge(GLFW_KEY_B, bWas);
                bool fF = edge(GLFW_KEY_F, fWas), vF = edge(GLFW_KEY_V, vWas);
                bool tF = edge(GLFW_KEY_T, tWas), xF = edge(GLFW_KEY_X, xWas);
                if (ctrl && pF) vis.geom     = (GeomMode)(((int)vis.geom + 1) % 3);
                if (ctrl && mF) vis.osc      = (CamOsc)(((int)vis.osc + 1) % 4);
                if (ctrl && nF) vis.depth    = (DepthCue)(((int)vis.depth + 1) % 3);
                if (ctrl && bF) vis.bg       = (vis.bg + 1) % 3;
                if (ctrl && fF) vis.alpha    = (AlphaMode)(((int)vis.alpha + 1) % 4);
                if (ctrl && vF) vis.pulse    = (PulseMode)(((int)vis.pulse + 1) % 3);
                if (ctrl && tF) vis.depthAid = (DepthAid)(((int)vis.depthAid + 1) % 3);
                if (ctrl && xF) vis.occlude4D = !vis.occlude4D;  // 4D hidden-surface removal

                // Focal length adjustment (universal).
                if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
                    level->focalLength() = glm::min(10.0f, level->focalLength() + 2.0f * deltaTime);
                if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
                    level->focalLength() = glm::max(0.1f, level->focalLength() - 2.0f * deltaTime);

                // Update first (moves cam3D), then derive the base view, then render.
                level->update(uctx);
            }
            vis.time += deltaTime;  // drives pulse + camera sway (keeps animating when frozen)

            // The observer camera sways per the M toggle (parallax depth cue).
            level->cam3D().oscMode = (int)vis.osc;
            level->cam3D().oscTime = vis.time;
            glm::mat4 baseView = level->cam3D().getViewMatrix();

            // Per-eye render. In mono this is one full-window pass identical to
            // before; in fake-stereo it's two side-by-side passes. Each eye sets
            // its own viewport + projection + MVP into the shared LevelContext;
            // levels just consume ctx.inner/outerMVP, so none of them change.
            std::vector<EyeView> eyes = buildDesktopEyes(fbW, fbH, baseView, fakeStereo);
            LevelContext rctx{window, renderer, hyperMesh, hyperBuf, deltaTime, insideMode,
                              projection, glm::mat4(1.0f), glm::mat4(1.0f), vis};
            for (const EyeView& eye : eyes) {
                glViewport(eye.x, eye.y, eye.w, eye.h);
                glm::mat4 mvp = eye.proj * eye.view;
                rctx.projection = eye.proj;
                rctx.outerMVP   = mvp;
                rctx.innerMVP   = mvp;
                level->render(rctx);
            }

            // HUD + buttons once, over the whole window (screen-space ImGui).
            glViewport(0, 0, fbW, fbH);
            level->renderHUD(rctx);

            if (menu.renderSettingsButton()) settingsOpen = !settingsOpen;
            if (settingsOpen) drawSettings(vis, settingsOpen);

            if (menu.renderBackButton() || level->checkWin()) {
                level.reset();
                settingsOpen = false;
                state = GameState::MENU;
            }
        }

        // Render ImGui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Optional one-shot frame capture (binary PPM, vertically flipped since GL
        // reads bottom-up). Fires once then asks the loop to exit.
        if (shotPath && ++frameNo == shotFrame) {
            std::vector<unsigned char> px((size_t)fbW * fbH * 3);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadPixels(0, 0, fbW, fbH, GL_RGB, GL_UNSIGNED_BYTE, px.data());
            if (FILE* f = fopen(shotPath, "wb")) {
                fprintf(f, "P6\n%d %d\n255\n", fbW, fbH);
                for (int y = fbH - 1; y >= 0; --y)
                    fwrite(px.data() + (size_t)y * fbW * 3, 1, (size_t)fbW * 3, f);
                fclose(f);
                std::cout << "Wrote screenshot " << shotPath
                          << " (" << fbW << "x" << fbH << ")" << std::endl;
            }
            glfwSetWindowShouldClose(window, true);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Free any active level's GL resources while the context is still current.
    level.reset();

    // Cleanup ImGui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();
    return 0;
}
