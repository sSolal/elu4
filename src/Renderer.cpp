#include "Renderer.h"
#include <cmath>
#include <algorithm>
#include <utility>

namespace {
    // Reorder a triangle index list far→near by each triangle's mean 4D depth so
    // transparent faces composite in painter's order (the far cell of a tesseract
    // is drawn before the near cell, instead of whatever the static list dictates).
    // vertData is the projected interleaved buffer (7 floats/vertex, depth at
    // offset 6); srcIdx is the static triangle list (groups of 3).
    void depthSortTriangles(const std::vector<float>& vertData,
                            const std::vector<unsigned int>& srcIdx,
                            std::vector<unsigned int>& out) {
        const size_t triCount = srcIdx.size() / 3;
        std::vector<std::pair<float, unsigned int>> keys;  // (depth key, triangle)
        keys.reserve(triCount);
        for (size_t t = 0; t < triCount; ++t) {
            unsigned int a = srcIdx[t * 3], b = srcIdx[t * 3 + 1], c = srcIdx[t * 3 + 2];
            // Sum of the three vertex depths — monotonic in the mean, so no /3 needed.
            float key = vertData[a * 7 + 6] + vertData[b * 7 + 6] + vertData[c * 7 + 6];
            keys.emplace_back(key, (unsigned int)t);
        }
        std::sort(keys.begin(), keys.end(),
                  [](const std::pair<float, unsigned int>& l,
                     const std::pair<float, unsigned int>& r) {
                      return l.first > r.first;  // farthest (largest depth) first
                  });
        out.resize(srcIdx.size());
        for (size_t i = 0; i < triCount; ++i) {
            unsigned int t = keys[i].second;
            out[i * 3]     = srcIdx[t * 3];
            out[i * 3 + 1] = srcIdx[t * 3 + 1];
            out[i * 3 + 2] = srcIdx[t * 3 + 2];
        }
    }

    // Indices into `instances`, ordered farthest 4D depth first. Works for any
    // instance type carrying a `glm::vec4 pos` (Instance4D, ObjectInstance).
    template <typename Inst>
    std::vector<size_t> depthSortInstances(const std::vector<Inst>& instances,
                                           const glm::vec4& camPos,
                                           const glm::mat4& camM) {
        std::vector<size_t> order(instances.size());
        for (size_t i = 0; i < instances.size(); ++i) order[i] = i;
        std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
            float da = -(camM * (instances[a].pos - camPos)).w;
            float db = -(camM * (instances[b].pos - camPos)).w;
            return da > db;  // farthest first
        });
        return order;
    }

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

    // "Reflection" depth aid: cast a soft dark silhouette of an object onto whichever
    // cube face it is nearest, fading in / tightening as it approaches. Reuses the
    // geometry already uploaded to `buf` (no re-upload) — the vertex shader flattens
    // it onto the face and the fragment shader fills it flat (uShadowMode).
    template <typename Buf>
    void drawFaceShadow(Shader& sh, Buf& buf, const std::vector<unsigned int>& triIdx,
                        const glm::vec4& center4D, const glm::mat4& camM,
                        const glm::vec4& camPos, float focal, const RenderSettings& vis) {
        const float kShadowRange = 0.22f;       // only objects this close to a face cast one

        glm::vec4 v = camM * (center4D - camPos);
        if (-v.w <= 1e-4f) return;              // centre behind the camera
        glm::vec3 pc = Math4D::project4Dto3D(v.x, v.y, v.z, v.w, focal);

        // Nearest face = the axis whose projected coordinate is closest to ±0.5.
        int axis = 0;
        for (int k = 1; k < 3; ++k)
            if (std::fabs(pc[k]) > std::fabs(pc[axis])) axis = k;
        float dist = glm::max(0.5f - std::fabs(pc[axis]), 0.0f);
        if (dist > kShadowRange) return;        // mid-cube: no shadow

        float nearness = 1.0f - dist / kShadowRange;            // 0 far … 1 touching face
        float value = (pc[axis] >= 0.0f ? 0.499f : -0.499f);    // just inside the face
        // Dark patch on light backgrounds; a pale patch on dark ones so it still reads.
        glm::vec3 shadowCol = (vis.bg == 0) ? glm::vec3(0.0f) : glm::vec3(0.55f);

        sh.setInt("uFlattenAxis", axis);
        sh.setFloat("uFlattenValue", value);
        sh.setVec3("uShadowCentroid", pc);
        sh.setFloat("uShadowSpread", glm::mix(1.45f, 1.0f, nearness));  // wide+blurry → tight
        sh.setInt("uShadowMode", 1);
        sh.setVec3("uShadowColor", shadowCol);
        sh.setFloat("uShadowAlpha", glm::mix(0.0f, 0.5f, nearness));    // fades in as it nears

        buf.drawSorted(triIdx);

        sh.setInt("uShadowMode", 0);            // restore for the next object's normal pass
        sh.setInt("uFlattenAxis", -1);
    }
}

Renderer::Renderer()
    : innerShader("shaders/inner.vert", "shaders/inner.frag"),
      wireShader("shaders/wire.vert", "shaders/wire.frag"),
      markerShader("shaders/marker.vert", "shaders/marker.frag"),
      wireEdgeVAO(0), wireEdgeVBO(0), wireEdgeEBO(0),
      markerVAO(0), markerVBO(0) {
    // Unit quad corners in [-1,1] for the screen-facing marker billboard.
    {
        const float corners[] = {
            -1.0f, -1.0f,  1.0f, -1.0f,  1.0f, 1.0f,
            -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, 1.0f,
        };
        glGenVertexArrays(1, &markerVAO);
        glGenBuffers(1, &markerVBO);
        glBindVertexArray(markerVAO);
        glBindBuffer(GL_ARRAY_BUFFER, markerVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(corners), corners, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
    }

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
    if (markerVAO)   glDeleteVertexArrays(1, &markerVAO);
    if (markerVBO)   glDeleteBuffers(1, &markerVBO);
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

    // Depth aids (T): vignette is a steady per-fragment darkening; the shadow pass
    // is off by default and flipped on per-object by drawFaceShadow().
    innerShader.setInt("uVignette", vis.depthAid == DepthAid::Vignette ? 1 : 0);
    innerShader.setInt("uShadowMode", 0);
    innerShader.setInt("uFlattenAxis", -1);
    innerShader.setFloat("uFlattenValue", 0.0f);
    innerShader.setVec3("uShadowCentroid", glm::vec3(0.0f));
    innerShader.setFloat("uShadowSpread", 1.0f);
    innerShader.setVec3("uShadowColor", glm::vec3(0.0f));
    innerShader.setFloat("uShadowAlpha", 0.0f);
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

    // Painter's order between instances: draw the farthest 4D depth first so nearer
    // objects paint over farther ones (transparency has no depth buffer to lean on).
    std::vector<size_t> order = depthSortInstances(instances, cam4D.pos, camM);

    std::vector<float> instVertData;
    std::vector<unsigned int> sortedIdx;

    for (size_t oi = 0; oi < order.size(); ++oi) {
        size_t i = order[oi];
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
            depthSortTriangles(instVertData, tesseract.faceIndices, sortedIdx);
            tbuf.drawSorted(sortedIdx);
            if (borders) {
                glLineWidth(lineWidthForDepth(cd, vis, 2.6f, 0.8f));
                innerShader.setBool("uLineMode", true);
                innerShader.setFloat("uBorderAmt", vis.borderAmt());
                tbuf.drawEdges();
                innerShader.setFloat("uBorderAmt", 0.0f);
            }
        }

        if (vis.depthAid == DepthAid::Reflection)
            drawFaceShadow(innerShader, tbuf, tesseract.faceIndices,
                           inst.pos, camM, cam4D.pos, focalLength, vis);
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

    // Painter's order between instances: farthest 4D depth first (see drawScene).
    std::vector<size_t> order = depthSortInstances(instances, cam4D.pos, camM);

    std::vector<float> objVertData;
    std::vector<unsigned int> sortedIdx;

    for (size_t oi = 0; oi < order.size(); ++oi) {
        size_t i = order[oi];
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
            depthSortTriangles(objVertData, obj.triangleIndices, sortedIdx);
            buf.drawSorted(sortedIdx);
            if (borders) {
                glLineWidth(lineWidthForDepth(cd, vis, 2.6f, 0.8f));
                innerShader.setBool("uLineMode", true);
                innerShader.setFloat("uBorderAmt", vis.borderAmt());
                buf.drawEdges();
                innerShader.setFloat("uBorderAmt", 0.0f);
            }
        }

        if (vis.depthAid == DepthAid::Reflection)
            drawFaceShadow(innerShader, buf, obj.triangleIndices,
                           inst.pos, camM, cam4D.pos, focalLength, vis);
    }

    glLineWidth(1.0f);
    glDepthMask(GL_TRUE);
}

void Renderer::drawPolyline(
    const Object4D& obj,
    ObjectBuffer& buf,
    const Camera4D& cam4D,
    const Math4D::Rotor4D& camOrientation,
    float focalLength,
    const glm::mat4& innerMVP,
    const RenderSettings& vis,
    const glm::vec3& color,
    float width
) {
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);
    setupInnerShader(innerMVP, vis);

    // The polyline's vertices are already in world space; a flat color is given by
    // setting colorA == colorB (collapses projectObjectInstance's W-gradient).
    ObjectInstance inst{glm::vec4(0.0f), Math4D::Rotor4D::identity(), color, color};

    std::vector<float> vertData;
    if (!projectObjectInstance(obj, inst, cam4D.pos, camOrientation, focalLength, vertData)) {
        glLineWidth(1.0f);
        glDepthMask(GL_TRUE);
        return;
    }
    buf.uploadVerts(vertData);

    innerShader.setFloat("uInstAlpha", 1.0f);
    innerShader.setBool("uLineMode", true);
    innerShader.setFloat("uBorderAmt", 0.0f);
    glLineWidth(width);
    buf.drawEdges();

    glLineWidth(1.0f);
    glDepthMask(GL_TRUE);
}

void Renderer::drawOuterCube(const glm::mat4& outerMVP) {
    wireShader.use();
    wireShader.setMat4("MVP", outerMVP);
    glBindVertexArray(wireEdgeVAO);
    glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, 0);
}

void Renderer::drawMarker(const glm::vec3& center, float sizeNDC, float aspect,
                          const glm::vec3& color, float alpha, const glm::mat4& mvp) {
    // Always-on-top overlay dot: no depth test, but keep the alpha blend the rest
    // of the scene uses. Restore depth testing afterwards.
    glDisable(GL_DEPTH_TEST);
    markerShader.use();
    markerShader.setMat4("MVP", mvp);
    markerShader.setVec3("uCenter", center);
    markerShader.setFloat("uSize", sizeNDC);
    markerShader.setFloat("uAspect", aspect);
    markerShader.setVec3("uColor", color);
    markerShader.setFloat("uAlpha", alpha);
    glBindVertexArray(markerVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
}
