#include "Renderer.h"

Renderer::Renderer()
    : innerShader("shaders/inner.vert", "shaders/inner.frag"),
      wireShader("shaders/wire.vert", "shaders/wire.frag"),
      wireEdgeVAO(0), wireEdgeVBO(0), wireEdgeEBO(0) {
    // Wireframe cube (edges only, not triangles)
    float wireVertices[] = {
        // 8 vertices of cube at (±0.5, ±0.5, ±0.5)
        -0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,
        -0.5f,  0.5f, -0.5f,
        -0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f
    };
    unsigned int wireIndices[] = {
        // 12 edges: 4 bottom, 4 top, 4 vertical
        0, 1,  1, 2,  2, 3,  3, 0,  // bottom face
        4, 5,  5, 6,  6, 7,  7, 4,  // top face
        0, 4,  1, 5,  2, 6,  3, 7   // vertical edges
    };

    glGenVertexArrays(1, &wireEdgeVAO);
    glGenBuffers(1, &wireEdgeVBO);
    glGenBuffers(1, &wireEdgeEBO);

    glBindVertexArray(wireEdgeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, wireEdgeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(wireVertices), wireVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, wireEdgeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(wireIndices), wireIndices, GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

Renderer::~Renderer() {
    if (wireEdgeVAO) glDeleteVertexArrays(1, &wireEdgeVAO);
    if (wireEdgeVBO) glDeleteBuffers(1, &wireEdgeVBO);
    if (wireEdgeEBO) glDeleteBuffers(1, &wireEdgeEBO);
}

void Renderer::drawScene(
    const std::vector<Instance4D>& instances,
    Tesseract::Buffers& tbuf,
    const Camera4D& cam4D,
    const Camera4D::Angles& angles,
    float focalLength,
    const Tesseract& tesseract,
    const glm::mat4& innerMVP
) {
    glDisable(GL_CULL_FACE);  // show both sides of faces
    glDepthMask(GL_FALSE);    // don't write to depth buffer

    innerShader.use();
    innerShader.setFloat("uAlpha", 0.35f);
    innerShader.setMat4("MVP", innerMVP);
    innerShader.setMat4("innerView", glm::mat4(1.0f));

    std::vector<float> instVertData;

    for (const auto& inst : instances) {
        // Project instance; skip if culled
        if (!projectInstance(inst, cam4D.pos, angles, focalLength, tesseract, instVertData)) {
            continue;
        }

        // Upload and draw
        tbuf.uploadVerts(instVertData);
        tbuf.draw();
    }

    glDepthMask(GL_TRUE);
}

void Renderer::drawOuterCube(const glm::mat4& outerMVP) {
    wireShader.use();
    wireShader.setMat4("MVP", outerMVP);
    glBindVertexArray(wireEdgeVAO);
    glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, 0);
}
