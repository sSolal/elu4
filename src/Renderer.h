#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <vector>
#include "Shader.h"
#include "Math4D.h"
#include "Scene.h"
#include "Camera.h"
#include "ObjectBuffer.h"
#include "Level2.h"
#include "RenderSettings.h"

class Renderer {
public:
    Renderer();
    ~Renderer();

    // Draw all `instances` of one mesh with 4D hidden-surface removal. By default the
    // occluder set is built from `instances` themselves (each occludes the others).
    // Pass `occluders` + `occluderObj` to instead be occluded by a DIFFERENT instance
    // set (e.g. draw the floor/goal occluded by the maze walls); the drawn instances
    // are then not self-occluders. Both nullptr (the default) = self-occlusion, so
    // existing callers are unaffected.
    void drawObjects(
        const std::vector<ObjectInstance>& instances,
        const Object4D& obj,
        ObjectBuffer& buf,
        const Camera4D& cam4D,
        const Math4D::Rotor4D& camOrientation,
        float focalLength,
        const glm::mat4& innerMVP,
        const RenderSettings& vis,
        const std::vector<ObjectInstance>* occluders = nullptr,
        const Object4D* occluderObj = nullptr
    );

    // Draw a polyline mesh (vertices already in world space) as connected line
    // segments, ALWAYS visible regardless of vis.geom — unlike drawObjects, which
    // only emits edges in wireframe/border modes. Used for HUD-style guide lines.
    void drawPolyline(
        const Object4D& obj,
        ObjectBuffer& buf,
        const Camera4D& cam4D,
        const Math4D::Rotor4D& camOrientation,
        float focalLength,
        const glm::mat4& innerMVP,
        const RenderSettings& vis,
        const glm::vec3& color,
        float width
    );

    void drawOuterCube(const glm::mat4& outerMVP);

    // Draw a small round screen-facing marker (a "dot") at a 3D point, projected
    // with `mvp`. Constant on-screen size (uSize is half-height in NDC), drawn as
    // an always-on-top overlay (no depth test). `aspect` is the viewport w/h.
    void drawMarker(const glm::vec3& center, float sizeNDC, float aspect,
                    const glm::vec3& color, float alpha, const glm::mat4& mvp);

private:
    // Set the shared inner-shader uniforms (MVP + all depth/alpha/pulse modes).
    // focalLength feeds the 4D-occlusion ray reconstruction (uFocal).
    void setupInnerShader(const glm::mat4& innerMVP, float focalLength, const RenderSettings& vis);

    Shader innerShader, wireShader, markerShader;
    GLuint wireEdgeVAO, wireEdgeVBO, wireEdgeEBO;
    GLuint markerVAO, markerVBO;
};
