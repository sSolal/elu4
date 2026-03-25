#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include <cmath>
#include "Object4D.h"
#include "Shader.h"
#include "Math4D.h"

const unsigned int SCR_WIDTH = 1200;
const unsigned int SCR_HEIGHT = 800;

struct VisualizerState {
    Object4D object;
    bool objectLoaded = false;

    // Rotation angles in each plane
    float angleXY = 0.0f;
    float angleXZ = 0.0f;
    float angleXW = 0.0f;
    float angleYZ = 0.0f;
    float angleYW = 0.0f;
    float angleZW = 0.0f;

    float rotationSpeed = 1.5f;
    float focalLength = 2.0f;

    // GPU buffers
    GLuint VAO = 0, vertexVBO = 0, edgeEBO = 0, cellEBO = 0;
    size_t vertexCount = 0, edgeCount = 0, cellIndexCount = 0;

    glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
    glm::vec3 cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);

    bool showEdges = true;
    bool showCells = true;

    // Visualization mode: true = wireframe, false = translucent
    bool wireframeMode = true;
    bool tabWasPressed = false;

    // Gizmo
    GLuint gizmoVAO = 0, gizmoVBO = 0;
} state;

void setupGizmo() {
    // Create a gizmo showing the 4 axes in their rotated positions
    std::vector<float> gizmoData;

    // Define 4D axis directions
    glm::vec4 axisX(0.5f, 0.0f, 0.0f, 0.0f);
    glm::vec4 axisY(0.0f, 0.5f, 0.0f, 0.0f);
    glm::vec4 axisZ(0.0f, 0.0f, 0.5f, 0.0f);
    glm::vec4 axisW(0.0f, 0.0f, 0.0f, 0.5f);

    std::vector<glm::vec4> axes = {axisX, axisY, axisZ, axisW};

    // For each axis: origin to axis endpoint
    // We'll render with different colors: X=red, Y=green, Z=blue, W=white
    for (const auto& axis : axes) {
        // Origin
        gizmoData.push_back(0.0f);
        gizmoData.push_back(0.0f);
        gizmoData.push_back(0.0f);
        gizmoData.push_back(1.0f);

        // Endpoint (we'll transform this in the shader/on CPU)
        gizmoData.push_back(axis.x);
        gizmoData.push_back(axis.y);
        gizmoData.push_back(axis.z);
        gizmoData.push_back(axis.w);
    }

    if (state.gizmoVAO == 0) {
        glGenVertexArrays(1, &state.gizmoVAO);
        glGenBuffers(1, &state.gizmoVBO);
    }

    glBindVertexArray(state.gizmoVAO);
    glBindBuffer(GL_ARRAY_BUFFER, state.gizmoVBO);
    glBufferData(GL_ARRAY_BUFFER, gizmoData.size() * sizeof(float), gizmoData.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void drawGizmo(const Shader& shader, const glm::mat4& MVP) {
    // Apply rotations to the 4 axes
    std::vector<glm::vec4> axes = {
        glm::vec4(0.5f, 0.0f, 0.0f, 0.0f),  // X
        glm::vec4(0.0f, 0.5f, 0.0f, 0.0f),  // Y
        glm::vec4(0.0f, 0.0f, 0.5f, 0.0f),  // Z
        glm::vec4(0.0f, 0.0f, 0.0f, 0.5f)   // W
    };

    std::vector<glm::vec3> colors = {
        glm::vec3(1.0f, 0.0f, 0.0f),  // Red for X
        glm::vec3(0.0f, 1.0f, 0.0f),  // Green for Y
        glm::vec3(0.0f, 0.0f, 1.0f),  // Blue for Z
        glm::vec3(1.0f, 1.0f, 1.0f)   // White for W
    };

    for (size_t i = 0; i < axes.size(); i++) {
        auto axis = axes[i];

        // Apply rotations
        float x = axis.x, y = axis.y, z = axis.z, w = axis.w;

        { float c = std::cos(state.angleXY); float s = std::sin(state.angleXY);
          float nx = c * x - s * y; float ny = s * x + c * y; x = nx; y = ny; }
        { float c = std::cos(state.angleXZ); float s = std::sin(state.angleXZ);
          float nx = c * x - s * z; float nz = s * x + c * z; x = nx; z = nz; }
        { float c = std::cos(state.angleXW); float s = std::sin(state.angleXW);
          float nx = c * x + s * w; float nw = -s * x + c * w; x = nx; w = nw; }
        { float c = std::cos(state.angleYZ); float s = std::sin(state.angleYZ);
          float ny = c * y - s * z; float nz = s * y + c * z; y = ny; z = nz; }
        { float c = std::cos(state.angleYW); float s = std::sin(state.angleYW);
          float ny = c * y + s * w; float nw = -s * y + c * w; y = ny; w = nw; }
        { float c = std::cos(state.angleZW); float s = std::sin(state.angleZW);
          float nz = c * z + s * w; float nw = -s * z + c * w; z = nz; w = nw; }

        glm::vec3 proj = Math4D::project4Dto3D(x, y, z, w, state.focalLength);

        // Draw line from origin to axis
        glm::mat4 model = glm::mat4(1.0f);
        glm::vec3 origin(0.0f);

        // Create line vertices
        std::vector<float> lineData;
        lineData.push_back(origin.x); lineData.push_back(origin.y); lineData.push_back(origin.z); lineData.push_back(1.0f);
        lineData.push_back(proj.x);   lineData.push_back(proj.y);   lineData.push_back(proj.z);   lineData.push_back(1.0f);

        GLuint lineVAO, lineVBO;
        glGenVertexArrays(1, &lineVAO);
        glGenBuffers(1, &lineVBO);
        glBindVertexArray(lineVAO);
        glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
        glBufferData(GL_ARRAY_BUFFER, lineData.size() * sizeof(float), lineData.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        shader.use();
        shader.setMat4("MVP", MVP);
        shader.setVec3("color", colors[i]);
        glDrawArrays(GL_LINE_STRIP, 0, 2);

        glDeleteBuffers(1, &lineVBO);
        glDeleteVertexArrays(1, &lineVAO);
    }
}

void setupBuffers() {
    if (!state.objectLoaded) return;

    // Clean up old buffers
    if (state.VAO != 0) {
        glDeleteBuffers(1, &state.vertexVBO);
        glDeleteBuffers(1, &state.edgeEBO);
        glDeleteBuffers(1, &state.cellEBO);
        glDeleteVertexArrays(1, &state.VAO);
    }

    glGenVertexArrays(1, &state.VAO);
    glBindVertexArray(state.VAO);

    // Vertex buffer
    glGenBuffers(1, &state.vertexVBO);
    glBindBuffer(GL_ARRAY_BUFFER, state.vertexVBO);

    std::vector<float> vertexData;
    for (const auto& v : state.object.vertices) {
        vertexData.push_back(v.x);
        vertexData.push_back(v.y);
        vertexData.push_back(v.z);
        vertexData.push_back(v.w);
    }

    glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(float), vertexData.data(), GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    state.vertexCount = state.object.vertices.size();

    // Edge indices
    if (!state.object.edges.empty()) {
        glGenBuffers(1, &state.edgeEBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state.edgeEBO);

        std::vector<unsigned int> edgeIndices;
        for (const auto& edge : state.object.edges) {
            edgeIndices.push_back(edge.first);
            edgeIndices.push_back(edge.second);
        }

        glBufferData(GL_ELEMENT_ARRAY_BUFFER, edgeIndices.size() * sizeof(unsigned int), edgeIndices.data(), GL_STATIC_DRAW);
        state.edgeCount = edgeIndices.size();
    }

    // Cell indices
    if (!state.object.cells.empty()) {
        glGenBuffers(1, &state.cellEBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state.cellEBO);

        std::vector<unsigned int> cellIndices;
        for (const auto& cell : state.object.cells) {
            for (int idx : cell) {
                cellIndices.push_back(idx);
            }
        }

        glBufferData(GL_ELEMENT_ARRAY_BUFFER, cellIndices.size() * sizeof(unsigned int), cellIndices.data(), GL_STATIC_DRAW);
        state.cellIndexCount = cellIndices.size();
    }

    glBindVertexArray(0);
}

void applyRotations(std::vector<glm::vec4>& verts) {
    for (auto& v : verts) {
        float x = v.x, y = v.y, z = v.z, w = v.w;

        // XY rotation
        {
            float c = std::cos(state.angleXY);
            float s = std::sin(state.angleXY);
            float nx = c * x - s * y;
            float ny = s * x + c * y;
            x = nx; y = ny;
        }

        // XZ rotation
        {
            float c = std::cos(state.angleXZ);
            float s = std::sin(state.angleXZ);
            float nx = c * x - s * z;
            float nz = s * x + c * z;
            x = nx; z = nz;
        }

        // XW rotation
        {
            float c = std::cos(state.angleXW);
            float s = std::sin(state.angleXW);
            float nx = c * x + s * w;
            float nw = -s * x + c * w;
            x = nx; w = nw;
        }

        // YZ rotation
        {
            float c = std::cos(state.angleYZ);
            float s = std::sin(state.angleYZ);
            float ny = c * y - s * z;
            float nz = s * y + c * z;
            y = ny; z = nz;
        }

        // YW rotation
        {
            float c = std::cos(state.angleYW);
            float s = std::sin(state.angleYW);
            float ny = c * y + s * w;
            float nw = -s * y + c * w;
            y = ny; w = nw;
        }

        // ZW rotation
        {
            float c = std::cos(state.angleZW);
            float s = std::sin(state.angleZW);
            float nz = c * z + s * w;
            float nw = -s * z + c * w;
            z = nz; w = nw;
        }

        v = glm::vec4(x, y, z, w);
    }
}

void drawUI() {
    // Simple text rendering would go here
    // For now, this is a placeholder for UI drawing
}

void loadObjectFromFile(const std::string& filePath) {
    try {
        state.object.loadFromJSON(filePath);
        state.objectLoaded = true;
        setupBuffers();
        std::cout << "Loaded object: " << state.object.name << " with "
                  << state.object.vertices.size() << " vertices" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error loading object: " << e.what() << std::endl;
        state.objectLoaded = false;
    }
}

int main(int argc, char* argv[]) {
    // Init GLFW
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

    // Load shader
    Shader shader("shaders/visualizer.vert", "shaders/visualizer.frag");

    // Load object from file if provided as argument
    if (argc > 1) {
        loadObjectFromFile(argv[1]);
    } else {
        std::cout << "Usage: visualizer <object.json>" << std::endl;
        std::cout << "Press 'O' to open a file" << std::endl;
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
        if (tabNow && !state.tabWasPressed) {
            state.wireframeMode = !state.wireframeMode;
            std::cout << (state.wireframeMode ? "Wireframe mode" : "Translucent mode") << std::endl;
        }
        state.tabWasPressed = tabNow;

        // Rotation controls (6 planes)
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) state.angleXY += state.rotationSpeed * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) state.angleXY -= state.rotationSpeed * deltaTime;

        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) state.angleXZ += state.rotationSpeed * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) state.angleXZ -= state.rotationSpeed * deltaTime;

        if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) state.angleXW += state.rotationSpeed * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS) state.angleXW -= state.rotationSpeed * deltaTime;

        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) state.angleYZ += state.rotationSpeed * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) state.angleYZ -= state.rotationSpeed * deltaTime;

        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) state.angleYW += state.rotationSpeed * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) state.angleYW -= state.rotationSpeed * deltaTime;

        if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) state.angleZW += state.rotationSpeed * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS) state.angleZW -= state.rotationSpeed * deltaTime;

        // Focal length
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
            state.focalLength = glm::min(10.0f, state.focalLength + 2.0f * deltaTime);
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
            state.focalLength = glm::max(0.1f, state.focalLength - 2.0f * deltaTime);

        // Toggle visualization
        if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) state.showEdges = !state.showEdges;
        if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) state.showCells = !state.showCells;

        // Render
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (state.objectLoaded) {
            // Apply rotations to vertices
            std::vector<glm::vec4> rotatedVerts = state.object.vertices;
            applyRotations(rotatedVerts);

            // Project to 3D and then to 2D
            std::vector<glm::vec3> projectedVerts;
            for (const auto& v : rotatedVerts) {
                glm::vec3 proj = Math4D::project4Dto3D(v.x, v.y, v.z, v.w, state.focalLength);
                projectedVerts.push_back(proj);
            }

            // Update vertex buffer with projected vertices
            glBindBuffer(GL_ARRAY_BUFFER, state.vertexVBO);
            std::vector<float> vertexData;
            for (const auto& v : projectedVerts) {
                vertexData.push_back(v.x);
                vertexData.push_back(v.y);
                vertexData.push_back(v.z);
                vertexData.push_back(1.0f);  // Dummy w for 3D
            }
            glBufferSubData(GL_ARRAY_BUFFER, 0, vertexData.size() * sizeof(float), vertexData.data());

            // Set up matrices
            glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / SCR_HEIGHT, 0.1f, 100.0f);
            glm::mat4 view = glm::lookAt(state.cameraPos, state.cameraTarget, glm::vec3(0.0f, 1.0f, 0.0f));
            glm::mat4 model = glm::mat4(1.0f);
            glm::mat4 MVP = projection * view * model;

            shader.use();
            shader.setMat4("MVP", MVP);
            shader.setVec3("color", glm::vec3(1.0f, 1.0f, 1.0f));

            glBindVertexArray(state.VAO);

            shader.use();

            if (state.wireframeMode) {
                // Wireframe mode: edges only
                shader.setFloat("alpha", 1.0f);
                glLineWidth(1.5f);
                if (state.edgeCount > 0) {
                    shader.setVec3("color", glm::vec3(1.0f, 1.0f, 1.0f));
                    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state.edgeEBO);
                    glDrawElements(GL_LINES, state.edgeCount, GL_UNSIGNED_INT, 0);
                }
                glLineWidth(1.0f);
            } else {
                // Translucent mode: cells with alpha
                shader.setFloat("alpha", 0.3f);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

                if (state.cellIndexCount > 0) {
                    shader.setVec3("color", glm::vec3(0.3f, 0.4f, 0.8f));
                    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state.cellEBO);
                    glDrawElements(GL_TRIANGLES, state.cellIndexCount, GL_UNSIGNED_INT, 0);
                }

                glDisable(GL_BLEND);
            }

            glBindVertexArray(0);

            // Draw gizmo
            drawGizmo(shader, MVP);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    if (state.VAO != 0) {
        glDeleteBuffers(1, &state.vertexVBO);
        glDeleteBuffers(1, &state.edgeEBO);
        glDeleteBuffers(1, &state.cellEBO);
        glDeleteVertexArrays(1, &state.VAO);
    }

    glfwTerminate();
    return 0;
}
