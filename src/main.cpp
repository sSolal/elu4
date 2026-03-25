#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include "Camera.h"
#include "Tesseract.h"
#include "Scene.h"
#include "Renderer.h"
#include "physics.h"

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

    // Create systems
    Tesseract tesseract;
    PhysicsWorld physWorld;
    auto instances = buildScene(physWorld);

    Camera3D cam3D;
    Camera4D cam4D;
    Renderer renderer;

    // Physics player body
    PhysicsBody playerBody;
    playerBody.radius = 0.75f;

    float focalLength4D = 1.5f;
    bool insideMode = false;
    bool tabWasPressed = false;

    float deltaTime = 0.0f;
    float lastFrame = 0.0f;

    // Render loop
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        // Tab toggle for inside/outside mode
        bool tabNow = glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS;
        if (tabNow && !tabWasPressed) {
            insideMode = !insideMode;
        }
        tabWasPressed = tabNow;

        // Focal length adjustment
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
            focalLength4D = glm::min(10.0f, focalLength4D + 2.0f * deltaTime);
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
            focalLength4D = glm::max(0.1f, focalLength4D - 2.0f * deltaTime);

        // Process input
        if (!insideMode) {
            cam3D.processInput(window, deltaTime);
        } else {
            cam4D.processInput(window, deltaTime, playerBody, physWorld);
        }

        // Render
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 outerView = cam3D.getViewMatrix();
        glm::mat4 outerMVP = projection * outerView;
        glm::mat4 innerMVP = projection * outerView;

        auto angles = cam4D.computeAngles();
        renderer.drawScene(instances, tesseract.buffers, cam4D, angles, focalLength4D, tesseract, innerMVP);
        renderer.drawOuterCube(outerMVP);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
