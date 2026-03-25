#version 330 core
layout (location = 0) in vec4 aPos;
uniform mat4 MVP;

void main() {
    gl_Position = MVP * vec4(aPos.xyz, 1.0);
}
