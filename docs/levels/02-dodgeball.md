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

## Resolved approach (as built)
- **Balls travel along W**, straight at the player. The head-return spring keeps the
  player looking down −W, so W is the *only* direction they can perceive motion from;
  balls along X/Z would be unseeable. They read as growing dots — which is the intended
  telegraph — and you dodge by **strafing in X/Z** out of the incoming lane.
- **Aim:** each ball's X/Z offset is a Gaussian centred on the player's current position,
  so balls come roughly at you. σ tightens over the run (3.0 → 1.5) to raise difficulty.
- **Difficulty ramp** over the 45 s round: spawn interval 1.2 s → 0.45 s, speed 6 → 12 u/s.
- **Win:** survive 45 s. **Hit:** soft reset (clear balls, restart timer), keep playing.
- The floor is a checkerboard of cube blocks tiling the (X,Z,W) ground volume, giving an
  X/Z position reference and a depth gradient along W.

## Possible follow-ups
- Floor "shadow" telegraph directly under each far ball (deferred — growing dot suffices).
- Multiple simultaneous lanes per wave at high difficulty.
