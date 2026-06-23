# elu4

**A game that teaches you to see, move, and think in four dimensions.**

elu4 is a small OpenGL game built around a single idea: 4D space isn't a
gimmick to be sliced and cross-sectioned, it's a place you can learn to *move
around in*. You start in a forest, a fairy says hello, and over ten short
levels you pick up one new 4D sense at a time — depth along a fourth axis,
turning that fourth direction into view, platforming, flight, and finally
reading a whole 4D landscape.

> Status: early and playable. Ten levels, a real-time 4D renderer with
> hidden-surface removal, and a standalone 4D object visualizer. There is no
> story content yet beyond the level scaffolding — see [`docs/Vision.md`](docs/Vision.md)
> for where it's headed.

## Screenshots

> _Coming soon._ Drop captures in here once the game has a screenshot/headless
> capture mode (see [CONTRIBUTING](CONTRIBUTING.md) — it's a great first
> contribution).

## Why 4D, and how you experience it

The camera looks down the **−W** axis, so the four axes read as:

| Axis    | Feels like                                            |
|---------|-------------------------------------------------------|
| **X, Z**| the horizontal strafe plane — the "four directions"   |
| **W**   | forward / depth — moving along W reads as *zooming*   |
| **Y**   | up (gravity)                                          |

Each level introduces exactly one perception or movement in isolation and
builds on the last. We deliberately avoid "4D-is-weird-from-3D" tricks
(through-the-wall, slicing, inside-out reveals) in favour of movement that
feels natural.

## The levels

| #  | Level             | Camera | Teaches                                  |
|----|-------------------|--------|------------------------------------------|
| 1  | Corridor          | FP     | W-translation = zoom                     |
| 2  | Dodgeball         | FP     | ±X / ±Z strafe (the four directions)     |
| 3  | Turn & Face       | FP     | retained yaw rotates depth into strafe   |
| 4  | Third Person      | Orbit  | controlling a 4D avatar                  |
| 5  | Leap              | Orbit  | Y-axis platforming / jump                |
| 6  | Plane (3D-like)   | Flight | up/down + two horizontal rotation DOF    |
| 7  | Plane (4D course) | Flight | hoops that curve through 4D              |
| 8  | Forest Fetch      | FP     | mapping space, an NPC fetch quest        |
| 9  | Maze              | FP     | spatial navigation + a trajectory minimap|
| 10 | Big Vista         | FP     | reading distance over a 4D heightfield   |

Per-level design notes live in [`docs/levels/`](docs/levels/).

## Install (play without building)

Prebuilt, ready-to-run packages are on the
[**Releases**](https://github.com/sSolal/elu4/releases) page — no compiler or
dependencies to install.

- **Linux** — download `elu4-x86_64.AppImage`, then:
  ```bash
  chmod +x elu4-x86_64.AppImage
  ./elu4-x86_64.AppImage
  ```
  (A working GPU driver with OpenGL is the only requirement; GLEW/GLFW are
  bundled.)
- **Windows** — download `elu4-windows-x64.zip`, extract it, and run
  `game.exe`.

The package contains the ten tutorial levels; the binary finds its assets
relative to itself, so it runs from anywhere (including a double-click).

## Building

elu4 is C++17 + OpenGL, built with CMake. It targets Linux (X11/Wayland) and
should port to other desktops with the same dependencies.

**Dependencies** (Debian/Ubuntu names):

```bash
sudo apt install build-essential cmake libglew-dev libglfw3-dev libglm-dev
```

**Clone (with the ImGui submodule) and build:**

```bash
git clone --recursive git@github.com:sSolal/elu4.git
cd elu4
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/game
```

Already cloned without `--recursive`? Fetch the submodule:

```bash
git submodule update --init --recursive
```

Or just use the convenience script, which builds and runs:

```bash
./run.sh
```

### Optional: VR (OpenXR)

A PCVR path (Route A, via Monado/WiVRn) is available behind a compile flag and
is off by default so the desktop build needs no OpenXR/X11 packages:

```bash
sudo apt install libopenxr-dev
cmake -S . -B build -DBUILD_VR=ON
cmake --build build -j
```

## Controls

Movement and look are spread across the keyboard so all four spatial axes and
the 4D turning planes are reachable at once. The in-game menu and per-level
prompts show the exact bindings; the core scheme:

| Keys        | Action                                                       |
|-------------|--------------------------------------------------------------|
| `E` / `A`   | move forward / back                                          |
| `Q` / `D`   | strafe left / right (X–Z plane)                              |
| `W` / `S`   | move *through the fourth dimension* (advance / retreat in W) |
| `U` `O` `J` `L` | turn — yaw across the X / Z / W planes                   |
| `I` / `K`   | look up / down                                               |
| `T`         | toggle depth-perception aids (vignette + face-shadow)        |

Some levels lock certain axes or spring your head back to centre so you can
focus on the one skill they teach.

## The 4D visualizer

A separate tool renders and rotates standalone 4D objects (hypercube,
hypersphere, …) from simple JSON files:

```bash
./build/visualizer objects/hypercube.json
```

See [`VISUALIZER.md`](VISUALIZER.md) and
[`VISUALIZER_QUICK_START.md`](VISUALIZER_QUICK_START.md) for the object format
and controls.

## Project layout

```
src/            engine + game (4D math, renderer, camera, levels)
  Math4D.h        Rotor4D geometric-algebra core
  levels/         the ten levels (subclass Level, registered in LevelRegistry)
  imgui/          Dear ImGui (git submodule)
shaders/        GLSL for the 4D renderer, visualizer, wireframe, UI
objects/        sample 4D objects (JSON) for the visualizer
scenarios/      headless scene configs
tests/          doctest unit tests for the 4D math core
docs/           Vision.md + per-level design docs
```

Other build targets: `visualizer`, `generate_objects`, `4dg` (object CLI),
`4dscene` (headless scene query), and `tests`.

## Tests

```bash
ctest --test-dir build      # or: ./build/tests
```

## Releasing

Pushing a version tag builds the Linux AppImage and Windows zip and publishes
them to a GitHub Release automatically (see
[`.github/workflows/release.yml`](.github/workflows/release.yml)):

```bash
git tag v0.1.0
git push origin v0.1.0
```

## Contributing

Contributions are very welcome — new levels, engine systems, docs, and bug
fixes alike. See [CONTRIBUTING.md](CONTRIBUTING.md) for the dev setup, how the
level framework works, and a list of good first issues. By participating you
agree to our [Code of Conduct](CODE_OF_CONDUCT.md).

## License

[MIT](LICENSE) © 2026 Solal Stern.

Bundled third-party code keeps its own licenses: [Dear ImGui](https://github.com/ocornut/imgui)
(MIT, vendored as a submodule) and [doctest](https://github.com/doctest/doctest)
(MIT, `src/third_party/doctest.h`).
