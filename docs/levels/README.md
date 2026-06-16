# Level curriculum

A teaching campaign for 4D navigation. Each level introduces **one** perception or
movement in isolation, building on the previous ones. We deliberately avoid
"4D-is-weird-from-3D" gimmicks (through-the-wall, cross-section slicing, inside-out
reveals) in favour of levels that feel like natural movement and navigation.

Axes as the player experiences them:
- **X, Z** — the horizontal strafe plane (the "four directions")
- **W** — forward/depth. The camera looks down −W, so moving along W reads as *zooming*.
- **Y** — up (gravity)

## Status

| # | Level | Camera | Teaches | Status |
|---|-------|--------|---------|--------|
| 1 | Corridor | FP | W-translation = zoom | **Built** |
| 2 | Dodgeball | FP | ±X/±Z strafe (four directions) | **Built** |
| 3 | Turn & Face | FP | Retained yaw rotates depth into strafe | Design |
| 4 | Third Person | Orbit | Controlling a 4D avatar | Design |
| 5 | Leap | Orbit | Y-axis platforming / jump | Design |
| 6 | Plane (3D-like) | Flight | Up/down + 2 horizontal rotation DOF | Design |
| 7 | Plane (4D course) | Flight | Hoops curve through 4D | Design |
| 8 | Forest Fetch | FP | Mapping space, NPC quest | Design |
| 9 | Maze | FP | Spatial navigation | Design |
| 10 | Big Vista | FP | Reading distance/landscape | Design |

## Framework

Levels subclass `Level` (`src/Level.h`) and are registered in
`src/LevelRegistry.cpp`. Per-level control config (axis locks, head-return, control
mode) lives in `LevelControls` (`src/LevelControls.h`) and is threaded into
`Camera4D::processInput`. Each design doc below lists the **new engine systems** a level
needs beyond what exists today, so we can build them progressively.

Implemented mechanics so far: **axis lock** (forbid translation on chosen axes) and
**gentle head-return** (spring yaw/pitch back to a default). Only the **FPS** control
mode exists; **Orbit** and **Flight** are stubs.
