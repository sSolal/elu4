#version 330 core

// Elua title-screen atmosphere: a port of the website's layered background
// gradients into one pass. Night vertical gradient, a brass lamp glow pooled at
// the top that breathes, a softer teal drift glow upper-right, a faint flowing
// drift band, an edge vignette, and a whisper of grain. y = 1 is the top.

in vec2 vUV;
out vec4 FragColor;

uniform float uTime;
uniform vec2  uRes;

// Palette (matches src/UiTheme.cpp / the website tokens).
const vec3 NIGHT_BOT = vec3(  7.0,  10.0,  18.0) / 255.0; // #070a12
const vec3 NIGHT_TOP = vec3( 11.0,  16.0,  25.0) / 255.0; // #0b1019
const vec3 LAMP      = vec3(227.0, 168.0,  87.0) / 255.0; // #e3a857
const vec3 DRIFT     = vec3(143.0, 185.0, 168.0) / 255.0; // #8fb9a8

// Soft elliptical radial glow: 1 at the centre, fading to 0 at the rim.
float glow(vec2 uv, vec2 c, vec2 r) {
    float d = length((uv - c) / r);
    return smoothstep(1.0, 0.0, d);
}

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    vec2 uv = vUV;

    // Base night, a touch lighter toward the top.
    vec3 col = mix(NIGHT_BOT, NIGHT_TOP, smoothstep(0.0, 1.0, uv.y));

    // Lamp pooled at top-centre, breathing on the website's ~11s cadence.
    float breathe = 0.85 + 0.15 * (0.5 + 0.5 * sin(uTime * 6.28318 / 11.0));
    float lamp = glow(uv, vec2(0.5, 1.02), vec2(0.72, 0.62));
    col += LAMP * lamp * 0.17 * breathe;

    // Softer teal drift glow, upper-right.
    float drift = glow(uv, vec2(0.86, 1.0), vec2(0.5, 0.46));
    col += DRIFT * drift * 0.085;

    // One faint flowing drift band low in the frame (the current, moving).
    float bandMask = smoothstep(0.0, 0.45, uv.y) * smoothstep(0.95, 0.45, uv.y);
    float band = 0.5 + 0.5 * sin(uv.x * 6.28318 * 1.4 - uTime * 0.35 + uv.y * 3.0);
    col += DRIFT * band * bandMask * 0.03;

    // Edge vignette to seat the panels and keep the title legible.
    float vig = smoothstep(1.35, 0.45, length((uv - 0.5) * vec2(1.12, 1.0)));
    col *= mix(0.62, 1.0, vig);

    // Whisper of animated grain to kill banding in the gradients.
    col += (hash(vUV * uRes + uTime) - 0.5) * 0.015;

    FragColor = vec4(col, 1.0);
}
