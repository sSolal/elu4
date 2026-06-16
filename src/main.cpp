#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <memory>
#include "GameState.h"
#include "Menu.h"
#include "Tesseract.h"
#include "Renderer.h"
#include "Level.h"
#include "LevelRegistry.h"
#include "RenderSettings.h"
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// Small always-on overlay listing every visualization toggle and its current state.
static void drawLegend(const RenderSettings& vis) {
    static const char* kGeom[]  = {"Solid", "Wireframe", "Solid + borders"};
    static const char* kOsc[]   = {"Static", "Horizontal", "Circle", "Random"};
    static const char* kDepth[] = {"Fog (near vivid)", "Normal", "Darken far"};
    static const char* kAlpha[] = {"Mid 0.35", "Light 0.2", "Heavy 0.85", "Near-transparent"};
    static const char* kBg[]    = {"Warm white", "Deep blue", "Black"};
    static const char* kPulse[] = {"off", "Sine (slow)", "Noise"};

    ImGui::SetNextWindowPos(ImVec2(10.0f, (float)SCR_HEIGHT - 158.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(232.0f, 148.0f), ImGuiCond_Always);
    ImGui::Begin("Legend", nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);
    ImGui::Text("P  Geometry: %s", kGeom[(int)vis.geom]);
    ImGui::Text("M  Cam sway: %s", kOsc[(int)vis.osc]);
    ImGui::Text("N  Depth:    %s", kDepth[(int)vis.depth]);
    ImGui::Text("B  Backgnd:  %s", kBg[vis.bg]);
    ImGui::Text("F  Alpha:    %s", kAlpha[(int)vis.alpha]);
    ImGui::Text("V  X-ray pulse: %s", kPulse[(int)vis.pulse]);
    ImGui::End();
}

int main() {
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

    // Create window
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "4D Game", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

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
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Create game systems
    GameState state = GameState::MENU;
    Menu menu;
    Tesseract tesseract;
    Renderer renderer;

    // The active level (null in the menu). Constructed lazily from the registry
    // when the player picks a level; reset() on Back / win frees its resources.
    std::unique_ptr<Level> level;
    bool insideMode = false;     // TAB: 3D observer vs 4D FPS
    bool tabWasPressed = false;

    // Global visualization toggles (P/M/N/B/F/V/G), with per-key edge-detection.
    RenderSettings vis;
    bool pWas = false, mWas = false, nWas = false, bWas = false,
         fWas = false, vWas = false;

    float deltaTime = 0.0f;
    float lastFrame = 0.0f;

    // Main game loop
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        // ImGui new frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Clear screen (background colour follows the B toggle).
        glm::vec3 bg = vis.bgColor();
        glClearColor(bg.r, bg.g, bg.b, 1.0f);
        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / SCR_HEIGHT, 0.1f, 100.0f);

        if (state == GameState::MENU) {
            int sel = menu.renderMainMenu();
            if (sel >= 0) {
                level = levelRegistry()[sel].factory();
                level->load();
                insideMode = false;
                state = GameState::IN_LEVEL;
                std::cout << "Entered level: " << level->name() << std::endl;
            }
        }
        else if (state == GameState::IN_LEVEL && level) {
            // TAB toggles 3D observer vs 4D FPS (universal across levels).
            bool tabNow = glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS;
            if (tabNow && !tabWasPressed) insideMode = !insideMode;
            tabWasPressed = tabNow;

            // Visualization toggles (edge-detected): cycle/flip on each press.
            auto edge = [&](int key, bool& was) {
                bool now = glfwGetKey(window, key) == GLFW_PRESS;
                bool fired = now && !was;
                was = now;
                return fired;
            };
            if (edge(GLFW_KEY_P, pWas)) vis.geom  = (GeomMode)(((int)vis.geom + 1) % 3);
            if (edge(GLFW_KEY_M, mWas)) vis.osc   = (CamOsc)(((int)vis.osc + 1) % 4);
            if (edge(GLFW_KEY_N, nWas)) vis.depth = (DepthCue)(((int)vis.depth + 1) % 3);
            if (edge(GLFW_KEY_B, bWas)) vis.bg    = (vis.bg + 1) % 3;
            if (edge(GLFW_KEY_F, fWas)) vis.alpha = (AlphaMode)(((int)vis.alpha + 1) % 4);
            if (edge(GLFW_KEY_V, vWas)) vis.pulse = (PulseMode)(((int)vis.pulse + 1) % 3);
            vis.time += deltaTime;  // drives pulse + camera sway

            // Focal length adjustment (universal).
            if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
                level->focalLength() = glm::min(10.0f, level->focalLength() + 2.0f * deltaTime);
            if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
                level->focalLength() = glm::max(0.1f, level->focalLength() - 2.0f * deltaTime);

            // Update first (moves cam3D), then derive the camera MVPs, then render.
            LevelContext uctx{window, renderer, tesseract, deltaTime, insideMode,
                              projection, glm::mat4(1.0f), glm::mat4(1.0f), vis};
            level->update(uctx);

            // The observer camera sways per the M toggle (parallax depth cue).
            level->cam3D().oscMode = (int)vis.osc;
            level->cam3D().oscTime = vis.time;
            glm::mat4 outerView = level->cam3D().getViewMatrix();
            glm::mat4 mvp = projection * outerView;

            LevelContext rctx{window, renderer, tesseract, deltaTime, insideMode,
                              projection, mvp, mvp, vis};
            level->render(rctx);
            level->renderHUD(rctx);
            drawLegend(vis);

            if (menu.renderBackButton() || level->checkWin()) {
                level.reset();
                state = GameState::MENU;
            }
        }

        // Render ImGui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

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
