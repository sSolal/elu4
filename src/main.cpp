#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <cmath>
#include "Shader.h"

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
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "3D Cube", nullptr, nullptr);
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

    // Cube vertices (pos + color), 6 faces x 4 vertices
    float vertices[] = {
        // Front (red)
        -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 0.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,  1.0f, 0.0f, 0.0f,

        // Back (green)
         0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 0.0f,

        // Left (blue)
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f, 0.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f,

        // Right (yellow)
         0.5f, -0.5f,  0.5f,  1.0f, 1.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  1.0f, 1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 0.0f,

        // Top (cyan)
        -0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 1.0f,
         0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 1.0f,

        // Bottom (magenta)
        -0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 1.0f,
         0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 1.0f
    };

    // Indices for 6 faces x 2 triangles each
    unsigned int indices[] = {
        0, 1, 2,  0, 2, 3,    // Front
        4, 5, 6,  4, 6, 7,    // Back
        8, 9, 10, 8, 10, 11,  // Left
        12, 13, 14, 12, 14, 15, // Right
        16, 17, 18, 16, 18, 19, // Top
        20, 21, 22, 20, 22, 23  // Bottom
    };

    // Inner scene cubes (3 cubes, each scaled 0.2, at different positions)
    // Cube 1: orange, at (0.3, 0, 0.1), scale 0.2
    // Cube 2: purple, at (-0.25, 0.15, -0.2), scale 0.2
    // Cube 3: teal, at (0, -0.2, 0.3), scale 0.2
    float innerVertices[] = {
        // Cube 1 (orange) - Front
        0.3f - 0.1f, -0.1f, 0.1f + 0.1f,  1.0f, 0.5f, 0.0f,
        0.3f + 0.1f, -0.1f, 0.1f + 0.1f,  1.0f, 0.5f, 0.0f,
        0.3f + 0.1f, 0.1f,  0.1f + 0.1f,  1.0f, 0.5f, 0.0f,
        0.3f - 0.1f, 0.1f,  0.1f + 0.1f,  1.0f, 0.5f, 0.0f,
        // Cube 1 - Back
        0.3f + 0.1f, -0.1f, 0.1f - 0.1f,  1.0f, 0.5f, 0.0f,
        0.3f - 0.1f, -0.1f, 0.1f - 0.1f,  1.0f, 0.5f, 0.0f,
        0.3f - 0.1f, 0.1f,  0.1f - 0.1f,  1.0f, 0.5f, 0.0f,
        0.3f + 0.1f, 0.1f,  0.1f - 0.1f,  1.0f, 0.5f, 0.0f,
        // Cube 1 - Left
        0.3f - 0.1f, -0.1f, 0.1f - 0.1f,  1.0f, 0.5f, 0.0f,
        0.3f - 0.1f, -0.1f, 0.1f + 0.1f,  1.0f, 0.5f, 0.0f,
        0.3f - 0.1f, 0.1f,  0.1f + 0.1f,  1.0f, 0.5f, 0.0f,
        0.3f - 0.1f, 0.1f,  0.1f - 0.1f,  1.0f, 0.5f, 0.0f,
        // Cube 1 - Right
        0.3f + 0.1f, -0.1f, 0.1f + 0.1f,  1.0f, 0.5f, 0.0f,
        0.3f + 0.1f, -0.1f, 0.1f - 0.1f,  1.0f, 0.5f, 0.0f,
        0.3f + 0.1f, 0.1f,  0.1f - 0.1f,  1.0f, 0.5f, 0.0f,
        0.3f + 0.1f, 0.1f,  0.1f + 0.1f,  1.0f, 0.5f, 0.0f,
        // Cube 1 - Top
        0.3f - 0.1f, 0.1f,  0.1f + 0.1f,  1.0f, 0.5f, 0.0f,
        0.3f + 0.1f, 0.1f,  0.1f + 0.1f,  1.0f, 0.5f, 0.0f,
        0.3f + 0.1f, 0.1f,  0.1f - 0.1f,  1.0f, 0.5f, 0.0f,
        0.3f - 0.1f, 0.1f,  0.1f - 0.1f,  1.0f, 0.5f, 0.0f,
        // Cube 1 - Bottom
        0.3f - 0.1f, -0.1f, 0.1f - 0.1f,  1.0f, 0.5f, 0.0f,
        0.3f + 0.1f, -0.1f, 0.1f - 0.1f,  1.0f, 0.5f, 0.0f,
        0.3f + 0.1f, -0.1f, 0.1f + 0.1f,  1.0f, 0.5f, 0.0f,
        0.3f - 0.1f, -0.1f, 0.1f + 0.1f,  1.0f, 0.5f, 0.0f,

        // Cube 2 (purple) - Front
        -0.25f - 0.1f, 0.15f - 0.1f, -0.2f + 0.1f,  1.0f, 0.0f, 1.0f,
        -0.25f + 0.1f, 0.15f - 0.1f, -0.2f + 0.1f,  1.0f, 0.0f, 1.0f,
        -0.25f + 0.1f, 0.15f + 0.1f, -0.2f + 0.1f,  1.0f, 0.0f, 1.0f,
        -0.25f - 0.1f, 0.15f + 0.1f, -0.2f + 0.1f,  1.0f, 0.0f, 1.0f,
        // Cube 2 - Back
        -0.25f + 0.1f, 0.15f - 0.1f, -0.2f - 0.1f,  1.0f, 0.0f, 1.0f,
        -0.25f - 0.1f, 0.15f - 0.1f, -0.2f - 0.1f,  1.0f, 0.0f, 1.0f,
        -0.25f - 0.1f, 0.15f + 0.1f, -0.2f - 0.1f,  1.0f, 0.0f, 1.0f,
        -0.25f + 0.1f, 0.15f + 0.1f, -0.2f - 0.1f,  1.0f, 0.0f, 1.0f,
        // Cube 2 - Left
        -0.25f - 0.1f, 0.15f - 0.1f, -0.2f - 0.1f,  1.0f, 0.0f, 1.0f,
        -0.25f - 0.1f, 0.15f - 0.1f, -0.2f + 0.1f,  1.0f, 0.0f, 1.0f,
        -0.25f - 0.1f, 0.15f + 0.1f, -0.2f + 0.1f,  1.0f, 0.0f, 1.0f,
        -0.25f - 0.1f, 0.15f + 0.1f, -0.2f - 0.1f,  1.0f, 0.0f, 1.0f,
        // Cube 2 - Right
        -0.25f + 0.1f, 0.15f - 0.1f, -0.2f + 0.1f,  1.0f, 0.0f, 1.0f,
        -0.25f + 0.1f, 0.15f - 0.1f, -0.2f - 0.1f,  1.0f, 0.0f, 1.0f,
        -0.25f + 0.1f, 0.15f + 0.1f, -0.2f - 0.1f,  1.0f, 0.0f, 1.0f,
        -0.25f + 0.1f, 0.15f + 0.1f, -0.2f + 0.1f,  1.0f, 0.0f, 1.0f,
        // Cube 2 - Top
        -0.25f - 0.1f, 0.15f + 0.1f, -0.2f + 0.1f,  1.0f, 0.0f, 1.0f,
        -0.25f + 0.1f, 0.15f + 0.1f, -0.2f + 0.1f,  1.0f, 0.0f, 1.0f,
        -0.25f + 0.1f, 0.15f + 0.1f, -0.2f - 0.1f,  1.0f, 0.0f, 1.0f,
        -0.25f - 0.1f, 0.15f + 0.1f, -0.2f - 0.1f,  1.0f, 0.0f, 1.0f,
        // Cube 2 - Bottom
        -0.25f - 0.1f, 0.15f - 0.1f, -0.2f - 0.1f,  1.0f, 0.0f, 1.0f,
        -0.25f + 0.1f, 0.15f - 0.1f, -0.2f - 0.1f,  1.0f, 0.0f, 1.0f,
        -0.25f + 0.1f, 0.15f - 0.1f, -0.2f + 0.1f,  1.0f, 0.0f, 1.0f,
        -0.25f - 0.1f, 0.15f - 0.1f, -0.2f + 0.1f,  1.0f, 0.0f, 1.0f,

        // Cube 3 (teal) - Front
        0.0f - 0.1f, -0.2f - 0.1f, 0.3f + 0.1f,  0.0f, 1.0f, 1.0f,
        0.0f + 0.1f, -0.2f - 0.1f, 0.3f + 0.1f,  0.0f, 1.0f, 1.0f,
        0.0f + 0.1f, -0.2f + 0.1f, 0.3f + 0.1f,  0.0f, 1.0f, 1.0f,
        0.0f - 0.1f, -0.2f + 0.1f, 0.3f + 0.1f,  0.0f, 1.0f, 1.0f,
        // Cube 3 - Back
        0.0f + 0.1f, -0.2f - 0.1f, 0.3f - 0.1f,  0.0f, 1.0f, 1.0f,
        0.0f - 0.1f, -0.2f - 0.1f, 0.3f - 0.1f,  0.0f, 1.0f, 1.0f,
        0.0f - 0.1f, -0.2f + 0.1f, 0.3f - 0.1f,  0.0f, 1.0f, 1.0f,
        0.0f + 0.1f, -0.2f + 0.1f, 0.3f - 0.1f,  0.0f, 1.0f, 1.0f,
        // Cube 3 - Left
        0.0f - 0.1f, -0.2f - 0.1f, 0.3f - 0.1f,  0.0f, 1.0f, 1.0f,
        0.0f - 0.1f, -0.2f - 0.1f, 0.3f + 0.1f,  0.0f, 1.0f, 1.0f,
        0.0f - 0.1f, -0.2f + 0.1f, 0.3f + 0.1f,  0.0f, 1.0f, 1.0f,
        0.0f - 0.1f, -0.2f + 0.1f, 0.3f - 0.1f,  0.0f, 1.0f, 1.0f,
        // Cube 3 - Right
        0.0f + 0.1f, -0.2f - 0.1f, 0.3f + 0.1f,  0.0f, 1.0f, 1.0f,
        0.0f + 0.1f, -0.2f - 0.1f, 0.3f - 0.1f,  0.0f, 1.0f, 1.0f,
        0.0f + 0.1f, -0.2f + 0.1f, 0.3f - 0.1f,  0.0f, 1.0f, 1.0f,
        0.0f + 0.1f, -0.2f + 0.1f, 0.3f + 0.1f,  0.0f, 1.0f, 1.0f,
        // Cube 3 - Top
        0.0f - 0.1f, -0.2f + 0.1f, 0.3f + 0.1f,  0.0f, 1.0f, 1.0f,
        0.0f + 0.1f, -0.2f + 0.1f, 0.3f + 0.1f,  0.0f, 1.0f, 1.0f,
        0.0f + 0.1f, -0.2f + 0.1f, 0.3f - 0.1f,  0.0f, 1.0f, 1.0f,
        0.0f - 0.1f, -0.2f + 0.1f, 0.3f - 0.1f,  0.0f, 1.0f, 1.0f,
        // Cube 3 - Bottom
        0.0f - 0.1f, -0.2f - 0.1f, 0.3f - 0.1f,  0.0f, 1.0f, 1.0f,
        0.0f + 0.1f, -0.2f - 0.1f, 0.3f - 0.1f,  0.0f, 1.0f, 1.0f,
        0.0f + 0.1f, -0.2f - 0.1f, 0.3f + 0.1f,  0.0f, 1.0f, 1.0f,
        0.0f - 0.1f, -0.2f - 0.1f, 0.3f + 0.1f,  0.0f, 1.0f, 1.0f,
    };

    unsigned int innerIndices[] = {
        0, 1, 2,  0, 2, 3,    // Cube 1 Front
        4, 5, 6,  4, 6, 7,    // Cube 1 Back
        8, 9, 10, 8, 10, 11,  // Cube 1 Left
        12, 13, 14, 12, 14, 15, // Cube 1 Right
        16, 17, 18, 16, 18, 19, // Cube 1 Top
        20, 21, 22, 20, 22, 23, // Cube 1 Bottom

        24, 25, 26, 24, 26, 27, // Cube 2 Front
        28, 29, 30, 28, 30, 31, // Cube 2 Back
        32, 33, 34, 32, 34, 35, // Cube 2 Left
        36, 37, 38, 36, 38, 39, // Cube 2 Right
        40, 41, 42, 40, 42, 43, // Cube 2 Top
        44, 45, 46, 44, 46, 47, // Cube 2 Bottom

        48, 49, 50, 48, 50, 51, // Cube 3 Front
        52, 53, 54, 52, 54, 55, // Cube 3 Back
        56, 57, 58, 56, 58, 59, // Cube 3 Left
        60, 61, 62, 60, 62, 63, // Cube 3 Right
        64, 65, 66, 64, 66, 67, // Cube 3 Top
        68, 69, 70, 68, 70, 71  // Cube 3 Bottom
    };

    unsigned int VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Color attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // Inner scene VAO
    unsigned int innerVAO, innerVBO, innerEBO;
    glGenVertexArrays(1, &innerVAO);
    glGenBuffers(1, &innerVBO);
    glGenBuffers(1, &innerEBO);

    glBindVertexArray(innerVAO);
    glBindBuffer(GL_ARRAY_BUFFER, innerVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(innerVertices), innerVertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, innerEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(innerIndices), innerIndices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // Load shaders
    Shader shader("shaders/square.vert", "shaders/square.frag");
    Shader innerShader("shaders/inner.vert", "shaders/inner.frag");
    Shader wireShader("shaders/wire.vert", "shaders/wire.frag");

    // Outer camera state
    glm::vec3 cameraPos   = glm::vec3(3.0f, 3.0f, 3.0f);
    glm::vec3 cameraUp    = glm::vec3(0.0f, 1.0f, 0.0f);
    float yaw   = -135.0f;
    float pitch = -35.0f;
    float cameraSpeed = 5.0f;
    float lookSpeed = 60.0f;

    // Inner camera state
    glm::vec3 innerCamPos = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 innerCameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
    float innerYaw = 0.0f;
    float innerPitch = 0.0f;

    // Mode toggle
    bool insideMode = false;
    bool tabWasPressed = false;

    float deltaTime = 0.0f;
    float lastFrame = 0.0f;

    // Render loop
    while (!glfwWindowShouldClose(window)) {
        // Delta time
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Input
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        // Tab toggle for inside/outside mode
        bool tabNow = glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS;
        if (tabNow && !tabWasPressed) {
            insideMode = !insideMode;
        }
        tabWasPressed = tabNow;

        if (!insideMode) {
            // Outside mode: control outer camera with arrows and WASD
            if (glfwGetKey(window, GLFW_KEY_LEFT)  == GLFW_PRESS) yaw   -= lookSpeed * deltaTime;
            if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) yaw   += lookSpeed * deltaTime;
            if (glfwGetKey(window, GLFW_KEY_UP)    == GLFW_PRESS) pitch += lookSpeed * deltaTime;
            if (glfwGetKey(window, GLFW_KEY_DOWN)  == GLFW_PRESS) pitch -= lookSpeed * deltaTime;
            pitch = glm::clamp(pitch, -89.0f, 89.0f);

            glm::vec3 front;
            front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
            front.y = sin(glm::radians(pitch));
            front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
            glm::vec3 cameraFront = glm::normalize(front);

            glm::vec3 right = glm::normalize(glm::cross(cameraFront, cameraUp));

            float speed = cameraSpeed * deltaTime;
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
                cameraPos += speed * cameraFront;
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
                cameraPos -= speed * cameraFront;
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
                cameraPos -= speed * right;
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
                cameraPos += speed * right;
            if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
                cameraPos += speed * cameraUp;
            if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
                cameraPos -= speed * cameraUp;
        } else {
            // Inside mode: control inner camera, outer camera frozen
            if (glfwGetKey(window, GLFW_KEY_LEFT)  == GLFW_PRESS) innerYaw   -= lookSpeed * deltaTime;
            if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) innerYaw   += lookSpeed * deltaTime;
            if (glfwGetKey(window, GLFW_KEY_UP)    == GLFW_PRESS) innerPitch += lookSpeed * deltaTime;
            if (glfwGetKey(window, GLFW_KEY_DOWN)  == GLFW_PRESS) innerPitch -= lookSpeed * deltaTime;
            innerPitch = glm::clamp(innerPitch, -89.0f, 89.0f);

            glm::vec3 innerFront;
            innerFront.x = cos(glm::radians(innerYaw)) * cos(glm::radians(innerPitch));
            innerFront.y = sin(glm::radians(innerPitch));
            innerFront.z = sin(glm::radians(innerYaw)) * cos(glm::radians(innerPitch));
            glm::vec3 innerCameraFront = glm::normalize(innerFront);

            glm::vec3 innerRight = glm::normalize(glm::cross(innerCameraFront, innerCameraUp));

            float speed = cameraSpeed * deltaTime;
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
                innerCamPos += speed * innerCameraFront;
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
                innerCamPos -= speed * innerCameraFront;
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
                innerCamPos -= speed * innerRight;
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
                innerCamPos += speed * innerRight;
            if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
                innerCamPos += speed * innerCameraUp;
            if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
                innerCamPos -= speed * innerCameraUp;
        }

        // Render
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Shared projection matrix
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / SCR_HEIGHT, 0.1f, 100.0f);

        // Compute outer camera matrices
        glm::vec3 outerFront;
        outerFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        outerFront.y = sin(glm::radians(pitch));
        outerFront.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        glm::vec3 outerCameraFront = glm::normalize(outerFront);

        glm::mat4 outerModel = glm::mat4(1.0f);
        glm::mat4 outerView = glm::lookAt(cameraPos, cameraPos + outerCameraFront, cameraUp);
        glm::mat4 outerMVP = projection * outerView * outerModel;

        // Pass 1: Inner scene with clip planes
        glm::vec3 innerFront;
        innerFront.x = cos(glm::radians(innerYaw)) * cos(glm::radians(innerPitch));
        innerFront.y = sin(glm::radians(innerPitch));
        innerFront.z = sin(glm::radians(innerYaw)) * cos(glm::radians(innerPitch));
        glm::vec3 innerCameraFront = glm::normalize(innerFront);

        // Inner view matrix maps inner-world coords to cube-local coords
        glm::mat4 innerLocalView = glm::lookAt(innerCamPos, innerCamPos + innerCameraFront, innerCameraUp);

        // Inner scene: inner view places objects in cube space, outer view/proj renders them
        glm::mat4 innerMVP = projection * outerView * innerLocalView;

        innerShader.use();
        innerShader.setMat4("MVP", innerMVP);
        innerShader.setMat4("innerView", innerLocalView);
        glBindVertexArray(innerVAO);
        glDrawElements(GL_TRIANGLES, 72, GL_UNSIGNED_INT, 0);

        // Pass 2: Wireframe outer cube
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

        wireShader.use();
        wireShader.setMat4("MVP", outerMVP);
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteVertexArrays(1, &innerVAO);
    glDeleteBuffers(1, &innerVBO);
    glDeleteBuffers(1, &innerEBO);
    glfwTerminate();

    return 0;
}
