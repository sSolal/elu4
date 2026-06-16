# Level 7 — Plane (4D course)

## Teaches (in isolation)
Now the hoop course **curves through 4D**: consecutive hoops are offset along W as well as
X/Z/Y, so flying it requires using *every* direction — including banking the depth axis (W)
into your flight path. The contrast with Level 6 (which stayed in a 3D slab) is the lesson.

## Camera / controls (`Mode::FLIGHT`)
Identical control scheme to Level 6. No new controls — the difficulty comes from the course
geometry, not new inputs. (If playtesting shows pilots can't reach W-offset hoops with the
Level 6 control set, revisit whether an explicit "W-bank" control is needed.)

## Scene
- A hoop course whose centreline is a curve with genuine W variation (e.g. a helix that
  also winds through W, or hoops placed so the natural flight path sweeps through all four
  axes).
- Visual aids: colour-code hoops by W-depth (the renderer already tints by W) so the pilot
  can read which hoops are "deeper".

## Win
- Pass through all hoops in order; optionally beat a target time.

## New engine systems required
- **None new** beyond Level 6's flight model, hoop triggers, and checkpoint sequence —
  this level is content/geometry on top of the Level 6 systems. (Validate that the flight
  model actually lets a pilot reach W-offset hoops.)
- Possibly a **W-depth indicator** on the HUD (how far ahead/behind the next hoop is in W).

## Open questions
- Is the Level 6 control set sufficient to fly an arbitrary 4D curve, or do we need a
  dedicated W-pitch/bank control? (Resolve during Level 6 playtest.)
- How to visually convey the next hoop's W-offset before you reach it (ghost ring? arrow?).
- Course shape: helix-through-W, or a hand-authored "interesting" 4D path?
