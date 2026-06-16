#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in float aDepth;   // camera-relative 4D depth (-v.w)

uniform mat4 MVP;
uniform mat4 innerView;

out vec3 fragColor;
out float vDepth;

void main() {
    gl_Position = MVP * vec4(aPos, 1.0);
    fragColor = aColor;
    vDepth = aDepth;

    // Compute position in cube-local space (for clipping to [-0.5, 0.5])
    vec4 worldPos = innerView * vec4(aPos, 1.0);

    // Clip to cube bounds [-0.5, 0.5] in world space
    gl_ClipDistance[0] = worldPos.x + 0.5;  // x >= -0.5
    gl_ClipDistance[1] = 0.5 - worldPos.x;  // x <= 0.5
    gl_ClipDistance[2] = worldPos.y + 0.5;  // y >= -0.5
    gl_ClipDistance[3] = 0.5 - worldPos.y;  // y <= 0.5
    gl_ClipDistance[4] = worldPos.z + 0.5;  // z >= -0.5
    gl_ClipDistance[5] = 0.5 - worldPos.z;  // z <= 0.5
}
