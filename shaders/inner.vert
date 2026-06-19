#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in float aDepth;   // camera-relative 4D depth (-v.w)

uniform mat4 MVP;
uniform mat4 innerView;

// Face-shadow pass (Reflection depth aid): flatten this geometry onto one cube
// face. uFlattenAxis is -1 when off, else 0/1/2 (x/y/z). The two in-plane axes are
// pushed away from uShadowCentroid by uShadowSpread (>1 = wider/blurrier when far),
// and the flatten axis is pinned to uFlattenValue (±0.499, just inside the face).
uniform int   uFlattenAxis;
uniform float uFlattenValue;
uniform vec3  uShadowCentroid;
uniform float uShadowSpread;

out vec3 fragColor;
out float vDepth;
out vec3 vWorldPos;        // cube-local position (innerView is identity → == aPos)

void main() {
    // Cube-local position. For the shadow pass, flatten/spread it onto a face.
    vec3 lp = aPos;
    if (uFlattenAxis >= 0) {
        lp = uShadowCentroid + (lp - uShadowCentroid) * uShadowSpread;
        lp[uFlattenAxis] = uFlattenValue;
    }

    gl_Position = MVP * vec4(lp, 1.0);
    fragColor = aColor;
    vDepth = aDepth;

    // Compute position in cube-local space (for clipping to [-0.5, 0.5])
    vec4 worldPos = innerView * vec4(lp, 1.0);
    vWorldPos = worldPos.xyz;

    // Clip to cube bounds [-0.5, 0.5] in world space
    gl_ClipDistance[0] = worldPos.x + 0.5;  // x >= -0.5
    gl_ClipDistance[1] = 0.5 - worldPos.x;  // x <= 0.5
    gl_ClipDistance[2] = worldPos.y + 0.5;  // y >= -0.5
    gl_ClipDistance[3] = 0.5 - worldPos.y;  // y <= 0.5
    gl_ClipDistance[4] = worldPos.z + 0.5;  // z >= -0.5
    gl_ClipDistance[5] = 0.5 - worldPos.z;  // z <= 0.5
}
