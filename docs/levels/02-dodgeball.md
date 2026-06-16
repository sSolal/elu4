# Level 2 — Dodgeball

## Teaches (in isolation)
Free strafing in the **horizontal plane** and the realisation that, perpendicular to the
incoming axis, you have a full **2D escape plane** — the four directions ±X and ±Z — not
just left/right.

## Camera / controls
First-person FPS (`LevelControls::Mode::FPS`). **No axis lock** on X/Z (the point is to
strafe). Forward/back along W still works but is useless for dodging. **Head-return ON**
(same gentle spring as Corridor) so the player keeps facing the incoming balls.

## Scene
- A flat floor (reuse the tesseract-grid floor pattern from `Scene.cpp`).
- Balls (hyperspheres — `objects/hypersphere.json`, or simple tesseracts to start) spawn
  far away along one axis and travel straight toward the player's position along that axis.
- Telegraph each ball's lane briefly before it launches.

## Win / lose
- **Win:** survive N balls (or T seconds) without being hit.
- **Lose:** a ball's AABB overlaps the player → reset / retry.

## New engine systems required
- **Moving projectiles**: a per-frame `update` that advances spawned instances along a
  velocity. Today scenes are static; need a small list of `{Instance4D, glm::vec4 velocity,
  float ttl}` updated each frame and culled when out of range.
- **Hit detection** between a projectile and the player body (4D AABB-vs-AABB; physics
  already has AABB math we can borrow, or a simple distance check on the player radius).
- **Spawn scheduler**: timed waves. Needs a per-level timer (accumulate `dt`).
- **Lose/retry flow**: currently only `checkWin()` exists; add a soft reset (re-`load()` or
  reposition player + clear projectiles).

## Open questions
- Balls along **W** (depth) would read as growing dots — confusing. Prefer balls along
  **X or Z** so they approach in a legible way, and the "extra" escape direction is the
  other horizontal axis. Confirm which axis balls travel along.
- Difficulty curve: ball speed/frequency ramp.
- Telegraph style (shadow on floor? colour flash at the spawn end?).
