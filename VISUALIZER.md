# 4D Asset Visualizer

A side tool for inspecting and editing the game's assets. It is **not** a separate
renderer: it drives the same `Renderer::drawObjects` pipeline the game uses (per-fragment
4D occlusion, depth cues, the W-gradient inner shader) under the same Elua UI theme, so an
asset looks identical to how it appears in-game.

## Running

Run from the project root so `shaders/` and `assets/` resolve:

```bash
./build/visualizer                  # opens empty — use the "Open asset..." button
./build/visualizer assets/bed.json  # or open a file directly
```

The **Open asset...** button uses the native OS file dialog (tinyfiledialogs),
defaulting to the `assets/` folder.

## Orbit camera

The camera orbits the object: it always points at the object's centre and slides over
the 4-sphere around it ("move laterally / up / down, but always face it").

| Keys      | Orbit plane |
|-----------|-------------|
| `u` / `l` | ZW          |
| `j` / `o` | XW          |
| `i` / `k` | YW          |
| wheel     | zoom (orbit distance) |
| `Esc`     | quit        |

### View-direction widget

Top-right: four axis bars (X red, Y green, Z orange, W blue) showing the unit direction
the camera looks **from** — which, because the camera always looks at the object, also
gives its position. A **−** and **+** button flank each axis; clicking one snaps all the
weight onto that axis, jumping the camera to look straight from ±X / ±Y / ±Z / ±W.

### Asset panel

Top-left: the asset name + part count, the **Open** button, and the shared render
toggles — Geometry (Solid / Wire / Borders), Alpha, Depth cue, Background, the 4D
occlusion switch, and Focal / Distance sliders.

## Asset format

Assets live in `assets/*.json`. An asset is a **named list of parts** in the asset's
local space; each part is one mesh placed at a local position/orientation with two-tone
colours. This is the single source of truth: the game loads the very same files (e.g.
`scripts/scenes/bedroom.lua` calls `engine.load_asset("assets/bed.json")` and places it).

```json
{
  "name": "Bed",
  "parts": [
    { "shape": "box", "half": [1.6, 0.25, 0.9, 1.6], "pos": [0, 0.5, 0, 0],
      "colorA": [0.78, 0.32, 0.32], "colorB": [0.55, 0.22, 0.22] },
    { "shape": "hypersphere", "radius": 0.5, "pos": [0, 2, 0, 0], "colorA": [0.2, 0.8, 0.3] },
    { "shape": "mesh", "ref": "tree.json", "pos": [0, 0, 0, 0] }
  ]
}
```

**Part fields**

- `shape`: `box` | `ground` | `hypercube` | `hypersphere` | `mesh`
  - primitives use `half` (vec4); `hypersphere` also takes `radius`; any part may set a
    uniform `scale`.
  - `mesh`: explicit geometry — either inline `vertices` / `edges` / `cells` (the legacy
    single-mesh format), or `ref` pointing at another file resolved relative to this one.
    This is how fully vertex-defined meshes are stored (the end goal); primitives are the
    convenient shorthand.
- `pos` (vec4 local offset, default 0), `rot` (per-plane angles `{xy,xz,xw,yz,yw,zw}` in
  radians, or omit for identity), `colorA` / `colorB` (vec3; `colorB` defaults to
  `colorA`), `solid` (bool — when placed in the game, registers a collider).

**Back-compat:** a file with top-level `vertices` / `edges` / `cells` and no `parts`
(e.g. `assets/tree.json`) loads as a single mesh part.

The loader is `src/Asset.{h,cpp}` (`loadAsset`), shared by the game and the visualizer.

## How it shares the game pipeline

- The object instance sits at the world origin; the orbit moves a 4D camera around it.
  Because the object's centre always projects to the 3D origin, a fixed `Camera3D` frames
  it while the orbit rotor `Q` (passed as the camera orientation) moves the eye.
- Each part is drawn with its own self-occluding `drawObjects` call — exactly how the game
  draws each inline box — so occlusion, depth fog, borders, and colours all match in-game.
- UI uses `elua::applyEluaStyle()` / `loadEluaFonts()` and the palette accessors in
  `src/UiTheme.h`; the view widget mirrors the in-game facing gauge (`src/HudWidgets.h`).

## Headless capture

`SCREENSHOT=<path> [SHOT_FRAME=N] ./build/visualizer assets/bed.json` renders N frames
then writes a binary PPM and exits (mirrors the game's `SCREENSHOT`), for quick checks.
