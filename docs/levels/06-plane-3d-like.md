# Level 6 — Plane (3D-like)

## Teaches (in isolation)
Flying a vehicle that can **climb and dive** (Y) but otherwise has the **two horizontal
rotational degrees of freedom**. The insight: if you never pitch up or down, flying here is
*exactly* like piloting an ordinary 3D plane with no gravity — a familiar anchor before we
add genuine 4D motion in Level 7.

## Camera / controls (`Mode::FLIGHT` — NEW)
- Continuous forward thrust along the plane's nose.
- **Pitch** (climb/dive) tilts the nose along Y.
- **Two horizontal turns** (yaw within the X-Z-W horizontal space): the plane banks/turns
  among the horizontal axes.
- No gravity (or negligible) — it's flight, not falling.
- Camera: chase cam behind the plane (a flavour of the orbit cam, or its own follow cam).

## Scene
- Open sky with a course of **hoops** to fly through.
- This level's hoops all lie in a single 3D slab (constant or gently varying W), so a pilot
  who keeps level flies an ordinary 3D slalom.

## Win
- Pass through all hoops in order (optionally timed).

## New engine systems required
- **Flight model**: constant-speed forward integration along the nose vector; pitch + two
  horizontal-turn controls compose the plane's `Rotor4D` orientation. Gravity disabled.
- **Chase camera** following the plane's orientation.
- **Hoop trigger**: detect the plane passing through a ring (point-in-disk / crossing test).
  Hoops can be modelled as `Object4D` tori or as ring-arranged markers.
- **Ordered checkpoint sequence + timer + HUD**.

## Open questions
- Control surface mapping (which keys pitch vs turn)?
- Do we model a torus hoop mesh, or fake the ring with cubes? (A real ring reads best.)
- Stall/min-speed behaviour, or pure constant speed?
- Reuse the orbit-cam follow logic from Level 4, or a dedicated flight cam?
