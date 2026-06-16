#pragma once

#include <GL/glew.h>
#include <vector>
#include "Object4D.h"

struct ObjectBuffer {
    GLuint vao, vbo, ebo;
    GLuint edgeEbo;
    size_t indexCount;
    size_t edgeIndexCount;
    size_t vertexCount;

    ObjectBuffer() : vao(0), vbo(0), ebo(0), edgeEbo(0),
                     indexCount(0), edgeIndexCount(0), vertexCount(0) {}

    void init(const Object4D& obj);
    void uploadVerts(const std::vector<float>& vertData);
    void draw();        // solid faces (GL_TRIANGLES)
    void drawEdges();   // wireframe edges (GL_LINES)
    void destroy();
};
