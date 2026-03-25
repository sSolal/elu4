#pragma once

#include <GL/glew.h>
#include <vector>
#include "Object4D.h"

struct ObjectBuffer {
    GLuint vao, vbo, ebo;
    size_t indexCount;
    size_t vertexCount;

    ObjectBuffer() : vao(0), vbo(0), ebo(0), indexCount(0), vertexCount(0) {}

    void init(const Object4D& obj);
    void uploadVerts(const std::vector<float>& vertData);
    void draw();
    void destroy();
};
