#version 330 core
in vec3 fragColor;
in float vDepth;            // camera-relative 4D depth (-v.w)
in vec3 vWorldPos;          // cube-local position in [-0.5, 0.5]^3
in vec3 vProjPos;           // raw 4D→3D projected position (for 4D occlusion)
out vec4 FragColor;

// ---- 4D hidden-surface removal -------------------------------------------------
// Each box-family solid is fed in as an occluder: a 4-volume |axis_k.(v-center)| <=
// half_k (k=0..3) in 4D camera space. A fragment is discarded when some occluder's
// solid lies nearer the 4D camera than the fragment along the SAME 4D line of sight
// (the W projection ray) — i.e. genuinely hidden in 4D, even though we never write a
// 3D depth buffer (so things merely overlapping in the 3D view still both show).
#define MAX_OCC 32
uniform bool  uOcclude;             // master switch (C toggle)
uniform int   uOccCount;
uniform int   uSelfIndex;           // occluder slot of the instance being drawn; skipped
uniform float uFocal;               // W-perspective focal length; 4D eye sits at w=uFocal
uniform vec4  uOccCenter[MAX_OCC];  // occluder center, 4D camera space
uniform vec4  uOccAxis[MAX_OCC * 4];// 4 unit slab axes per occluder, 4D camera space
uniform vec4  uOccHalf[MAX_OCC];    // per-axis half-extents (x,y,z,w)

// The fragment's 4D line of sight is the ray v(t) = (P*(uFocal - t), t) through its
// 3D image point P (t = camera-space W). Return the W-interval [tlo,thi] in which
// occluder o's solid covers this ray (tlo>thi ⇒ the ray misses it).
vec2 occInterval(int o, vec3 P) {
    vec4 c = uOccCenter[o];
    vec4 h = uOccHalf[o];
    float tlo = -1e30, thi = 1e30;
    for (int k = 0; k < 4; ++k) {
        vec4  ax = uOccAxis[o * 4 + k];
        float Ak = dot(ax.xyz, P);
        float Bk = dot(ax, c);
        // axis_k . v(t) - axis_k.c = (Ak*uFocal - Bk) + t*(ax.w - Ak)
        float pk = Ak * uFocal - Bk;
        float qk = ax.w - Ak;
        float hk = h[k];
        if (abs(qk) < 1e-6) {
            if (abs(pk) > hk) return vec2(1.0, -1.0);       // ray parallel & outside slab
        } else {
            float ta = (-hk - pk) / qk;
            float tb = ( hk - pk) / qk;
            tlo = max(tlo, min(ta, tb));
            thi = min(thi, max(ta, tb));
        }
    }
    return vec2(tlo, thi);
}

// True if some solid lies nearer the 4D eye than this fragment along the same ray.
bool occluded4D() {
    vec3 P = vProjPos;
    // Exact fragment depth: the fragment sits on its OWN box's surface, so its W is
    // one of that box's two ray crossings (tlo/thi from the slabs — no interpolation,
    // unlike -vDepth which is projectively warped and would speckle). The interpolated
    // depth only disambiguates which crossing.
    float td = -vDepth;
    float tFrag = td;
    if (uSelfIndex >= 0) {
        vec2 s = occInterval(uSelfIndex, P);
        if (s.x <= s.y) tFrag = (abs(td - s.x) < abs(td - s.y)) ? s.x : s.y;
    }
    // A fragment is hidden only by solid that lies strictly BETWEEN it and the camera
    // along the ray: camera-frame W in (tFrag, 0). Larger W = nearer the camera; the
    // camera plane is at W=0 (in front is W<0). Clamping the occluder's near reach to
    // 0 is essential — without it, cubes the camera has already moved past (now behind
    // it, W>0) would still "occlude" everything ahead and blink the corridor out.
    // Including the fragment's own box culls its far faces / hidden border edges.
    // EPS lets abutting/coincident surfaces tie instead of flickering.
    const float EPS = 1e-3;
    for (int o = 0; o < uOccCount; ++o) {
        vec2 iv = occInterval(o, P);
        if (iv.x > iv.y) continue;                   // ray misses this occluder
        float lo = max(iv.x, tFrag);                 // not behind the fragment
        float hi = min(iv.y, 0.0);                   // not behind the camera plane
        if (hi > lo + EPS) return true;              // solid in front of the fragment
    }
    return false;
}

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

    // 4D hidden-surface removal: drop fragments genuinely occluded in 4D.
    if (uOcclude && occluded4D()) discard;

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
