#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include "Object4D.h"
#include "Visualizer.h"

const unsigned int SCR_WIDTH = 1200;
const unsigned int SCR_HEIGHT = 800;
const float ROTATION_SPEED = 1.5f;

glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
glm::vec3 cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);
bool tabWasPressed = false;

int main(int argc, char* argv[]) {
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

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "4D Object Visualizer", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return -1;
    }

    glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Create visualizer after GL context is ready
    Visualizer visualizer;

    // Load object from file if provided as argument
    if (argc > 1) {
        try {
            Object4D object(argv[1]);
            visualizer.loadObject(object);
            std::cout << "Loaded object: " << object.name << " with "
                      << object.vertices.size() << " vertices" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error loading object: " << e.what() << std::endl;
        }
    } else {
        std::cout << "Usage: visualizer <object.json>" << std::endl;
    }

    // Render loop
    double lastTime = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        double currentTime = glfwGetTime();
        float deltaTime = (float)(currentTime - lastTime);
        lastTime = currentTime;

        // Input handling
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, true);
        }

        // TAB toggle wireframe/translucent
        bool tabNow = glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS;
        if (tabNow && !tabWasPressed) {
            visualizer.setWireframeMode(!visualizer.getWireframeMode());
            std::cout << (visualizer.getWireframeMode() ? "Wireframe mode" : "Translucent mode") << std::endl;
        }
        tabWasPressed = tabNow;

        // Rotation controls (6 planes)
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) visualizer.rotateXY(ROTATION_SPEED * deltaTime);
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) visualizer.rotateXY(-ROTATION_SPEED * deltaTime);

        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) visualizer.rotateXZ(ROTATION_SPEED * deltaTime);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) visualizer.rotateXZ(-ROTATION_SPEED * deltaTime);

        if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) visualizer.rotateXW(ROTATION_SPEED * deltaTime);
        if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS) visualizer.rotateXW(-ROTATION_SPEED * deltaTime);

        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) visualizer.rotateYZ(ROTATION_SPEED * deltaTime);
        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) visualizer.rotateYZ(-ROTATION_SPEED * deltaTime);

        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) visualizer.rotateYW(ROTATION_SPEED * deltaTime);
        if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) visualizer.rotateYW(-ROTATION_SPEED * deltaTime);

        if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) visualizer.rotateZW(ROTATION_SPEED * deltaTime);
        if (glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS) visualizer.rotateZW(-ROTATION_SPEED * deltaTime);

        // Focal length
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
            visualizer.setFocalLength(visualizer.getFocalLength() + 2.0f * deltaTime);
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
            visualizer.setFocalLength(visualizer.getFocalLength() - 2.0f * deltaTime);

        // Toggle visualization
        if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) visualizer.toggleEdges();
        if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) visualizer.toggleCells();

        // Render
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (visualizer.isObjectLoaded()) {
            // Set up matrices
            glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / SCR_HEIGHT, 0.1f, 100.0f);
            glm::mat4 view = glm::lookAt(cameraPos, cameraTarget, glm::vec3(0.0f, 1.0f, 0.0f));
            glm::mat4 model = glm::mat4(1.0f);
            glm::mat4 MVP = projection * view * model;

            visualizer.draw(MVP);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
