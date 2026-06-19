#version 330 core
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uTex;

void main() {
    // Opaque floating panel — the ImGui canvas rendered to an offscreen texture.
    FragColor = vec4(texture(uTex, vUV).rgb, 1.0);
}
