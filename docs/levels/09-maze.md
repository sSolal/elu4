# Level 9 — Maze

## Teaches (in isolation)
**Spatial navigation** through a 4D maze: corridors and junctions laid out across the
X-Z-W floor. Consolidates the player's ability to hold a 4D layout in their head and to
turn/move deliberately toward an exit. No new movement — pure wayfinding.

## Camera / controls
First-person FPS, full unrestricted controls. Head-return off (you need to look down side
corridors and keep facing that way).

## Scene
- A maze built from walls (tesseract slabs) forming passages in the X-Z-W space. Start with
  a maze that is mostly planar (X-Z) with a few W-junctions, so it's solvable but uses the
  extra axis.
- A clearly marked entrance and an exit/goal.
- Optional: breadcrumbs or a fog-of-war so the player must actually map it.

## Win
- Reach the exit.

## New engine systems required
- **Maze generation or authoring**: a data format for walls/cells. Could be procedural
  (grid + carve) or hand-authored JSON. The framework already supports placing many
  `Instance4D` walls and registering AABBs.
- **Exit trigger** (proximity → win), reusing the Corridor goal pattern.
- Optional: **minimap / breadcrumb** system, or intentionally none to keep it challenging.

## Open questions
- Procedural vs hand-authored maze? (Procedural scales; hand-authored teaches better early.)
- How "4D" should it be — mostly X-Z with W shortcuts, or genuinely 3-axis branching (risk:
  disorienting)?
- Any aids (compass, breadcrumbs), or deliberately bare?
- Maze size / expected solve time.
