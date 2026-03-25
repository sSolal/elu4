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
        GLuint vao, vbo, ebo;
        size_t indexCount;

        void init(const std::vector<unsigned int>& indices);
        void uploadVerts(const std::vector<float>& vertData);
        void draw();
        void destroy();
    };

    Buffers buffers;

    Tesseract();
    ~Tesseract() { buffers.destroy(); }

private:
    void buildTopology();
};
