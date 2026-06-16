# Level 4 — Third Person

## Teaches (in isolation)
Controlling a **4D avatar** seen from outside, with a camera that always points at it and
orbits around it. Switching from "I am the camera" to "I steer a character" makes the
avatar's degrees of freedom legible: it can run forward/back, strafe horizontally, turn
both ways, and jump.

## Camera / controls (`Mode::ORBIT` — NEW)
- The camera is locked onto the avatar at a fixed offset and follows it; player input
  rotates the camera *around* the avatar (orbit), and rotates the avatar's heading.
- Movement keys drive the **avatar** in its own frame: forward/back, strafe ±, jump.
- The existing `Camera4D` is the *eye*; we add an avatar transform (position +
  orientation) that the camera tracks.

## Scene
- A simple bounded arena with a clear floor and a few reference props so motion is visible.
- A goal pad to walk the avatar onto.

## Win
- Bring the avatar to the goal pad.

## New engine systems required
- **Avatar entity**: position (`glm::vec4`), heading (`Rotor4D`), velocity; driven by the
  physics body instead of the camera. Reuse `PhysicsBody`/`PhysicsWorld` for the avatar.
- **Orbit camera mode**: camera position = avatar position + rotated offset; view looks at
  the avatar. New branch in the control handling (today `processInput` is FPS-only) — likely
  a separate `Camera4D` method or a mode switch driven by `LevelControls::mode`.
- **Avatar rendering**: render the avatar mesh (a tesseract or a custom `Object4D`) at its
  transform, in addition to the scene.
- **Jump**: add an upward `velY` impulse on key press when `onGround` (physics already
  tracks `onGround` and gravity).

## Open questions
- Orbit input mapping (which keys orbit the camera vs turn the avatar)?
- Fixed follow distance vs adjustable zoom?
- How is the avatar's W-extent shown so the player understands it is a 4D body?
