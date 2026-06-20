# Contributing to elu4

Thanks for your interest! elu4 is a small project with room to grow — new
levels, engine systems, rendering work, docs, and bug fixes are all welcome.

By participating you agree to abide by our [Code of Conduct](CODE_OF_CONDUCT.md).

## Getting set up

```bash
git clone --recursive git@github.com:sSolal/elu4.git
cd elu4
sudo apt install build-essential cmake libglew-dev libglfw3-dev libglm-dev libx11-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/game
```

If you forgot `--recursive`, run `git submodule update --init --recursive` to
fetch Dear ImGui.

Run the tests before and after your change:

```bash
ctest --test-dir build      # or ./build/tests
```

The math core (`src/Math4D.h`) is covered by headless doctest tests in
`tests/test_math4d.cpp` — please extend them when you touch 4D math.

## How the game is structured

- **4D math** lives in `src/Math4D.h` (a `Rotor4D` geometric-algebra
  orientation). All rotation/orientation should go through it.
- **Rendering** is in `src/Renderer.cpp` with GLSL in `shaders/`. The renderer
  does 4D hidden-surface removal (back-cell culling + per-fragment occlusion)
  on a single tetrahedron primitive.
- **Levels** subclass `Level` (`src/Level.h`) and are registered in
  `src/LevelRegistry.cpp`. Per-level control configuration (axis locks,
  head-return, control mode) is threaded through `Camera4D::processInput`
  (`src/Camera.cpp`).

### Adding a level

1. Add `src/levels/MyLevel.{h,cpp}` subclassing `Level`.
2. Register it in `src/LevelRegistry.cpp`.
3. Add the source to `add_executable(game ...)` in `CMakeLists.txt`.
4. Write a short design note in `docs/levels/` following the existing ones —
   state the single perception/movement it teaches and any new engine systems
   it needs.

Keep new levels in the spirit of the curriculum: introduce **one** idea in
isolation, and prefer natural movement over "4D is weird" gimmicks.

## Coding conventions

- C++17. Match the style of the file you're editing — naming, brace placement,
  and comment density. The codebase favours short, explanatory comments on the
  *why*, not the *what*.
- Keep the desktop build dependency-free beyond GLEW/GLFW/GLM; put anything
  heavier behind a CMake `option()` like the existing `BUILD_VR`.
- No build artifacts in commits — `build/` is gitignored.

## Submitting changes

1. Fork and branch from `main`.
2. Make focused commits with clear messages (imperative mood, e.g. "Add Leap
   level").
3. Ensure the project builds and `ctest` passes.
4. Open a pull request describing what changed and how you verified it. Fill in
   the PR template.

## Good first issues

- **A screenshot / headless capture mode.** Add a `--screenshot out.png` (and
  ideally `--headless` / `--no-ui`) flag that loads a level, settles a few
  frames, then `glReadPixels` → PNG (drop in `stb_image_write.h`). This unblocks
  the README gallery and CI visual checks.
- **Flesh out a level marked thin** in `docs/levels/` into real mechanics.
- **Port the build** to macOS/Windows and document it.
- **Expand the 4D math tests** in `tests/test_math4d.cpp`.

Not sure where to start? Open an issue and ask — happy to point you somewhere.
