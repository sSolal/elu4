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

## Resolved (as built)
- **Fixed jump** (tap Space when grounded → constant arc), reusing Level 4's `JUMP_VEL`.
  No charge-and-release.
- **No W-offset gaps.** The course is a straight chain of beams running forward along
  **-W** (the axis the third-person camera looks down, so W = run forward). All beams
  share X = Z = 0; the only sideways axes are X/Z, and the beams are **narrow** there —
  drift off and you fall. W-navigation is reserved for the later plane/maze levels.
- **Mixed up-and-across** layout: ~8 beams of varying length at varying heights (ups
  kept ≤ ~0.8 so a fixed jump clears them; longer gaps go downhill).
- **Sky decorations**: floating tesseracts scattered beside/above the course give motion
  parallax so forward progress reads over the void. Visual only (no colliders).
- No moving platforms (reserved for later).

Implemented in `src/levels/LeapLevel.{h,cpp}`. Each beam's visual is one slab; its
collider is a row of uniform-halfSize cubes tiled along W (the engine's colliders are
uniform hypercubes). Kill-plane at `KILL_Y` respawns the avatar at the last beam landed
on; reaching + dwelling on the goal beam wins.
