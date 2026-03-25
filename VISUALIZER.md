# 4D Object Visualizer

A tool for visualizing and interacting with 4-dimensional objects using 4D→3D→2D perspective projection.

## Overview

The visualizer system consists of:

1. **Object4D Format** - JSON-based format for storing 4D geometric objects
2. **Visualizer** - Interactive program for rotating and viewing 4D objects
3. **Object Generator** - Tool for generating test objects

## 4D Object Format

Objects are stored as JSON files with the following structure:

```json
{
  "name": "Object Name",
  "vertices": [
    [x, y, z, w],
    ...
  ],
  "edges": [
    [v0, v1],
    ...
  ],
  "cells": [
    [v0, v1, v2, v3, ...],
    ...
  ]
}
```

**Components:**
- `vertices`: Array of 4D points [x, y, z, w]
- `edges`: Pairs of vertex indices that form 1D edges
- `cells`: Polygons (lists of vertex indices) that form 2D faces in 4D space

## Building

From the build directory:

```bash
cmake --build . --target visualizer
cmake --build . --target generate_objects
```

## Running the Visualizer

### Load an object from file:
```bash
./visualizer ../hypercube.json
./visualizer ../hypersphere.json
```

### Keyboard Controls

**6 Planes of Rotation** (each plane can rotate independently):

| Plane | Positive Rotation | Negative Rotation |
|-------|-------------------|-------------------|
| XY    | Q                 | W                 |
| XZ    | A                 | S                 |
| XW    | Z                 | X                 |
| YZ    | E                 | R                 |
| YW    | D                 | F                 |
| ZW    | C                 | V                 |

**Other Controls:**
- `UP/DOWN` - Adjust focal length (4D perspective)
- `1` - Toggle edge display
- `2` - Toggle cell (face) display
- `ESC` - Exit

## Focal Length

The focal length controls the perspective projection from 4D to 3D. Higher values show less perspective distortion, lower values create more extreme perspective effects.

## Test Objects

### Hypercube (hypercube.json)
- 16 vertices (all ±0.5 combinations)
- 32 edges (1D structure)
- 24 square faces (2D cells in 4D)
- Represents a 4-dimensional unit cube

### Hypersphere (hypersphere.json)
- 64 vertices (4×4×4 low-resolution grid on 3-sphere)
- 144 edges (connecting adjacent grid points)
- 81 quad faces (from grid structure)
- Approximate 4D sphere

## Generating Custom Objects

The `generate_objects` program creates the test objects. To extend it:

1. Create a function that populates `Object4D`
2. Call `saveToJSON()` to write to file

Example:
```cpp
Object4D myObject;
myObject.name = "My Object";
myObject.vertices.push_back(glm::vec4(0, 0, 0, 0));
myObject.vertices.push_back(glm::vec4(1, 0, 0, 0));
myObject.edges.push_back({0, 1});
myObject.saveToJSON("my_object.json");
```

## Implementation Details

### 4D to 3D Projection

Uses perspective projection along the W axis:
```
f = 1.0 / (focal_length - w)
projected_3d = (f*x, f*y, f*z)
```

This creates the characteristic "hyperbolic" appearance when rotating in 4D planes.

### Rendering

- Vertices are stored as 4D points
- At each frame:
  1. Apply 6 rotations (composition of all rotation angles)
  2. Project 4D vertices to 3D using focal length
  3. Render as wireframe (edges) or solid (cells)

### File Structure

```
src/
├── Object4D.h/cpp       - 4D object definition and JSON I/O
├── visualizer_main.cpp  - Main visualizer program
├── generate_objects.cpp - Object generation utility
└── (reused files)
    ├── Shader.h/cpp     - OpenGL shader management
    ├── Camera.h/cpp     - Camera system
    └── Math4D.h         - 4D math utilities

hypercube.json         - Test hypercube object
hypersphere.json       - Test hypersphere object
```

## Notes

- Objects are rendered using OpenGL with simple wireframe/solid modes
- The projection handles vertices behind the camera (w > -focal_length) by clamping
- Rotating in different planes creates the classic "hypercube" unfolding effect
- For best visualization, try slowly rotating in one or two planes at a time
