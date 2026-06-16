#include "Tesseract.h"

void Tesseract::Buffers::init(const std::vector<unsigned int>& indices,
                              const std::vector<unsigned int>& edgeIndices) {
    indexCount = indices.size();
    edgeIndexCount = edgeIndices.size();

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    glGenBuffers(1, &edgeEbo);
    glGenBuffers(1, &sortedEbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    // 16 vertices × 7 floats (pos + color + depth), dynamic
    glBufferData(GL_ARRAY_BUFFER, 16 * 7 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, edgeEbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, edgeIndices.size() * sizeof(unsigned int), edgeIndices.data(), GL_STATIC_DRAW);

    // Per-frame depth-sorted face indices: same size as the static list, refilled each draw.
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sortedEbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), nullptr, GL_DYNAMIC_DRAW);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void Tesseract::Buffers::uploadVerts(const std::vector<float>& vertData) {
    glBindBuffer(GL_COPY_READ_BUFFER, vbo);
    glBufferSubData(GL_COPY_READ_BUFFER, 0, (GLsizeiptr)(vertData.size() * sizeof(float)), vertData.data());
    glBindBuffer(GL_COPY_READ_BUFFER, 0);
}

void Tesseract::Buffers::draw() {
    glBindVertexArray(vao);
    // drawEdges() may have changed the VAO's element binding; restore it.
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glDrawElements(GL_TRIANGLES, (GLsizei)indexCount, GL_UNSIGNED_INT, 0);
}

void Tesseract::Buffers::drawSorted(const std::vector<unsigned int>& sortedIndices) {
    glBindVertexArray(vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sortedEbo);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0,
                    (GLsizeiptr)(sortedIndices.size() * sizeof(unsigned int)),
                    sortedIndices.data());
    glDrawElements(GL_TRIANGLES, (GLsizei)sortedIndices.size(), GL_UNSIGNED_INT, 0);
}

void Tesseract::Buffers::drawEdges() {
    glBindVertexArray(vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, edgeEbo);
    glDrawElements(GL_LINES, (GLsizei)edgeIndexCount, GL_UNSIGNED_INT, 0);
}

void Tesseract::Buffers::destroy() {
    if (vao) glDeleteVertexArrays(1, &vao);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (ebo) glDeleteBuffers(1, &ebo);
    if (edgeEbo) glDeleteBuffers(1, &edgeEbo);
    if (sortedEbo) glDeleteBuffers(1, &sortedEbo);
    vao = vbo = ebo = edgeEbo = sortedEbo = 0;
}

Tesseract::Tesseract() {
    buildTopology();
}

void Tesseract::buildTopology() {
    // 16 vertices: all (±HS, ±HS, ±HS, ±HS)
    {
        int vi = 0;
        for (int x : {-1, 1}) {
            for (int y : {-1, 1}) {
                for (int z : {-1, 1}) {
                    for (int w : {-1, 1}) {
                        verts4D[vi++] = glm::vec4(x * HS, y * HS, z * HS, w * HS);
                    }
                }
            }
        }
    }

    // 32 edges: vertices differing in exactly 1 coordinate
    for (int i = 0; i < 16; i++) {
        for (int j = i + 1; j < 16; j++) {
            glm::vec4 d = verts4D[i] - verts4D[j];
            int diffs = (d.x != 0) + (d.y != 0) + (d.z != 0) + (d.w != 0);
            if (diffs == 1) {
                edges.push_back({i, j});
            }
        }
    }

    // 24 faces of tesseract: choose 2 varying axes, fix other 2 at ±1
    // Bit layout: bit3=x, bit2=y, bit1=z, bit0=w
    for (int a = 0; a < 4; a++) {
        for (int b = a + 1; b < 4; b++) {
            // c, d are the fixed axes
            int fixed[2];
            int fi = 0;
            for (int ax = 0; ax < 4; ax++) {
                if (ax != a && ax != b) {
                    fixed[fi++] = ax;
                }
            }
            int c = fixed[0], d = fixed[1];

            for (int sc : {0, 1}) {
                for (int sd : {0, 1}) {
                    int base = (sc << c) | (sd << d);
                    int v0 = base;
                    int v1 = base | (1 << a);
                    int v2 = base | (1 << a) | (1 << b);
                    int v3 = base | (1 << b);
                    faceIndices.push_back((unsigned)v0);
                    faceIndices.push_back((unsigned)v1);
                    faceIndices.push_back((unsigned)v2);
                    faceIndices.push_back((unsigned)v0);
                    faceIndices.push_back((unsigned)v2);
                    faceIndices.push_back((unsigned)v3);
                }
            }
        }
    }

    // Flatten the edge pairs into a GL_LINES index list for wireframe rendering.
    std::vector<unsigned int> edgeIndices;
    edgeIndices.reserve(edges.size() * 2);
    for (const auto& e : edges) {
        edgeIndices.push_back((unsigned)e.first);
        edgeIndices.push_back((unsigned)e.second);
    }

    buffers.init(faceIndices, edgeIndices);
}
