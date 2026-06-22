# Quick Start: 4D Asset Visualizer

The visualizer is a side tool for inspecting and framing the game's assets. It shares
the game's real rendering pipeline (4D occlusion, depth cues, the W-gradient inner
shader) and the same Elua UI theme, so what you see matches the game.

## Run

Always run from the project root (so `shaders/` and `assets/` resolve):

```bash
./run.sh            # builds everything
./build/visualizer                 # opens empty; use "Open asset..."
./build/visualizer assets/bed.json # or open a file directly
```

Click **Open asset...** to pick any `assets/*.json` with the native file dialog.

## Controls

The camera **orbits the object** — it always points at it and slides over the
4-sphere around it.

- **u / l** — orbit in the ZW plane
- **j / o** — orbit in the XW plane
- **i / k** — orbit in the YW plane
- **mouse wheel** — zoom (orbit distance)
- **Esc** — quit

**View direction widget** (top-right): four axis bars (X/Y/Z/W) show the direction the
camera looks *from*. The **−/+** button beside each axis snaps the camera to look
straight from that axis (e.g. **+W** jumps to "look from +W").

**Asset panel** (top-left): Geometry (Solid / Wire / Borders), Alpha, Depth cue,
Background, the 4D-occlusion toggle, and Focal / Distance sliders.

## Assets

Assets live in `assets/*.json`. An asset is a list of **parts**; each part is either a
procedural primitive (`box`, `ground`, `hypercube`, `hypersphere`) or an explicit
vertex `mesh`, with a local position/orientation and two-tone colours. The game loads
these same files (single source of truth — see `assets/bed.json` placed by
`scripts/scenes/bedroom.lua`). See **VISUALIZER.md** for the full format.
