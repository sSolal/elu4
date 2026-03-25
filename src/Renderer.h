#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <vector>
#include "Shader.h"
#include "Scene.h"
#include "Tesseract.h"
#include "Camera.h"
#include "ObjectBuffer.h"
#include "Level2.h"

class Renderer {
public:
    Renderer();
    ~Renderer();

    void drawScene(
        const std::vector<Instance4D>& instances,
        Tesseract::Buffers& tbuf,
        const Camera4D& cam4D,
        const Camera4D::Angles& angles,
        float focalLength,
        const Tesseract& tesseract,
        const glm::mat4& innerMVP
    );

    void drawObjects(
        const std::vector<ObjectInstance>& instances,
        const Object4D& obj,
        ObjectBuffer& buf,
        const Camera4D& cam4D,
        const Camera4D::Angles& angles,
        float focalLength,
        const glm::mat4& innerMVP
    );

    void drawOuterCube(const glm::mat4& outerMVP);

private:
    Shader innerShader, wireShader;
    GLuint wireEdgeVAO, wireEdgeVBO, wireEdgeEBO;
};
