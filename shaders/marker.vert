#version 330 core
layout (location = 0) in vec2 aCorner;   // unit-quad corner in [-1,1]

uniform mat4  MVP;
uniform vec3  uCenter;   // marker centre (inner-cube local space)
uniform float uSize;     // half-size in NDC (vertical)
uniform float uAspect;   // viewport w/h, keeps the dot round

out vec2 vCorner;

void main() {
    // Project the centre, then offset the corners in clip space so the marker is
    // always screen-facing and a constant on-screen size, at the centre's depth.
    vec4 c = MVP * vec4(uCenter, 1.0);
    c.xy += vec2(aCorner.x * uSize / uAspect, aCorner.y * uSize) * c.w;
    gl_Position = c;
    vCorner = aCorner;
}
