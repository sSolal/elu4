#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <vector>
#include "Shader.h"
#include "Math4D.h"
#include "Scene.h"
#include "Tesseract.h"
#include "Camera.h"
#include "ObjectBuffer.h"
#include "Level2.h"
#include "RenderSettings.h"

class Renderer {
public:
    Renderer();
    ~Renderer();

    void drawScene(
        const std::vector<Instance4D>& instances,
        Tesseract::Buffers& tbuf,
        const Camera4D& cam4D,
        const Math4D::Rotor4D& camOrientation,
        float focalLength,
        const Tesseract& tesseract,
        const glm::mat4& innerMVP,
        const RenderSettings& vis
    );

    void drawObjects(
        const std::vector<ObjectInstance>& instances,
        const Object4D& obj,
        ObjectBuffer& buf,
        const Camera4D& cam4D,
        const Math4D::Rotor4D& camOrientation,
        float focalLength,
        const glm::mat4& innerMVP,
        const RenderSettings& vis
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

private:
    // Set the shared inner-shader uniforms (MVP + all depth/alpha/pulse modes).
    void setupInnerShader(const glm::mat4& innerMVP, const RenderSettings& vis);

    Shader innerShader, wireShader;
    GLuint wireEdgeVAO, wireEdgeVBO, wireEdgeEBO;
};
