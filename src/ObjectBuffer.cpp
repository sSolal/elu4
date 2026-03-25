#include "ObjectBuffer.h"

void ObjectBuffer::init(const Object4D& obj) {
    indexCount = obj.triangleIndices.size();
    vertexCount = obj.vertices.size();

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    // Allocate for dynamic updates: vertex count × 6 floats (xyz + rgb)
    glBufferData(GL_ARRAY_BUFFER, vertexCount * 6 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // Color attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Upload triangle indices
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 obj.triangleIndices.size() * sizeof(unsigned int),
                 obj.triangleIndices.data(),
                 GL_STATIC_DRAW);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void ObjectBuffer::uploadVerts(const std::vector<float>& vertData) {
    glBindBuffer(GL_COPY_READ_BUFFER, vbo);
    glBufferSubData(GL_COPY_READ_BUFFER, 0, (GLsizeiptr)(vertData.size() * sizeof(float)), vertData.data());
    glBindBuffer(GL_COPY_READ_BUFFER, 0);
}

void ObjectBuffer::draw() {
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, (GLsizei)indexCount, GL_UNSIGNED_INT, 0);
}

void ObjectBuffer::destroy() {
    if (vao) glDeleteVertexArrays(1, &vao);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (ebo) glDeleteBuffers(1, &ebo);
    vao = vbo = ebo = 0;
}
