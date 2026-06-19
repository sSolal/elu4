#pragma once
// A world-space ImGui panel for VR: render the menu / HUD into an offscreen
// texture, then draw that texture on a quad floating in the 3D scene so it is
// visible in the headset. Header-only and only included by main.cpp in the VR
// build (HAVE_OPENXR), so it needs no CMake entry of its own.
#ifdef HAVE_OPENXR

#include <GL/glew.h>
#include <glm/glm.hpp>
#include "Shader.h"

class VRPanel {
public:
    VRPanel(int w, int h) : w_(w), h_(h),
                            shader_("shaders/panel.vert", "shaders/panel.frag") {
        // Offscreen colour target the ImGui canvas is rendered into.
        glGenFramebuffers(1, &fbo_);
        glGenTextures(1, &tex_);
        glBindTexture(GL_TEXTURE_2D, tex_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w_, h_, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glGenRenderbuffers(1, &depth_);
        glBindRenderbuffer(GL_RENDERBUFFER, depth_);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w_, h_);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Unit quad in the XY plane (-0.5..0.5). ImGui's OpenGL backend already
        // flips Y via its ortho projection when rendering into the FBO, so the
        // canvas's top sits at the texture's top (v=1) — standard UVs show it
        // upright on the quad (top vertex → v=1, bottom → v=0).
        const float verts[] = {
            // x      y     z     u     v
            -0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
             0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
             0.5f,  0.5f, 0.0f, 1.0f, 1.0f,
            -0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
             0.5f,  0.5f, 0.0f, 1.0f, 1.0f,
            -0.5f,  0.5f, 0.0f, 0.0f, 1.0f,
        };
        glGenVertexArrays(1, &vao_);
        glGenBuffers(1, &vbo_);
        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glBindVertexArray(0);
    }

    ~VRPanel() {
        glDeleteFramebuffers(1, &fbo_);
        glDeleteTextures(1, &tex_);
        glDeleteRenderbuffers(1, &depth_);
        glDeleteVertexArrays(1, &vao_);
        glDeleteBuffers(1, &vbo_);
    }

    int width()  const { return w_; }
    int height() const { return h_; }
    float aspect() const { return (float)w_ / (float)h_; }

    // Bind the offscreen target and clear it to a dark background. The caller
    // then issues ImGui draw commands (ImGui_ImplOpenGL3_RenderDrawData) into it.
    void bindForImGui() const {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glViewport(0, 0, w_, h_);
        glClearColor(0.06f, 0.07f, 0.09f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    // Draw the panel quad with the given model→clip matrix (per eye).
    void drawInWorld(const glm::mat4& mvp) const {
        shader_.use();
        shader_.setMat4("MVP", mvp);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex_);
        shader_.setInt("uTex", 0);
        glBindVertexArray(vao_);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }

private:
    int w_, h_;
    GLuint fbo_ = 0, tex_ = 0, depth_ = 0, vao_ = 0, vbo_ = 0;
    Shader shader_;
};

#endif // HAVE_OPENXR
