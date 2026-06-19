#include "Minimap.h"

#include "Renderer.h"
#include <glm/gtc/matrix_transform.hpp>

Minimap::Minimap()
    : shader_("shaders/flat3d.vert", "shaders/flat3d.frag") {
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

Minimap::~Minimap() {
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
}

void Minimap::drawLines(const std::vector<glm::vec3>& pts, GLenum mode,
                        const glm::vec3& color, float alpha, float width) {
    if (pts.empty()) return;
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, pts.size() * sizeof(glm::vec3), pts.data(), GL_DYNAMIC_DRAW);
    shader_.setVec3("uColor", color);
    shader_.setFloat("uAlpha", alpha);
    glLineWidth(width);
    glDrawArrays(mode, 0, (GLsizei)pts.size());
    glLineWidth(1.0f);
    glBindVertexArray(0);
}

void Minimap::drawInWorld(Renderer& renderer, const glm::mat4& sceneMVP, float aspect,
                          const std::vector<glm::vec3>& trail,
                          const glm::vec3& boundsMin, const glm::vec3& boundsMax,
                          const glm::vec3& playerPos, const glm::vec3& facingDir,
                          const glm::vec3& anchorCenter, float anchorHalf) {
    // Model transform: pack the map-space bounds box into a small cube of half-size
    // `anchorHalf` centered at `anchorCenter` (in scene space, i.e. a corner of the
    // outer box). The cube's axes stay aligned with the scene's, so it never tilts
    // relative to the big box and W stays vertical.
    glm::vec3 center  = 0.5f * (boundsMin + boundsMax);
    glm::vec3 halfExt = glm::max(glm::vec3(1e-4f), 0.5f * (boundsMax - boundsMin));
    glm::mat4 M = glm::translate(glm::mat4(1.0f), anchorCenter) *
                  glm::scale(glm::mat4(1.0f), glm::vec3(anchorHalf) / halfExt) *
                  glm::translate(glm::mat4(1.0f), -center);
    glm::mat4 mvp = sceneMVP * M;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    shader_.use();
    shader_.setMat4("MVP", mvp);

    // Frame cube: the 12 edges of the bounds box (becomes the small anchored cube).
    const glm::vec3& a = boundsMin;
    const glm::vec3& b = boundsMax;
    glm::vec3 c[8] = {
        {a.x, a.y, a.z}, {b.x, a.y, a.z}, {b.x, a.y, b.z}, {a.x, a.y, b.z},
        {a.x, b.y, a.z}, {b.x, b.y, a.z}, {b.x, b.y, b.z}, {a.x, b.y, b.z},
    };
    int e[24] = {0,1, 1,2, 2,3, 3,0,  4,5, 5,6, 6,7, 7,4,  0,4, 1,5, 2,6, 3,7};
    std::vector<glm::vec3> frame;
    frame.reserve(24);
    for (int i = 0; i < 24; ++i) frame.push_back(c[e[i]]);
    drawLines(frame, GL_LINES, glm::vec3(0.55f, 0.58f, 0.66f), 1.0f, 1.5f);

    // Trajectory: the whole path as a white curve.
    if (trail.size() >= 2)
        drawLines(trail, GL_LINE_STRIP, glm::vec3(1.0f, 1.0f, 1.0f), 1.0f, 2.5f);

    // Facing arrow: a short cyan segment from the player along the heading, ball tip.
    glm::vec3 tip = playerPos;
    float flen = glm::length(facingDir);
    if (flen > 1e-5f) {
        glm::vec3 f = facingDir / flen;
        tip = playerPos + f * (glm::length(boundsMax - boundsMin) * 0.16f);
        std::vector<glm::vec3> seg = { playerPos, tip };
        drawLines(seg, GL_LINES, glm::vec3(0.25f, 0.9f, 1.0f), 1.0f, 2.5f);
    }

    // Ball at the tip: reuse the billboard marker (round, always-on-top).
    renderer.drawMarker(tip, 0.022f, aspect, glm::vec3(0.35f, 0.95f, 1.0f), 1.0f, mvp);
}
