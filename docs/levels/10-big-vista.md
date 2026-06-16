# Level 10 — Big Vista

## Teaches (in isolation)
**Reading distance and landscape.** A single very large scene with detail both right next to
the camera and very far away, so the player learns to interpret the viewport: what is near
vs far, how the W-depth tint and perspective scaling encode distance, and how to pick out
a far landmark and navigate toward it. The capstone perception level.

## Camera / controls
First-person FPS, full unrestricted controls. Generous movement speed so traversing the big
space is pleasant.

## Scene
- One large open landscape spanning a wide range in X-Z-W: foreground props (close, large,
  detailed), midground features, and distant landmarks (tiny, heavily W-tinted).
- Strong, legible depth cues: the renderer's W-based colour gradient plus perspective
  scaling do most of the work; arrange objects so near/far is unambiguous.
- A far-off goal landmark the player must identify from the vista and walk to.

## Win
- Identify the correct distant landmark (per a HUD hint) and reach it.

## New engine systems required
- **Large-scene performance**: many instances at varying distances. Verify the existing
  per-instance frustum/behind-camera cull in `projectInstance` is enough; may need
  distance-based culling or LOD if frame-rate drops.
- **Far-plane / focal tuning**: ensure distant objects remain visible and legibly tiny;
  may need to expose or auto-tune focal length and the GL projection far plane.
- **Landmark identification UI**: a hint ("reach the tall blue spire") + arrival trigger.

## Open questions
- How big is "big" — and does performance hold? (Profile early.)
- Should focal length be fixed, player-adjustable (it already is via UP/DOWN), or scripted?
- Are explicit distance markers wanted, or should distance be read purely from scale/tint?
- Single goal, or a "tour" of several far landmarks to really drill distance-reading?
