#include "Renderer.h"
#include <cmath>
#include <cstdio>
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
    // instance type carrying a `glm::vec4 pos` (ObjectInstance).
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

    // Occluder budget — must match inner.frag (MAX_OCC, MAX_PLANES). A solid with
    // more facets than kMaxPlanesPerOcc is skipped from the shader occluder set (it
    // still renders and self-culls on the CPU) so one dense mesh can't dominate.
    // Occluder budget — must match inner.frag (MAX_OCC, MAX_PLANES). A solid with
    // more facets than kMaxPlanesPerOcc is skipped from the shader occluder set (it
    // still renders and self-culls on the CPU) so one dense mesh can't dominate.
    constexpr int kMaxOccluders    = 64;
    constexpr int kMaxPlanes       = 512;
    constexpr int kMaxPlanesPerOcc = 16;

    // Per-frame occluder arrays packed for the inner-shader uniform arrays. Each
    // occluder is a convex solid = intersection of its facet half-spaces; planes are
    // flattened, each occluder owning the slice [planeOfs, planeOfs+planeCnt).
    struct OccUpload {
        std::vector<glm::vec4> center;   // N centers (4D camera space)
        std::vector<glm::vec4> planeN;   // facet outward unit normals (camera space)
        std::vector<float>     planeD;   // facet offsets (local; shader adds dot(n,center))
        std::vector<int>       planeOfs; // first plane index per occluder
        std::vector<int>       planeCnt; // plane count per occluder
        std::vector<int>       slotOf;   // instance index -> occluder slot (-1 if dropped)
        int count = 0;
        int slot(size_t instIdx) const {
            return instIdx < slotOf.size() ? slotOf[instIdx] : -1;
        }
    };

    // Build the occluder set for one draw call: every instance is the convex solid
    // { x : dot(hullN[i], x) <= hullD[i] } (local space). When there are more than the
    // cap, keep the nearest (depthSortInstances is far→near, so the tail).
    template <typename Inst>
    OccUpload buildOccluders(const std::vector<Inst>& instances,
                             const std::vector<glm::vec4>& hullN,
                             const std::vector<float>& hullD,
                             const glm::vec4& camPos, const glm::mat4& camM) {
        OccUpload up;
        const int n = (int)instances.size();
        up.slotOf.assign(n, -1);
        const int planes = (int)hullN.size();
        if (planes == 0 || planes > kMaxPlanesPerOcc) return up;  // skip dense/empty hulls

        std::vector<size_t> idx;
        if (n <= kMaxOccluders) {
            idx.resize(n);
            for (int i = 0; i < n; ++i) idx[i] = (size_t)i;
        } else {
            std::vector<size_t> order = depthSortInstances(instances, camPos, camM);
            for (int i = n - kMaxOccluders; i < n; ++i) idx.push_back(order[i]);
            static bool warned = false;
            if (!warned) {
                warned = true;
                std::fprintf(stderr,
                    "[4D occlusion] %d solid instances exceed cap %d; using nearest.\n",
                    n, kMaxOccluders);
            }
        }
        up.center.reserve(idx.size());
        up.planeN.reserve(idx.size() * planes);
        up.planeD.reserve(idx.size() * planes);
        up.planeOfs.reserve(idx.size());
        up.planeCnt.reserve(idx.size());
        for (size_t s = 0; s < idx.size(); ++s) {
            size_t i = idx[s];
            if ((int)up.planeN.size() + planes > kMaxPlanes) {
                static bool pwarned = false;
                if (!pwarned) {
                    pwarned = true;
                    std::fprintf(stderr,
                        "[4D occlusion] plane budget %d exceeded; dropping far occluders.\n",
                        kMaxPlanes);
                }
                break;
            }
            glm::mat4 R = camM * instances[i].orientation.toMatrix();  // local normal -> camera
            up.slotOf[i] = (int)up.center.size();
            up.planeOfs.push_back((int)up.planeN.size());
            up.planeCnt.push_back(planes);
            up.center.push_back(camM * (instances[i].pos - camPos));
            for (int p = 0; p < planes; ++p) {
                up.planeN.push_back(R * hullN[p]);
                up.planeD.push_back(hullD[p]);
            }
        }
        up.count = (int)up.center.size();
        return up;
    }

    // Push an occluder set into the inner shader (or disable the pass).
    void uploadOccluders(Shader& sh, const OccUpload& occ, bool enabled) {
        const bool on = enabled && occ.count > 0;
        sh.setBool("uOcclude", on);
        sh.setInt("uOccCount", on ? occ.count : 0);
        sh.setInt("uSelfIndex", -1);  // per-instance value set in the draw loop
        if (on) {
            sh.setVec4Array("uOccCenter", occ.center.data(), occ.count);
            sh.setIntArray("uOccPlaneOfs", occ.planeOfs.data(), occ.count);
            sh.setIntArray("uOccPlaneCnt", occ.planeCnt.data(), occ.count);
            sh.setVec4Array("uOccPlaneN", occ.planeN.data(), (int)occ.planeN.size());
            sh.setFloatArray("uOccPlaneD", occ.planeD.data(), (int)occ.planeD.size());
        }
    }

    // 4D back-cell culling for ONE convex instance: emit only the triangles whose
    // facet faces the 4D eye. Facet f (outward normal n, offset d, local space) faces
    // the eye iff focal·n_cam.w − dot(n_cam, center) > d, where n_cam = camMobjM·n (the
    // matrix is orthonormal, so normals transform like directions). A surface triangle
    // bounds up to two facets (triFace); it is drawn when EITHER faces the eye. For a
    // convex solid the front facets are exactly the visible near shell, so this is
    // exact (no interpolation, unlike the per-fragment shader path).
    void cullBackCells(const std::vector<glm::vec4>& hullN,
                       const std::vector<float>& hullD,
                       const std::vector<unsigned int>& triIdx,
                       const std::vector<glm::ivec2>& triFace,
                       const glm::mat4& camMobjM, const glm::vec4& center, float focal,
                       std::vector<unsigned int>& out) {
        const int nf = (int)hullN.size();
        std::vector<char> front(nf);
        for (int f = 0; f < nf; ++f) {
            glm::vec4 nCam = camMobjM * hullN[f];
            float g = focal * nCam.w - glm::dot(nCam, center);
            front[f] = (g > hullD[f]) ? 1 : 0;
        }
        out.clear();
        out.reserve(triIdx.size());
        const size_t triCount = triIdx.size() / 3;
        for (size_t t = 0; t < triCount; ++t) {
            glm::ivec2 fc = (t < triFace.size()) ? triFace[t] : glm::ivec2(-1, -1);
            bool visible = (fc.x >= 0 && front[fc.x]) || (fc.y >= 0 && front[fc.y]);
            if (visible) {
                out.push_back(triIdx[t * 3]);
                out.push_back(triIdx[t * 3 + 1]);
                out.push_back(triIdx[t * 3 + 2]);
            }
        }
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

void Renderer::setupInnerShader(const glm::mat4& innerMVP, float focalLength, const RenderSettings& vis) {
    innerShader.use();
    innerShader.setMat4("MVP", innerMVP);
    innerShader.setMat4("innerView", glm::mat4(1.0f));
    // 4D occlusion: off by default; box-family draw calls re-enable it after
    // uploading their occluder set. uFocal feeds the ray reconstruction.
    innerShader.setFloat("uFocal", focalLength);
    innerShader.setBool("uOcclude", false);
    innerShader.setInt("uOccCount", 0);
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

void Renderer::drawObjects(
    const std::vector<ObjectInstance>& instances,
    const Object4D& obj,
    ObjectBuffer& buf,
    const Camera4D& cam4D,
    const Math4D::Rotor4D& camOrientation,
    float focalLength,
    const glm::mat4& innerMVP,
    const RenderSettings& vis,
    const std::vector<ObjectInstance>* occluders,
    const Object4D* occluderObj
) {
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);

    setupInnerShader(innerMVP, focalLength, vis);
    const bool wire    = (vis.geom == GeomMode::Wireframe);
    const bool borders = (vis.geom == GeomMode::SolidBorders);
    const glm::mat4 camM = camOrientation.toMatrix();

    // 4D hidden-surface removal. Occluders are the drawn instances themselves (self-
    // occlusion) unless an external occluder set is supplied — then this call's
    // instances are occluded by THOSE solids and never self-occlude. Only solid
    // (occludes=true) occluder meshes can occlude; polylines keep drawing in full.
    const bool external = (occluders != nullptr && occluderObj != nullptr);
    const Object4D& occObj = external ? *occluderObj : obj;
    const std::vector<ObjectInstance>& occInsts = external ? *occluders : instances;
    const bool occOn = vis.occlude4D && occObj.occludes;
    OccUpload occ;
    if (occOn) occ = buildOccluders(occInsts, occObj.hullN, occObj.hullD, cam4D.pos, camM);
    uploadOccluders(innerShader, occ, occOn);

    // Painter's order between instances: farthest 4D depth first.
    std::vector<size_t> order = depthSortInstances(instances, cam4D.pos, camM);

    std::vector<float> objVertData;
    std::vector<unsigned int> sortedIdx;
    std::vector<unsigned int> visIdx;

    for (size_t oi = 0; oi < order.size(); ++oi) {
        size_t i = order[oi];
        const auto& inst = instances[i];
        if (!projectObjectInstance(obj, inst, cam4D.pos, camOrientation, focalLength, objVertData))
            continue;
        buf.uploadVerts(objVertData);

        glm::vec4 center_i = camM * (inst.pos - cam4D.pos);
        float cd = -center_i.w;  // instance-centre 4D depth
        int seed = (int)i * 3 + (int)std::floor(cd * 4.0f) * 101;
        float instA = (vis.pulse == PulseMode::Off) ? 1.0f
                                                    : pulseFactor(vis.pulse, vis.time, seed);
        innerShader.setFloat("uInstAlpha", instA);
        // Self-skip only when self-occluding; with an external occluder set the drawn
        // instance is not in it, so it has no self slot.
        innerShader.setInt("uSelfIndex", (occOn && !external) ? occ.slot(i) : -1);

        if (wire) {
            glLineWidth(lineWidthForDepth(cd, vis, 2.0f, 0.7f));
            innerShader.setBool("uLineMode", true);
            innerShader.setFloat("uBorderAmt", 0.0f);
            buf.drawEdges();
        } else {
            innerShader.setBool("uLineMode", false);
            const std::vector<unsigned int>* idxList = &obj.triangleIndices;
            if (occOn) {
                cullBackCells(obj.hullN, obj.hullD, obj.triangleIndices, obj.triFace,
                              camM * inst.orientation.toMatrix(), center_i, focalLength, visIdx);
                idxList = &visIdx;
            }
            depthSortTriangles(objVertData, *idxList, sortedIdx);
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
    setupInnerShader(innerMVP, focalLength, vis);  // leaves uOcclude=false: guide lines always show

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
