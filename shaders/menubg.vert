#version 330 core

// Fullscreen triangle generated from gl_VertexID — no VBO needed; the caller
// binds an empty VAO and draws 3 vertices. vUV spans [0,1] over the screen
// (y = 1 at the top), with the corners running slightly past for the triangle.
out vec2 vUV;

void main() {
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    vUV = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
