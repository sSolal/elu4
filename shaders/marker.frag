#version 330 core
in vec2 vCorner;
out vec4 FragColor;

uniform vec3  uColor;
uniform float uAlpha;

void main() {
    // Round the quad into a soft-edged disc.
    float r = length(vCorner);
    if (r > 1.0) discard;
    float a = uAlpha * smoothstep(1.0, 0.55, r);
    FragColor = vec4(uColor, a);
}
