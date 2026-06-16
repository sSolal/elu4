#include "Renderer.h"
#include <cmath>

namespace {
    // Deterministic [0,1) hash for value noise.
    float hash1(int n) {
        unsigned int x = (unsigned int)n * 374761393u + 1u;
        x = (x ^ (x >> 13)) * 1274126177u;
        return float((x ^ (x >> 16)) & 0xFFFFFFu) / float(0x1000000);
    }
    // Smooth 1-D value noise in [0,1].
    float valueNoise1D(float t) {
        float fi = std::floor(t);
        int i = (int)fi;
        float f = t - fi;
        float a = hash1(i), b = hash1(i + 1);
        float u = f * f * (3.0f - 2.0f * f);
        return a + (b - a) * u;
    }
    // Per-instance opacity multiplier in [0,1]: whole-object fade, desynced by seed
    // so each object dims at its own time and you can see the others through it.
    float pulseFactor(PulseMode mode, float time, int seed) {
        if (mode == PulseMode::Sine) {
            const float T = 9.0f;                      // long temporal period
            float phase = (float)seed * 2.399963f;     // golden-angle spread → desync
            return 0.5f - 0.5f * std::cos(6.2831853f * time / T + phase);
        }
        // Noise: independent slow fade in/out per object, stretched so it fully clears.
        float v = valueNoise1D(time * 0.12f + (float)seed * 13.37f);
        return glm::clamp((v - 0.15f) / 0.7f, 0.0f, 1.0f);
    }
    // Edge thickness shrinks with 4D depth so far objects' outlines recede.
    float lineWidthForDepth(float centerDepth, const RenderSettings& vis, float wNear, float wFar) {
        float nd = glm::clamp((centerDepth - vis.depthNear) /
                              glm::max(vis.depthFar - vis.depthNear, 1e-3f), 0.0f, 1.0f);
        return wNear + (wFar - wNear) * nd;
    }
}

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

void Renderer::setupInnerShader(const glm::mat4& innerMVP, const RenderSettings& vis) {
    innerShader.use();
    innerShader.setMat4("MVP", innerMVP);
    innerShader.setMat4("innerView", glm::mat4(1.0f));
    innerShader.setInt("uDepthCue", (int)vis.depth);
    innerShader.setInt("uAlphaMode", (int)vis.alpha);
    innerShader.setFloat("uDepthNear", vis.depthNear);
    innerShader.setFloat("uDepthFar", vis.depthFar);
    innerShader.setFloat("uVibrance", vis.vibrance);
    innerShader.setVec3("uFogColor", vis.bgColor());
    innerShader.setFloat("uFogStrength", vis.fogStrength);
    innerShader.setFloat("uInstAlpha", 1.0f);
    innerShader.setBool("uLineMode", false);
    innerShader.setVec3("uBorderTarget", vis.borderTarget());
    innerShader.setFloat("uBorderAmt", 0.0f);
}

void Renderer::drawScene(
    const std::vector<Instance4D>& instances,
    Tesseract::Buffers& tbuf,
    const Camera4D& cam4D,
    const Math4D::Rotor4D& camOrientation,
    float focalLength,
    const Tesseract& tesseract,
    const glm::mat4& innerMVP,
    const RenderSettings& vis
) {
    glDisable(GL_CULL_FACE);  // show both sides of faces
    glDepthMask(GL_FALSE);    // don't write to depth buffer

    setupInnerShader(innerMVP, vis);
    const bool wire    = (vis.geom == GeomMode::Wireframe);
    const bool borders = (vis.geom == GeomMode::SolidBorders);
    const glm::mat4 camM = camOrientation.toMatrix();

    std::vector<float> instVertData;

    for (size_t i = 0; i < instances.size(); ++i) {
        const auto& inst = instances[i];
        if (!projectInstance(inst, cam4D.pos, camOrientation, focalLength, tesseract, instVertData))
            continue;
        tbuf.uploadVerts(instVertData);

        float cd = -(camM * (inst.pos - cam4D.pos)).w;  // instance-centre 4D depth
        int seed = (int)i * 3 + (int)std::floor(cd * 4.0f) * 101;
        float instA = (vis.pulse == PulseMode::Off) ? 1.0f
                                                    : pulseFactor(vis.pulse, vis.time, seed);
        innerShader.setFloat("uInstAlpha", instA);

        if (wire) {
            glLineWidth(lineWidthForDepth(cd, vis, 2.0f, 0.7f));
            innerShader.setBool("uLineMode", true);
            innerShader.setFloat("uBorderAmt", 0.0f);
            tbuf.drawEdges();
        } else {
            innerShader.setBool("uLineMode", false);
            tbuf.draw();
            if (borders) {
                glLineWidth(lineWidthForDepth(cd, vis, 2.6f, 0.8f));
                innerShader.setBool("uLineMode", true);
                innerShader.setFloat("uBorderAmt", vis.borderAmt());
                tbuf.drawEdges();
                innerShader.setFloat("uBorderAmt", 0.0f);
            }
        }
    }

    glLineWidth(1.0f);
    glDepthMask(GL_TRUE);
}

void Renderer::drawObjects(
    const std::vector<ObjectInstance>& instances,
    const Object4D& obj,
    ObjectBuffer& buf,
    const Camera4D& cam4D,
    const Math4D::Rotor4D& camOrientation,
    float focalLength,
    const glm::mat4& innerMVP,
    const RenderSettings& vis
) {
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);

    setupInnerShader(innerMVP, vis);
    const bool wire    = (vis.geom == GeomMode::Wireframe);
    const bool borders = (vis.geom == GeomMode::SolidBorders);
    const glm::mat4 camM = camOrientation.toMatrix();

    std::vector<float> objVertData;

    for (size_t i = 0; i < instances.size(); ++i) {
        const auto& inst = instances[i];
        if (!projectObjectInstance(obj, inst, cam4D.pos, camOrientation, focalLength, objVertData))
            continue;
        buf.uploadVerts(objVertData);

        float cd = -(camM * (inst.pos - cam4D.pos)).w;  // instance-centre 4D depth
        int seed = (int)i * 3 + (int)std::floor(cd * 4.0f) * 101;
        float instA = (vis.pulse == PulseMode::Off) ? 1.0f
                                                    : pulseFactor(vis.pulse, vis.time, seed);
        innerShader.setFloat("uInstAlpha", instA);

        if (wire) {
            glLineWidth(lineWidthForDepth(cd, vis, 2.0f, 0.7f));
            innerShader.setBool("uLineMode", true);
            innerShader.setFloat("uBorderAmt", 0.0f);
            buf.drawEdges();
        } else {
            innerShader.setBool("uLineMode", false);
            buf.draw();
            if (borders) {
                glLineWidth(lineWidthForDepth(cd, vis, 2.6f, 0.8f));
                innerShader.setBool("uLineMode", true);
                innerShader.setFloat("uBorderAmt", vis.borderAmt());
                buf.drawEdges();
                innerShader.setFloat("uBorderAmt", 0.0f);
            }
        }
    }

    glLineWidth(1.0f);
    glDepthMask(GL_TRUE);
}

void Renderer::drawOuterCube(const glm::mat4& outerMVP) {
    wireShader.use();
    wireShader.setMat4("MVP", outerMVP);
    glBindVertexArray(wireEdgeVAO);
    glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, 0);
}
