#pragma once

#include <memory>

class Shader;

// The Elua title-screen's animated background: one fullscreen pass running
// shaders/menubg.frag (night gradient + breathing lamp glow + drift + vignette).
// The hand-drawn drift strands and logo mark in UiTheme are layered on top via
// ImGui. Construct after the GL context exists; call render() each menu frame
// after glClear and before ImGui renders.
class MenuBackground {
public:
    MenuBackground();
    ~MenuBackground();

    // Draw the fullscreen background. `time` is seconds (animation phase),
    // `w`/`h` the framebuffer size (for aspect + grain). Manages its own GL
    // state (depth test off, blend off) and restores nothing the caller relies
    // on beyond what the main loop re-sets each frame.
    void render(float time, int w, int h);

private:
    std::unique_ptr<Shader> shader;
    unsigned int vao = 0;     // empty VAO; the vertex shader synthesises the tri
};
