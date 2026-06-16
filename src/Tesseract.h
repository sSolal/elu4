#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <array>
#include <vector>

class Tesseract {
public:
    static constexpr float HS = 0.3f;

    // Static 4D topology data
    std::array<glm::vec4, 16> verts4D;
    std::vector<std::pair<int, int>> edges;
    std::vector<unsigned int> faceIndices;

    // GPU buffers for rendering faces
    struct Buffers {
        GLuint vao, vbo, ebo, edgeEbo, sortedEbo;
        size_t indexCount, edgeIndexCount;

        void init(const std::vector<unsigned int>& indices,
                  const std::vector<unsigned int>& edgeIndices);
        void uploadVerts(const std::vector<float>& vertData);
        void draw();        // solid faces (GL_TRIANGLES), static index order
        // Solid faces drawn back-to-front using a caller-supplied, per-frame
        // depth-sorted index list (painter's order for correct transparency).
        void drawSorted(const std::vector<unsigned int>& sortedIndices);
        void drawEdges();   // wireframe edges (GL_LINES)
        void destroy();
    };

    Buffers buffers;

    Tesseract();
    ~Tesseract() { buffers.destroy(); }

private:
    void buildTopology();
};
