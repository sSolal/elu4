# Level 5 — Leap

## Teaches (in isolation)
The **Y axis is special** (up/gravity). Platforming: jump the avatar from platform to
platform across gaps. Builds timing and a feel for gravity, in third person so the arc is
readable.

## Camera / controls (`Mode::ORBIT`)
Same orbit-camera + avatar control as Level 4. Adds emphasis on the **jump** mechanic
(charge/arc). Horizontal movement is in the X-Z-W floor; jumps are along Y.

## Scene
- A series of platforms (tesseract slabs) at varying heights and horizontal positions,
  separated by gaps over a void/kill-plane.
- Platforms may be offset in **W** as well as X/Z, so the player must also line up the
  depth axis before jumping (gentle introduction; keep early jumps purely X/Z).
- A goal platform at the end.

## Win / lose
- **Win:** reach the goal platform.
- **Lose:** fall below the kill-plane → respawn at last platform / start.

## New engine systems required
- **Jump impulse + arc** on the avatar (`velY` impulse when `onGround`); physics gravity
  already pulls it back down.
- **Kill-plane / respawn**: detect `pos.y < threshold`, reset avatar to a checkpoint.
- **Checkpoints**: remember the last safely-landed platform.
- **Landing detection** per platform (physics `onGround` + which AABB was contacted).

## Open questions
- Fixed jump or charge-and-release for variable height?
- Do W-offset platforms appear this level or stay reserved for the plane levels? (Lean
  toward keeping Leap mostly 3D-feeling: X/Z/Y, with at most one W-gap as a teaser.)
- Moving platforms? (Probably later.)
