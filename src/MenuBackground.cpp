#include "MenuBackground.h"
#include "Shader.h"
#include <GL/glew.h>

MenuBackground::MenuBackground()
    : shader(std::make_unique<Shader>("shaders/menubg.vert", "shaders/menubg.frag")) {
    // An empty VAO is still required by the core profile to issue a draw; the
    // vertex shader builds the fullscreen triangle from gl_VertexID alone.
    glGenVertexArrays(1, &vao);
}

MenuBackground::~MenuBackground() {
    if (vao) glDeleteVertexArrays(1, &vao);
}

void MenuBackground::render(float time, int w, int h) {
    // Opaque fullscreen fill: no depth, no blend, so it cleanly replaces the
    // cleared background. The main loop re-enables depth test before the scene.
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    shader->use();
    shader->setFloat("uTime", time);
    // The Shader API has no setVec2; upload uRes directly.
    int loc = glGetUniformLocation(shader->ID, "uRes");
    if (loc >= 0) glUniform2f(loc, (float)w, (float)h);

    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    // Restore the loop's persistent GL state (depth test + blend are enabled
    // once at init and assumed on by the scene/ImGui paths).
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
}
