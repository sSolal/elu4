#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <vector>
#include "Shader.h"

class Renderer;  // for the billboard "ball" at the arrow tip

// A small 3D minimap cube that lives IN the scene, nestled into a corner of the
// outer boundary box. It is real world-space geometry drawn with the scene's
// camera (not a screen-space inset), so it sits in the corner of the 3D box and
// turns with it. It shows an axis-aligned wireframe cube (the X-Z-W floor volume,
// with world W mapped to the cube's vertical) containing only the player's whole
// trajectory as a white curve, plus a short facing arrow tipped with a ball. It
// deliberately never draws the maze itself.
//
// Points are passed in MAP space (caller maps world (x,z,w) -> (x, w, z); W is up).
// A model transform packs that bounds box into a small cube anchored at `anchor`.
class Minimap {
public:
    Minimap();
    ~Minimap();

    void drawInWorld(Renderer& renderer, const glm::mat4& sceneMVP, float aspect,
                     const std::vector<glm::vec3>& trail,
                     const glm::vec3& boundsMin, const glm::vec3& boundsMax,
                     const glm::vec3& playerPos, const glm::vec3& facingDir,
                     const glm::vec3& anchorCenter, float anchorHalf);

private:
    void drawLines(const std::vector<glm::vec3>& pts, GLenum mode,
                   const glm::vec3& color, float alpha, float width);

    Shader shader_;            // flat3d: MVP * pos, flat uColor/uAlpha
    GLuint vao_ = 0, vbo_ = 0; // dynamic line buffer (re-uploaded each draw)
};
