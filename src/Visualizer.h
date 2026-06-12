#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <vector>
#include "Object4D.h"
#include "Shader.h"
#include "Math4D.h"

class Visualizer {
public:
    Visualizer();
    ~Visualizer();

    void loadObject(const Object4D& object);
    void setupBuffers();
    void draw(const glm::mat4& MVP);

    // Rotation controls: compose new plane rotation onto current orientation.
    // Re-normalize after each compose so float drift can't accumulate over a
    // long session of rotating in many directions.
    void rotateXY(float angle) { orientation = Math4D::Rotor4D::fromXY(angle) * orientation; orientation.normalize(); }
    void rotateXZ(float angle) { orientation = Math4D::Rotor4D::fromXZ(angle) * orientation; orientation.normalize(); }
    void rotateXW(float angle) { orientation = Math4D::Rotor4D::fromXW(angle) * orientation; orientation.normalize(); }
    void rotateYZ(float angle) { orientation = Math4D::Rotor4D::fromYZ(angle) * orientation; orientation.normalize(); }
    void rotateYW(float angle) { orientation = Math4D::Rotor4D::fromYW(angle) * orientation; orientation.normalize(); }
    void rotateZW(float angle) { orientation = Math4D::Rotor4D::fromZW(angle) * orientation; orientation.normalize(); }

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

    // Current orientation as a rotor
    Math4D::Rotor4D orientation = Math4D::Rotor4D::identity();
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
