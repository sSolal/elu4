#include "Visualizer.h"
#include "Math4D.h"
#include <cmath>

Visualizer::Visualizer()
    : shader("shaders/visualizer.vert", "shaders/visualizer.frag") {
}

Visualizer::~Visualizer() {
    if (VAO != 0) {
        glDeleteBuffers(1, &vertexVBO);
        glDeleteBuffers(1, &edgeEBO);
        glDeleteBuffers(1, &cellEBO);
        glDeleteVertexArrays(1, &VAO);
    }
}

void Visualizer::loadObject(const Object4D& obj) {
    object = obj;
    objectLoaded = true;
    setupBuffers();
}

void Visualizer::setupBuffers() {
    if (!objectLoaded) return;

    // Clean up old buffers
    if (VAO != 0) {
        glDeleteBuffers(1, &vertexVBO);
        glDeleteBuffers(1, &edgeEBO);
        glDeleteBuffers(1, &cellEBO);
        glDeleteVertexArrays(1, &VAO);
    }

    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    // Vertex buffer
    glGenBuffers(1, &vertexVBO);
    glBindBuffer(GL_ARRAY_BUFFER, vertexVBO);

    std::vector<float> vertexData;
    for (const auto& v : object.vertices) {
        vertexData.push_back(v.x);
        vertexData.push_back(v.y);
        vertexData.push_back(v.z);
        vertexData.push_back(v.w);
    }

    glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(float), vertexData.data(), GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    vertexCount = object.vertices.size();

    // Edge indices
    if (!object.edges.empty()) {
        glGenBuffers(1, &edgeEBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, edgeEBO);

        std::vector<unsigned int> edgeIndices;
        for (const auto& edge : object.edges) {
            edgeIndices.push_back(edge.first);
            edgeIndices.push_back(edge.second);
        }

        glBufferData(GL_ELEMENT_ARRAY_BUFFER, edgeIndices.size() * sizeof(unsigned int), edgeIndices.data(), GL_STATIC_DRAW);
        edgeCount = edgeIndices.size();
    }

    // Cell indices
    if (!object.triangleIndices.empty()) {
        glGenBuffers(1, &cellEBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cellEBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, object.triangleIndices.size() * sizeof(unsigned int),
                     object.triangleIndices.data(), GL_STATIC_DRAW);
        cellIndexCount = object.triangleIndices.size();
    } else if (!object.cells.empty()) {
        // Fallback: triangulate cells on-the-fly
        glGenBuffers(1, &cellEBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cellEBO);

        std::vector<unsigned int> cellIndices;
        for (const auto& cell : object.cells) {
            if (cell.size() == 4) {
                cellIndices.push_back(cell[0]);
                cellIndices.push_back(cell[1]);
                cellIndices.push_back(cell[2]);
                cellIndices.push_back(cell[0]);
                cellIndices.push_back(cell[2]);
                cellIndices.push_back(cell[3]);
            } else if (cell.size() == 3) {
                for (int idx : cell) cellIndices.push_back(idx);
            }
        }

        glBufferData(GL_ELEMENT_ARRAY_BUFFER, cellIndices.size() * sizeof(unsigned int), cellIndices.data(), GL_STATIC_DRAW);
        cellIndexCount = cellIndices.size();
    }

    glBindVertexArray(0);
}

void Visualizer::applyRotations(std::vector<glm::vec4>& verts) {
    for (auto& v : verts) {
        float x = v.x, y = v.y, z = v.z, w = v.w;

        // XY rotation
        {
            float c = std::cos(angleXY);
            float s = std::sin(angleXY);
            float nx = c * x - s * y;
            float ny = s * x + c * y;
            x = nx; y = ny;
        }

        // XZ rotation
        {
            float c = std::cos(angleXZ);
            float s = std::sin(angleXZ);
            float nx = c * x - s * z;
            float nz = s * x + c * z;
            x = nx; z = nz;
        }

        // XW rotation
        {
            float c = std::cos(angleXW);
            float s = std::sin(angleXW);
            float nx = c * x + s * w;
            float nw = -s * x + c * w;
            x = nx; w = nw;
        }

        // YZ rotation
        {
            float c = std::cos(angleYZ);
            float s = std::sin(angleYZ);
            float ny = c * y - s * z;
            float nz = s * y + c * z;
            y = ny; z = nz;
        }

        // YW rotation
        {
            float c = std::cos(angleYW);
            float s = std::sin(angleYW);
            float ny = c * y + s * w;
            float nw = -s * y + c * w;
            y = ny; w = nw;
        }

        // ZW rotation
        {
            float c = std::cos(angleZW);
            float s = std::sin(angleZW);
            float nz = c * z + s * w;
            float nw = -s * z + c * w;
            z = nz; w = nw;
        }

        v = glm::vec4(x, y, z, w);
    }
}

void Visualizer::drawGizmo(const glm::mat4& MVP) {
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
        float x = axis.x, y = axis.y, z = axis.z, w = axis.w;

        // Apply rotations
        { float c = std::cos(angleXY); float s = std::sin(angleXY);
          float nx = c * x - s * y; float ny = s * x + c * y; x = nx; y = ny; }
        { float c = std::cos(angleXZ); float s = std::sin(angleXZ);
          float nx = c * x - s * z; float nz = s * x + c * z; x = nx; z = nz; }
        { float c = std::cos(angleXW); float s = std::sin(angleXW);
          float nx = c * x + s * w; float nw = -s * x + c * w; x = nx; w = nw; }
        { float c = std::cos(angleYZ); float s = std::sin(angleYZ);
          float ny = c * y - s * z; float nz = s * y + c * z; y = ny; z = nz; }
        { float c = std::cos(angleYW); float s = std::sin(angleYW);
          float ny = c * y + s * w; float nw = -s * y + c * w; y = ny; w = nw; }
        { float c = std::cos(angleZW); float s = std::sin(angleZW);
          float nz = c * z + s * w; float nw = -s * z + c * w; z = nz; w = nw; }

        glm::vec3 proj = Math4D::project4Dto3D(x, y, z, w, focalLength);

        // Draw line from origin to axis
        std::vector<float> lineData;
        lineData.push_back(0.0f); lineData.push_back(0.0f); lineData.push_back(0.0f); lineData.push_back(1.0f);
        lineData.push_back(proj.x); lineData.push_back(proj.y); lineData.push_back(proj.z); lineData.push_back(1.0f);

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

void Visualizer::draw(const glm::mat4& MVP) {
    if (!objectLoaded) return;

    // Apply rotations to vertices
    std::vector<glm::vec4> rotatedVerts = object.vertices;
    applyRotations(rotatedVerts);

    // Project to 3D
    std::vector<glm::vec3> projectedVerts;
    for (const auto& v : rotatedVerts) {
        glm::vec3 proj = Math4D::project4Dto3D(v.x, v.y, v.z, v.w, focalLength);
        projectedVerts.push_back(proj);
    }

    // Update vertex buffer with projected vertices
    glBindBuffer(GL_ARRAY_BUFFER, vertexVBO);
    std::vector<float> vertexData;
    for (const auto& v : projectedVerts) {
        vertexData.push_back(v.x);
        vertexData.push_back(v.y);
        vertexData.push_back(v.z);
        vertexData.push_back(1.0f);  // Dummy w for 3D
    }
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertexData.size() * sizeof(float), vertexData.data());

    shader.use();
    shader.setMat4("MVP", MVP);
    glBindVertexArray(VAO);

    if (wireframeMode) {
        // Wireframe mode: edges only
        shader.setFloat("alpha", 1.0f);
        glLineWidth(1.5f);
        if (edgeCount > 0 && showEdges) {
            shader.setVec3("color", glm::vec3(1.0f, 1.0f, 1.0f));
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, edgeEBO);
            glDrawElements(GL_LINES, edgeCount, GL_UNSIGNED_INT, 0);
        }
        glLineWidth(1.0f);
    } else {
        // Translucent mode: cells with alpha
        shader.setFloat("alpha", 0.3f);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        if (cellIndexCount > 0 && showCells) {
            shader.setVec3("color", glm::vec3(0.3f, 0.4f, 0.8f));
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cellEBO);
            glDrawElements(GL_TRIANGLES, cellIndexCount, GL_UNSIGNED_INT, 0);
        }

        glDisable(GL_BLEND);
    }

    glBindVertexArray(0);

    // Draw gizmo
    drawGizmo(MVP);
}
