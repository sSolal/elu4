#include "ObjectBuffer.h"

void ObjectBuffer::init(const Object4D& obj) {
    indexCount = obj.triangleIndices.size();
    vertexCount = obj.vertices.size();

    // Flatten the edge pairs into a GL_LINES index list.
    std::vector<unsigned int> edgeIdx;
    edgeIdx.reserve(obj.edges.size() * 2);
    for (const auto& e : obj.edges) {
        edgeIdx.push_back((unsigned int)e.first);
        edgeIdx.push_back((unsigned int)e.second);
    }
    edgeIndexCount = edgeIdx.size();

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    glGenBuffers(1, &edgeEbo);
    glGenBuffers(1, &sortedEbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    // Allocate for dynamic updates: vertex count × 7 floats (xyz + rgb + depth)
    glBufferData(GL_ARRAY_BUFFER, vertexCount * 7 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // Color attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // Depth attribute (camera-relative 4D depth)
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    // Upload triangle indices
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 obj.triangleIndices.size() * sizeof(unsigned int),
                 obj.triangleIndices.data(),
                 GL_STATIC_DRAW);

    // Upload edge indices (separate element buffer)
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, edgeEbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 edgeIdx.size() * sizeof(unsigned int),
                 edgeIdx.data(),
                 GL_STATIC_DRAW);

    // Per-frame depth-sorted triangle indices: same capacity, refilled each draw.
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sortedEbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 obj.triangleIndices.size() * sizeof(unsigned int),
                 nullptr,
                 GL_DYNAMIC_DRAW);

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
    // The VAO's element binding may have been changed by drawEdges(); set it back.
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glDrawElements(GL_TRIANGLES, (GLsizei)indexCount, GL_UNSIGNED_INT, 0);
}

void ObjectBuffer::drawSorted(const std::vector<unsigned int>& sortedIndices) {
    glBindVertexArray(vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sortedEbo);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0,
                    (GLsizeiptr)(sortedIndices.size() * sizeof(unsigned int)),
                    sortedIndices.data());
    glDrawElements(GL_TRIANGLES, (GLsizei)sortedIndices.size(), GL_UNSIGNED_INT, 0);
}

void ObjectBuffer::drawEdges() {
    glBindVertexArray(vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, edgeEbo);
    glDrawElements(GL_LINES, (GLsizei)edgeIndexCount, GL_UNSIGNED_INT, 0);
}

void ObjectBuffer::destroy() {
    if (vao) glDeleteVertexArrays(1, &vao);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (ebo) glDeleteBuffers(1, &ebo);
    if (edgeEbo) glDeleteBuffers(1, &edgeEbo);
    if (sortedEbo) glDeleteBuffers(1, &sortedEbo);
    vao = vbo = ebo = edgeEbo = sortedEbo = 0;
}
