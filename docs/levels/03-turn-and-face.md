# Level 3 — Turn & Face

## Teaches (in isolation)
**Retained heading.** This is the first level where turning the head *stays* (no
spring-back). Turning is a `ZW`/`XW` rotor composition, so swinging your view literally
rotates the depth axis (W) into your strafe plane and vice-versa — the four spatial axes
are interchangeable by turning. First deliberate taste of 4D rotation, framed as
"looking around", not as a gimmick.

## Camera / controls
First-person FPS. **Head-return OFF** (`headReturn = false`) — the whole lesson is that
the head now stays where you point it. No axis lock; full movement available, but the
task rewards turning rather than translating.

## Scene
- The player stands at the centre of an open space.
- Several distinct landmark objects placed around the player in the horizontal 3-space
  (different X, Z, **and W** offsets), each a different colour.
- Because some landmarks are offset in W, they start "zoomed" small/large; turning to face
  them (rotating W into view) brings them to a natural facing.

## Win
- Face each landmark in turn (a landmark counts as "faced" when it is near screen-centre
  and within a forward cone). Visit all of them → win. A HUD checklist shows progress.

## New engine systems required
- **Facing test**: project a target into camera space and check it is within an angular
  cone of the forward (−W) axis. Reuse the camera orientation matrix already computed in
  rendering.
- **Per-target "visited" state + HUD checklist** (small ImGui list).
- Optional: a compass/indicator pointing toward the next unvisited landmark.

## Open questions
- How many landmarks (4? 6 — one per ±X/±Z and ±W)?
- Should turning be only yaw (ZW/XW) or also allow the player to re-base "forward" so a
  W-offset landmark becomes straight ahead? Decide how aggressive the rotation lesson is.
- Do we need any movement at all, or is this purely a rotation level?
