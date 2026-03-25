#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <cmath>
#include <array>
#include <vector>
#include "Shader.h"
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
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

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

    // Hypercube (tesseract) VAO — dynamic, updated each frame
    unsigned int hcVAO, hcVBO;
    glGenVertexArrays(1, &hcVAO);
    glGenBuffers(1, &hcVBO);

    glBindVertexArray(hcVAO);
    glBindBuffer(GL_ARRAY_BUFFER, hcVBO);
    // 32 edges * 2 vertices * 6 floats (pos + color)
    glBufferData(GL_ARRAY_BUFFER, 32 * 2 * 6 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // Face VAO for tesseract faces (solid, transparent)
    unsigned int faceVAO, faceVBO, faceEBO;
    glGenVertexArrays(1, &faceVAO);
    glGenBuffers(1, &faceVBO);
    glGenBuffers(1, &faceEBO);

    glBindVertexArray(faceVAO);
    glBindBuffer(GL_ARRAY_BUFFER, faceVBO);
    // 16 vertices × 6 floats (pos + color), dynamic
    glBufferData(GL_ARRAY_BUFFER, 16 * 6 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, faceEBO);
    // Face indices will be set up below after computing them
    glBindVertexArray(0);

    // Load shaders
    Shader shader("shaders/square.vert", "shaders/square.frag");
    Shader innerShader("shaders/inner.vert", "shaders/inner.frag");
    Shader wireShader("shaders/wire.vert", "shaders/wire.frag");

    // Outer camera state
    glm::vec3 cameraPos   = glm::vec3(-3.0f, 3.0f, 3.0f);
    glm::vec3 cameraUp    = glm::vec3(0.0f, 1.0f, 0.0f);
    float yaw   = -45.0f;
    float pitch = -35.0f;
    float cameraSpeed = 1.5f;
    float lookSpeed = 60.0f;

    // Inner camera state (single 4D position + orientation angles)
    glm::vec4 innerCam4D = glm::vec4(0.0f, 2.5f, 0.0f, 0.0f);
    float innerYawZ = 0.0f;
    float innerYawW = 0.0f;
    float innerPitch = 0.0f;

    // Physics objects
    PhysicsBody playerBody;
    playerBody.pos    = innerCam4D;
    playerBody.radius = 0.75f;
    PhysicsWorld physWorld;

    // 4D hypercube (tesseract) geometry
    const float HS = 0.3f;         // half-size in each 4D dimension
    float focalLength4D = 1.5f;    // 4D perspective focal distance (adjustable)

    // 16 vertices: all (±HS, ±HS, ±HS, ±HS)
    std::array<glm::vec4, 16> hcVerts4D;
    {
        int vi = 0;
        for (int x : {-1, 1}) for (int y : {-1, 1}) for (int z : {-1, 1}) for (int w : {-1, 1})
            hcVerts4D[vi++] = glm::vec4(x*HS, y*HS, z*HS, w*HS);
    }

    // 32 edges: vertices differing in exactly 1 coordinate
    std::vector<std::pair<int,int>> hcEdges;
    for (int i = 0; i < 16; i++)
        for (int j = i+1; j < 16; j++) {
            glm::vec4 d = hcVerts4D[i] - hcVerts4D[j];
            int diffs = (d.x != 0) + (d.y != 0) + (d.z != 0) + (d.w != 0);
            if (diffs == 1) hcEdges.push_back({i, j});
        }

    // 24 faces of tesseract: choose 2 varying axes, fix other 2 at ±1
    // Bit layout: bit3=x, bit2=y, bit1=z, bit0=w
    std::vector<unsigned int> hcFaceIndices;
    for (int a = 0; a < 4; a++) for (int b = a+1; b < 4; b++) {
        // c, d are the fixed axes
        int fixed[2]; int fi = 0;
        for (int ax = 0; ax < 4; ax++) if (ax != a && ax != b) fixed[fi++] = ax;
        int c = fixed[0], d = fixed[1];
        for (int sc : {0,1}) for (int sd : {0,1}) {
            int base = (sc << c) | (sd << d);
            int v0=base, v1=base|(1<<a), v2=base|(1<<a)|(1<<b), v3=base|(1<<b);
            hcFaceIndices.insert(hcFaceIndices.end(), {(unsigned)v0,(unsigned)v1,(unsigned)v2,
                                                        (unsigned)v0,(unsigned)v2,(unsigned)v3});
        }
    }

    // Upload face indices to EBO
    glBindBuffer(GL_COPY_READ_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, faceEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, hcFaceIndices.size()*sizeof(unsigned int),
                 hcFaceIndices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    // Wireframe cube (edges only, not triangles)
    float wireVertices[] = {
        // 8 vertices of cube at (±0.5, ±0.5, ±0.5)
        -0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,
        -0.5f,  0.5f, -0.5f,
        -0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f
    };
    unsigned int wireIndices[] = {
        // 12 edges: 4 bottom, 4 top, 4 vertical
        0, 1,  1, 2,  2, 3,  3, 0,  // bottom face
        4, 5,  5, 6,  6, 7,  7, 4,  // top face
        0, 4,  1, 5,  2, 6,  3, 7   // vertical edges
    };

    unsigned int wireEdgeVAO, wireEdgeVBO, wireEdgeEBO;
    glGenVertexArrays(1, &wireEdgeVAO);
    glGenBuffers(1, &wireEdgeVBO);
    glGenBuffers(1, &wireEdgeEBO);

    glBindVertexArray(wireEdgeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, wireEdgeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(wireVertices), wireVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, wireEdgeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(wireIndices), wireIndices, GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);


    // Instances: hypercubes on ground
    struct Instance4D {
        glm::vec4 pos;
        float rotXZ, rotXW, rotZW;
        glm::vec3 colorA, colorB;
        const std::array<glm::vec4, 16>* verts;
        int vertCount;
        std::vector<std::pair<int,int>>* edges;
        std::vector<unsigned int>* faceIndices;
    };

    std::vector<Instance4D> instances;

    // Ground grid: hypercubes at y = -HS (below ground level), 9x9x9
    glm::vec3 groundGray = glm::vec3(0.5f, 0.5f, 0.5f);
    for (float x = -2.4f; x <= 2.4f; x += 0.6f) {
        for (float z = -2.4f; z <= 2.4f; z += 0.6f) {
            for (float w = -2.4f; w <= 2.4f; w += 0.6f) {
                instances.push_back({{x, -HS, z, w}, 0.0f, 0.0f, 0.0f,
                                     groundGray, groundGray,
                                     &hcVerts4D, 16, &hcEdges, &hcFaceIndices});
            }
        }
    }

    // Hypercubes floating above ground, spread out with different colors
    instances.push_back({{0.0f, HS, 0.0f, 0.0f}, 0.0f, 0.0f, 0.0f,
                         glm::vec3(0.2f, 0.4f, 1.0f), glm::vec3(1.0f, 0.8f, 0.2f),
                         &hcVerts4D, 16, &hcEdges, &hcFaceIndices});
    instances.push_back({{1.8f, HS, 1.2f, 0.8f}, 0.52f, 0.0f, 0.31f,
                         glm::vec3(1.0f, 0.2f, 0.2f), glm::vec3(1.0f, 1.0f, 0.2f),
                         &hcVerts4D, 16, &hcEdges, &hcFaceIndices});
    instances.push_back({{-1.8f, HS, 1.5f, -1.0f}, 1.1f, 0.0f, 0.0f,
                         glm::vec3(0.2f, 1.0f, 0.2f), glm::vec3(0.8f, 0.2f, 1.0f),
                         &hcVerts4D, 16, &hcEdges, &hcFaceIndices});
    instances.push_back({{1.5f, HS, -1.8f, 1.2f}, 0.0f, 0.78f, 0.0f,
                         glm::vec3(1.0f, 0.5f, 0.2f), glm::vec3(0.2f, 1.0f, 1.0f),
                         &hcVerts4D, 16, &hcEdges, &hcFaceIndices});
    instances.push_back({{-1.5f, HS, -1.2f, 0.9f}, 0.31f, 0.52f, 0.0f,
                         glm::vec3(0.8f, 0.2f, 0.8f), glm::vec3(0.2f, 1.0f, 0.5f),
                         &hcVerts4D, 16, &hcEdges, &hcFaceIndices});

    // Populate physics world with all instances
    for (const auto& inst : instances) {
        physWorld.addObject(inst.pos, HS);
    }

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

        // Focal length adjustment (up/down arrow keys)
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
            focalLength4D = glm::min(10.0f, focalLength4D + 2.0f * deltaTime);
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
            focalLength4D = glm::max(0.1f, focalLength4D - 2.0f * deltaTime);

        if (!insideMode) {
            // Outside mode: control outer camera with IJKL (rotation) and WASD/EC (movement)
            if (glfwGetKey(window, GLFW_KEY_J)  == GLFW_PRESS) yaw   -= lookSpeed * deltaTime;
            if (glfwGetKey(window, GLFW_KEY_L)  == GLFW_PRESS) yaw   += lookSpeed * deltaTime;
            if (glfwGetKey(window, GLFW_KEY_I)  == GLFW_PRESS) pitch += lookSpeed * deltaTime;
            if (glfwGetKey(window, GLFW_KEY_K)  == GLFW_PRESS) pitch -= lookSpeed * deltaTime;
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
            if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
                cameraPos += speed * cameraUp;
            if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS)
                cameraPos -= speed * cameraUp;
        } else {
            // Inside mode: control inner camera, outer camera frozen
            if (glfwGetKey(window, GLFW_KEY_U)  == GLFW_PRESS) innerYawZ  -= lookSpeed * deltaTime;
            if (glfwGetKey(window, GLFW_KEY_L)  == GLFW_PRESS) innerYawZ  += lookSpeed * deltaTime;
            if (glfwGetKey(window, GLFW_KEY_J)  == GLFW_PRESS) innerYawW  -= lookSpeed * deltaTime;
            if (glfwGetKey(window, GLFW_KEY_O)  == GLFW_PRESS) innerYawW  += lookSpeed * deltaTime;
            if (glfwGetKey(window, GLFW_KEY_I)  == GLFW_PRESS) innerPitch += lookSpeed * deltaTime;
            if (glfwGetKey(window, GLFW_KEY_K)  == GLFW_PRESS) innerPitch -= lookSpeed * deltaTime;
            innerPitch = glm::clamp(innerPitch, -89.0f, 89.0f);

            float speed = cameraSpeed * deltaTime;

            // Compute 4D camera basis vectors in world space (inverse of XW→ZW→YW rotation)
            float cxwRad = glm::radians(innerYawW);
            float czwRad = glm::radians(innerYawZ);
            float cywRad = glm::radians(innerPitch);
            float cxw = cos(cxwRad), sxw = sin(cxwRad);
            float czw = cos(czwRad), szw = sin(czwRad);
            float cyw = cos(cywRad), syw = sin(cywRad);

            // Camera-relative world directions (inverse of XW→ZW→YW rotation applied to basis vectors)
            // Camera X-axis → world: only XW rotation affects X
            float fwdX = cxw,       fwdW =  sxw;                           // camera X (forward)
            // Camera Z-axis → world: XW and ZW rotations affect Z
            float rgtX = -sxw*szw,  rgtZ = czw,  rgtW = cxw*szw;          // camera Z (strafe)
            // Camera W-axis → world: clamped to XZW plane (no Y component, for FPS-style horizontal movement)
            float advX = -sxw*czw, advZ = -szw, advW = cxw*czw;           // camera W (advance, Y-clamped)

            // Accumulate desired movement from input (physics will resolve collisions)
            glm::vec4 desiredMove(0.0f);

            // E/A: forward/back (camera X direction)
            if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
                { desiredMove.x += speed*fwdX; desiredMove.w += speed*fwdW; }
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
                { desiredMove.x -= speed*fwdX; desiredMove.w -= speed*fwdW; }

            // Q/D: strafe left/right (camera Z direction)
            if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
                { desiredMove.x -= speed*rgtX; desiredMove.z -= speed*rgtZ; desiredMove.w -= speed*rgtW; }
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
                { desiredMove.x += speed*rgtX; desiredMove.z += speed*rgtZ; desiredMove.w += speed*rgtW; }

            // W/S: advance/retreat (camera W direction — into 4th dimension, clamped to XZW plane)
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
                { desiredMove.x += speed*advX; desiredMove.z += speed*advZ; desiredMove.w += speed*advW; }
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
                { desiredMove.x -= speed*advX; desiredMove.z -= speed*advZ; desiredMove.w -= speed*advW; }

            // Physics step
            playerBody.pos = innerCam4D;
            physWorld.step(playerBody, desiredMove, deltaTime);
            innerCam4D = playerBody.pos;
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

        // The 4D projection produces positions in a fixed camera-retinal 3D frame.
        // No additional 3D translation is needed — the projection is already relative to the camera.
        glm::mat4 innerMVP = projection * outerView;

        // Project 4D hypercube vertices to 3D (perspective along W axis)
        // Apply all three 4D camera rotations (XW, ZW, YW planes) to vertices before projection
        std::array<glm::vec3, 16> hcVerts3D;
        float cxwRad = glm::radians(innerYawW);
        float czwRad = glm::radians(innerYawZ);   // U/L maps to ZW rotation
        float cywRad = glm::radians(innerPitch);  // I/K maps to YW rotation
        float cxw = cos(cxwRad), sxw = sin(cxwRad);
        float czw = cos(czwRad), szw = sin(czwRad);
        float cyw = cos(cywRad), syw = sin(cywRad);

        for (int i = 0; i < 16; i++) {
            // Translate to camera space (camera-relative coordinates)
            float vx = hcVerts4D[i].x - innerCam4D.x;
            float vy = hcVerts4D[i].y - innerCam4D.y;
            float vz = hcVerts4D[i].z - innerCam4D.z;
            float vw = hcVerts4D[i].w - innerCam4D.w;

            // 1. XW rotation (yaw-W: J/O keys)
            float nx = cxw*vx + sxw*vw;
            float nw = -sxw*vx + cxw*vw;
            vx = nx; vw = nw;

            // 2. ZW rotation (U/L keys)
            float nz = czw*vz + szw*vw;
            nw = -szw*vz + czw*vw;
            vz = nz; vw = nw;

            // 3. YW rotation (I/K keys)
            float ny = cyw*vy + syw*vw;
            nw = -syw*vy + cyw*vw;
            vy = ny; vw = nw;

            // W-perspective projection (camera looks along -W, like 3D looks along -Z)
            float negVw = -vw;
            if (negVw < 1e-4f) negVw = 1e-4f;  // clamp vertices at/behind camera
            float f = 1.0f / (negVw + focalLength4D);
            hcVerts3D[i] = glm::vec3(f * vx, f * vy, f * vz);
        }

        // Color function for W-axis (blue=W-, yellow=W+)
        auto wColor = [&](int idx) {
            float t = (hcVerts4D[idx].w + HS) / (2.0f * HS);
            return glm::mix(glm::vec3(0.2f, 0.4f, 1.0f), glm::vec3(1.0f, 0.8f, 0.2f), t);
        };

        // Upload face vertex data
        std::vector<float> faceVertData;
        faceVertData.reserve(16 * 6);
        for (int i = 0; i < 16; i++) {
            glm::vec3 p = hcVerts3D[i], c = wColor(i);
            faceVertData.insert(faceVertData.end(), {p.x, p.y, p.z, c.r, c.g, c.b});
        }
        glBindBuffer(GL_ARRAY_BUFFER, faceVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(faceVertData.size()*sizeof(float)), faceVertData.data());
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        // Render inner scene: translucent shapes only (no edges)
        glDisable(GL_CULL_FACE);  // show both sides of faces
        glDepthMask(GL_FALSE);    // don't write to depth buffer

        innerShader.use();
        innerShader.setFloat("uAlpha", 0.35f);
        innerShader.setMat4("MVP", innerMVP);
        innerShader.setMat4("innerView", glm::mat4(1.0f));

        // Draw all instance faces (translucent, no edges)
        for (auto& inst : instances) {
            // Cull instances entirely behind the camera in W
            bool behindCamera = true;
            for (int i = 0; i < inst.vertCount; i++) {
                glm::vec4 localV = (*inst.verts)[i];
                float vx=localV.x, vy=localV.y, vz=localV.z, vw=localV.w;
                // Local rotations (XZ, XW, ZW)
                { float c=cos(inst.rotXZ), s=sin(inst.rotXZ);
                  float nx=c*vx-s*vz, nz=s*vx+c*vz; vx=nx; vz=nz; }
                { float c=cos(inst.rotXW), s=sin(inst.rotXW);
                  float nx=c*vx-s*vw, nw=s*vx+c*vw; vx=nx; vw=nw; }
                { float c=cos(inst.rotZW), s=sin(inst.rotZW);
                  float nz=c*vz-s*vw, nw=s*vz+c*vw; vz=nz; vw=nw; }
                // Translate + camera space
                vx += inst.pos.x - innerCam4D.x;
                vy += inst.pos.y - innerCam4D.y;
                vz += inst.pos.z - innerCam4D.z;
                vw += inst.pos.w - innerCam4D.w;
                // Camera rotations
                { float nx=cxw*vx+sxw*vw; float nw=-sxw*vx+cxw*vw; vx=nx; vw=nw; }
                { float nz=czw*vz+szw*vw; float nw=-szw*vz+czw*vw; vz=nz; vw=nw; }
                { float ny=cyw*vy+syw*vw; float nw=-syw*vy+cyw*vw; vy=ny; vw=nw; }
                if (-vw > 0) behindCamera = false;
            }
            if (behindCamera) continue;

            std::vector<float> instVertData;
            for (int i = 0; i < inst.vertCount; i++) {
                glm::vec4 localV = (*inst.verts)[i];
                float vx=localV.x, vy=localV.y, vz=localV.z, vw=localV.w;
                // Local rotations (XZ, XW, ZW)
                { float c=cos(inst.rotXZ), s=sin(inst.rotXZ);
                  float nx=c*vx-s*vz, nz=s*vx+c*vz; vx=nx; vz=nz; }
                { float c=cos(inst.rotXW), s=sin(inst.rotXW);
                  float nx=c*vx-s*vw, nw=s*vx+c*vw; vx=nx; vw=nw; }
                { float c=cos(inst.rotZW), s=sin(inst.rotZW);
                  float nz=c*vz-s*vw, nw=s*vz+c*vw; vz=nz; vw=nw; }
                // Translate + camera space
                vx += inst.pos.x - innerCam4D.x;
                vy += inst.pos.y - innerCam4D.y;
                vz += inst.pos.z - innerCam4D.z;
                vw += inst.pos.w - innerCam4D.w;
                // Camera rotations
                { float nx=cxw*vx+sxw*vw; float nw=-sxw*vx+cxw*vw; vx=nx; vw=nw; }
                { float nz=czw*vz+szw*vw; float nw=-szw*vz+czw*vw; vz=nz; vw=nw; }
                { float ny=cyw*vy+syw*vw; float nw=-syw*vy+cyw*vw; vy=ny; vw=nw; }
                float negVw=-vw; if(negVw<1e-4f) negVw=1e-4f;
                float f=1.0f/(negVw+focalLength4D);
                glm::vec3 p(f*vx, f*vy, f*vz);
                float t = (localV.w + HS) / (2.0f * HS);
                if(t < 0.0f) t = 0.0f; if(t > 1.0f) t = 1.0f;
                glm::vec3 c = glm::mix(inst.colorA, inst.colorB, t);
                instVertData.insert(instVertData.end(), {p.x, p.y, p.z, c.r, c.g, c.b});
            }
            glBindBuffer(GL_ARRAY_BUFFER, faceVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(instVertData.size()*sizeof(float)), instVertData.data());
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(faceVAO);
            glDrawElements(GL_TRIANGLES, (GLsizei)inst.faceIndices->size(), GL_UNSIGNED_INT, 0);
        }

        glDepthMask(GL_TRUE);

        // Wireframe outer cube (edges only)
        wireShader.use();
        wireShader.setMat4("MVP", outerMVP);
        glBindVertexArray(wireEdgeVAO);
        glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, 0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteVertexArrays(1, &hcVAO);
    glDeleteBuffers(1, &hcVBO);
    glDeleteVertexArrays(1, &faceVAO);
    glDeleteBuffers(1, &faceVBO);
    glDeleteBuffers(1, &faceEBO);
    glDeleteVertexArrays(1, &wireEdgeVAO);
    glDeleteBuffers(1, &wireEdgeVBO);
    glDeleteBuffers(1, &wireEdgeEBO);
    glfwTerminate();

    return 0;
}
