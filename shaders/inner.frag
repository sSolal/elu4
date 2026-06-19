#version 330 core
in vec3 fragColor;
in float vDepth;            // camera-relative 4D depth (-v.w)
in vec3 vWorldPos;          // cube-local position in [-0.5, 0.5]^3
out vec4 FragColor;

// Depth aids (T). Vignette darkens fragments by their distance from the cube centre
// (the camera's W line of sight). Shadow mode emits a flat dark patch for the
// face-projection "reflection" pass.
uniform int   uVignette;    // 1 = darken away from cube centre
uniform int   uShadowMode;  // 1 = flat shadow fill (skip normal shading)
uniform vec3  uShadowColor;
uniform float uShadowAlpha;

// Depth-cue colour (N): 0 = Fog (far desaturates), 1 = Normal, 2 = DarkenFar
uniform int   uDepthCue;
// Transparency mode (F): 0 = Mid, 1 = Light, 2 = Heavy, 3 = NearTransparent
uniform int   uAlphaMode;
uniform float uDepthNear;
uniform float uDepthFar;
uniform float uVibrance;    // saturation boost applied to every base colour
uniform vec3  uFogColor;    // sky colour that far objects fade into
uniform float uFogStrength; // how strongly distance pulls toward uFogColor

// Per-instance opacity multiplier — drives the X-ray pulse (V). 1.0 = no fade.
// Whole objects fade together because it is set per draw call, not per vertex.
uniform float uInstAlpha;

// Line rendering: draw an opaque edge that shares the surface colour, nudged
// toward uBorderTarget by uBorderAmt (0 = plain wireframe, >0 = solid+borders).
uniform bool  uLineMode;
uniform vec3  uBorderTarget;
uniform float uBorderAmt;

void main() {
    // Face-shadow pass: flat dark patch, no shading/fog. Overlapping flattened
    // triangles accumulate via alpha blending → denser core, soft "blob shadow".
    if (uShadowMode == 1) {
        FragColor = vec4(uShadowColor, uShadowAlpha);
        return;
    }

    // Normalised depth in [0,1] across the configured window.
    float nd = clamp((vDepth - uDepthNear) / max(uDepthFar - uDepthNear, 1e-3), 0.0, 1.0);

    // --- Vibrance: make the scene colours more vivid before the depth cue ---
    float l0 = dot(fragColor, vec3(0.299, 0.587, 0.114));
    vec3 col = clamp(mix(vec3(l0), fragColor, uVibrance), 0.0, 1.0);

    // --- Colour cue (N) ---
    // Fog factor: how much this fragment is swallowed by distance haze. Capped at
    // 0.9 so even the most distant surface keeps ~10% of its original colour and
    // opacity — it asymptotes to "barely visible", never fully invisible.
    // Only the Fog cue produces haze; the other cues leave alpha untouched.
    float fog = (uDepthCue == 0) ? clamp(nd * uFogStrength, 0.0, 1.0) * 0.9 : 0.0;
    if (uDepthCue == 0) {
        // Distance fog: far objects fade into the sky colour, so they read as
        // receding into the background.
        col = mix(col, uFogColor, fog);
    } else if (uDepthCue == 2) {
        // DarkenFar: far objects fade toward black.
        col *= (1.0 - 0.7 * nd);
    }

    // --- Vignette (T): darken away from the cube centre ---
    // Distance from the cube centre runs 0 (origin) → 0.5 (face centre) → ~0.87
    // (corner). Centre stays full-bright; periphery dims, so a central object reads
    // as "aligned with the camera's W axis".
    if (uVignette == 1) {
        col *= mix(1.0, 0.22, smoothstep(0.0, 0.72, length(vWorldPos)));
    }

    if (uLineMode) {
        // Edges share the surface colour, nudged darker/lighter to stand out;
        // their opacity follows the object so a pulsing object fades entirely.
        // Fog also thins them out so far outlines dissolve into the haze.
        col = mix(col, uBorderTarget, uBorderAmt);
        FragColor = vec4(col, clamp(uInstAlpha * (1.0 - fog), 0.0, 1.0));
        return;
    }

    // --- Alpha (F) ---
    float a;
    if (uAlphaMode == 1)      a = 0.20;
    else if (uAlphaMode == 2) a = 0.85;
    else if (uAlphaMode == 3) a = mix(0.15, 0.9, nd);   // near = more transparent
    else                      a = 0.35;                 // Mid (default)

    a *= uInstAlpha;        // X-ray pulse
    a *= (1.0 - fog);       // distance haze: far surfaces thin out, not just recolour
    FragColor = vec4(col, clamp(a, 0.0, 1.0));
}
