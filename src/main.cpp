#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include "GameState.h"
#include "Menu.h"
#include "Camera.h"
#include "Tesseract.h"
#include "Scene.h"
#include "Level2.h"
#include "Renderer.h"
#include "physics.h"
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

int main() {
    // Init GLFW
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

    // Level 1 state (lazy-init)
    bool level1Initialized = false;
    PhysicsWorld physWorld1;
    std::vector<Instance4D> scene1;
    Camera3D cam3D_L1;
    Camera4D cam4D_L1;
    PhysicsBody playerBody_L1;
    float focalLength4D_L1 = 1.5f;
    bool insideMode_L1 = false;
    bool tabWasPressed_L1 = false;

    // Level 2 state (lazy-init)
    bool level2Initialized = false;
    PhysicsWorld physWorld2;
    Level2Scene scene2;
    Object4D treeObj;
    ObjectBuffer treeBuffer;
    Camera3D cam3D_L2;
    Camera4D cam4D_L2;
    PhysicsBody playerBody_L2;
    float focalLength4D_L2 = 1.5f;
    bool insideMode_L2 = false;
    bool tabWasPressed_L2 = false;

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

        // Clear screen
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / SCR_HEIGHT, 0.1f, 100.0f);

        if (state == GameState::MENU) {
            // Menu state: just render menu
            state = menu.renderMainMenu(state);
        }
        else if (state == GameState::LEVEL_1) {
            // Initialize Level 1 on first entry
            if (!level1Initialized) {
                scene1 = buildScene(physWorld1);
                playerBody_L1.radius = 0.75f;
                level1Initialized = true;
                std::cout << "Initialized Level 1" << std::endl;
            }

            // Input and rendering
            // Tab toggle
            bool tabNow = glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS;
            if (tabNow && !tabWasPressed_L1) {
                insideMode_L1 = !insideMode_L1;
            }
            tabWasPressed_L1 = tabNow;

            // Focal length adjustment
            if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
                focalLength4D_L1 = glm::min(10.0f, focalLength4D_L1 + 2.0f * deltaTime);
            if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
                focalLength4D_L1 = glm::max(0.1f, focalLength4D_L1 - 2.0f * deltaTime);

            // Camera input
            if (!insideMode_L1) {
                cam3D_L1.processInput(window, deltaTime);
            } else {
                cam4D_L1.processInput(window, deltaTime, playerBody_L1, physWorld1);
            }

            // Render level 1
            glm::mat4 outerView = cam3D_L1.getViewMatrix();
            glm::mat4 outerMVP = projection * outerView;
            glm::mat4 innerMVP = projection * outerView;

            auto angles = cam4D_L1.computeAngles();
            renderer.drawScene(scene1, tesseract.buffers, cam4D_L1, angles, focalLength4D_L1, tesseract, innerMVP);
            renderer.drawOuterCube(outerMVP);

            // Render back button
            state = menu.renderBackButton(state);
        }
        else if (state == GameState::LEVEL_2) {
            // Initialize Level 2 on first entry
            if (!level2Initialized) {
                treeObj = Object4D("objects/tree.json");
                treeBuffer.init(treeObj);
                scene2 = buildLevel2(physWorld2, treeObj);
                playerBody_L2.radius = 0.75f;
                level2Initialized = true;
                std::cout << "Initialized Level 2" << std::endl;
            }

            // Input and rendering
            // Tab toggle
            bool tabNow = glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS;
            if (tabNow && !tabWasPressed_L2) {
                insideMode_L2 = !insideMode_L2;
            }
            tabWasPressed_L2 = tabNow;

            // Focal length adjustment
            if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
                focalLength4D_L2 = glm::min(10.0f, focalLength4D_L2 + 2.0f * deltaTime);
            if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
                focalLength4D_L2 = glm::max(0.1f, focalLength4D_L2 - 2.0f * deltaTime);

            // Camera input
            if (!insideMode_L2) {
                cam3D_L2.processInput(window, deltaTime);
            } else {
                cam4D_L2.processInput(window, deltaTime, playerBody_L2, physWorld2);
            }

            // Render level 2
            glm::mat4 outerView = cam3D_L2.getViewMatrix();
            glm::mat4 outerMVP = projection * outerView;
            glm::mat4 innerMVP = projection * outerView;

            auto angles = cam4D_L2.computeAngles();
            // Ground (tesseract instances)
            renderer.drawScene(scene2.ground, tesseract.buffers, cam4D_L2, angles, focalLength4D_L2, tesseract, innerMVP);
            // Trees (object4d instances)
            renderer.drawObjects(scene2.trees, treeObj, treeBuffer, cam4D_L2, angles, focalLength4D_L2, innerMVP);
            // Outer cube
            renderer.drawOuterCube(outerMVP);

            // Render back button
            state = menu.renderBackButton(state);
        }

        // Render ImGui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup ImGui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    // Cleanup level 2 resources
    if (level2Initialized) {
        treeBuffer.destroy();
    }

    glfwTerminate();
    return 0;
}
