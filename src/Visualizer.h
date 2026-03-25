#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <vector>
#include "Object4D.h"
#include "Shader.h"

class Visualizer {
public:
    Visualizer();
    ~Visualizer();

    void loadObject(const Object4D& object);
    void setupBuffers();
    void draw(const glm::mat4& MVP);

    // Rotation controls
    void rotateXY(float angle) { angleXY += angle; }
    void rotateXZ(float angle) { angleXZ += angle; }
    void rotateXW(float angle) { angleXW += angle; }
    void rotateYZ(float angle) { angleYZ += angle; }
    void rotateYW(float angle) { angleYW += angle; }
    void rotateZW(float angle) { angleZW += angle; }

    void setFocalLength(float f) { focalLength = glm::clamp(f, 0.1f, 10.0f); }
    void setWireframeMode(bool mode) { wireframeMode = mode; }
    void toggleEdges() { showEdges = !showEdges; }
    void toggleCells() { showCells = !showCells; }

    bool isObjectLoaded() const { return objectLoaded; }
    float getFocalLength() const { return focalLength; }
    bool getWireframeMode() const { return wireframeMode; }

private:
    Object4D object;
    bool objectLoaded = false;

    // Rotation angles in each plane
    float angleXY = 0.0f, angleXZ = 0.0f, angleXW = 0.0f;
    float angleYZ = 0.0f, angleYW = 0.0f, angleZW = 0.0f;
    float focalLength = 2.0f;

    // GPU buffers
    GLuint VAO = 0, vertexVBO = 0, edgeEBO = 0, cellEBO = 0;
    size_t vertexCount = 0, edgeCount = 0, cellIndexCount = 0;

    bool showEdges = true, showCells = true;
    bool wireframeMode = true;

    Shader shader;

    void applyRotations(std::vector<glm::vec4>& verts);
    void drawGizmo(const glm::mat4& MVP);
};
