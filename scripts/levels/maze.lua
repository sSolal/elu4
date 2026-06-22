-- Level 9 (Lua port) — "Maze".
-- A faithful port of src/levels/MazeLevel.cpp: a fixed-seed procedurally generated
-- block maze across the X-Z-W floor 3-volume (Y is height only). Geometry, colliders
-- and the per-frame visibility cull (PVS) are all built in script; the minimap is the
-- C++ component, enabled here with { trace = true }. Uses the standard input path.

-- --- Tunables (mirror MazeLevel.h) -----------------------------------------
local N      = 4
local G      = 2 * N + 1     -- 9
local CELL   = 4.0
local RADIUS = 1.0
local ARRIVE_DIST = 3.0
local VIS_DEPTH   = 2

-- Colours
local FLOOR_A   = vec3(0.20, 0.22, 0.27)
local FLOOR_B   = vec3(0.15, 0.17, 0.21)
local WALL_LOW  = vec3(0.28, 0.32, 0.46)
local WALL_HIGH = vec3(0.55, 0.48, 0.40)

-- The 6 axis-aligned grid steps (±X, ±Z, ±W in grid space).
local NI = {1, -1, 0, 0, 0, 0}
local NJ = {0, 0, 1, -1, 0, 0}
local NK = {0, 0, 0, 0, 1, -1}

-- --- State -----------------------------------------------------------------
local solid = {}            -- [gidx] = 1 solid / 0 open
local wallData  = {}        -- [gidx] = {pos=, ca=, cb=}  (only bordering walls)
local floorData = {}        -- [gidx] = {pos=, ca=, cb=}  (open cells)
local goalCell, goalPos
local boundsMin, boundsMax
local wallMesh, floorMesh
local visWalls, visFloors
local lastPlayerCell = -1
local goalVisible = false
local won = false

local I = Rotor4D.identity()
local C = (G - 1) * 0.5

local function gidx(i, j, k) return (i * G + j) * G + k end
local function in_grid(i, j, k)
  return i >= 0 and i < G and j >= 0 and j < G and k >= 0 and k < G
end
local function clampG(v) return v < 0 and 0 or (v >= G and G - 1 or v) end

local function cellWorld(gi, gj, gk, y)
  return vec4((gi - C) * CELL, y, (gj - C) * CELL, (gk - C) * CELL)
end

local function is_open(i, j, k)
  if not in_grid(i, j, k) then return false end   -- boundary = solid
  return solid[gidx(i, j, k)] == 0
end

-- Horizontal distance over the X-Z-W floor (Y never counts).
local function hdist(a, b)
  local dx, dz, dw = a.x - b.x, a.z - b.z, a.w - b.w
  return math.sqrt(dx * dx + dz * dz + dw * dw)
end

function load()
  engine.use_minimap{ trace = true }
  engine.use_standard_input(true)

  -- Spawn in the entrance room (grid corner 1,1,1), eye at body rest height.
  engine.cam4d.pos   = cellWorld(1, 1, 1, RADIUS)
  engine.cam4d.speed = 3.5
  engine.body.radius = RADIUS
  engine.controls.lockedAxes = engine.AxisLock.NONE
  engine.controls.headReturn = false
  engine.set_scene_far(90.0, 0.40)

  wallMesh  = engine.make_box(vec4(CELL * 0.5, CELL * 0.5, CELL * 0.5, CELL * 0.5))
  floorMesh = engine.make_ground(vec4(CELL * 0.5, 0.15, CELL * 0.5, CELL * 0.5))
  visWalls, visFloors = engine.instance_set(), engine.instance_set()

  -- --- Carve a perfect 3D maze on the room lattice (recursive backtracker). ---
  for c = 0, G * G * G - 1 do solid[c] = 1 end
  for ri = 0, N - 1 do for rj = 0, N - 1 do for rk = 0, N - 1 do
    solid[gidx(2 * ri + 1, 2 * rj + 1, 2 * rk + 1)] = 0
  end end end

  local seed = 1337
  local function rnd()
    seed = (seed * 1664525 + 1013904223) & 0xFFFFFFFF
    return (seed >> 8) & 0xFFFFFF
  end

  local DI = {1, -1, 0, 0, 0, 0}
  local DJ = {0, 0, 1, -1, 0, 0}
  local DK = {0, 0, 0, 0, 1, -1}
  local visited = {}
  local function ridx(i, j, k) return (i * N + j) * N + k end
  visited[ridx(0, 0, 0)] = true
  local stack = {{i = 0, j = 0, k = 0}}
  while #stack > 0 do
    local cur = stack[#stack]
    local cand, nc = {}, 0
    for d = 1, 6 do
      local ni, nj, nk = cur.i + DI[d], cur.j + DJ[d], cur.k + DK[d]
      if ni >= 0 and ni < N and nj >= 0 and nj < N and nk >= 0 and nk < N
         and not visited[ridx(ni, nj, nk)] then
        nc = nc + 1; cand[nc] = d
      end
    end
    if nc == 0 then
      stack[#stack] = nil
    else
      local d = cand[rnd() % nc + 1]
      local ni, nj, nk = cur.i + DI[d], cur.j + DJ[d], cur.k + DK[d]
      -- Open the wall cell midway between the two rooms (grid coords).
      local wi = (2 * cur.i + 1) + DI[d]
      local wj = (2 * cur.j + 1) + DJ[d]
      local wk = (2 * cur.k + 1) + DK[d]
      solid[gidx(wi, wj, wk)] = 0
      visited[ridx(ni, nj, nk)] = true
      stack[#stack + 1] = {i = ni, j = nj, k = nk}
    end
  end

  -- --- Emit geometry + colliders from the grid. ---
  for gi = 0, G - 1 do for gj = 0, G - 1 do for gk = 0, G - 1 do
    local cid = gidx(gi, gj, gk)
    if is_open(gi, gj, gk) then
      local fp = cellWorld(gi, gj, gk, -0.15)
      local parity = (gi + gj + gk) & 1
      local shade = (parity == 1) and FLOOR_A or FLOOR_B
      floorData[cid] = { pos = fp, ca = shade, cb = shade }
      engine.world:add_object(cellWorld(gi, gj, gk, -CELL * 0.5), CELL * 0.5)
    else
      -- Wall block: emit only if it borders a passage.
      local borders = false
      for d = 1, 6 do
        if is_open(gi + NI[d], gj + NJ[d], gk + NK[d]) then borders = true; break end
      end
      if borders then
        local wp = cellWorld(gi, gj, gk, CELL * 0.5)
        local t = gk / (G - 1)
        local base = WALL_LOW * (1.0 - t) + WALL_HIGH * t   -- mix(low, high, t)
        wallData[cid] = { pos = wp, ca = base * 0.85, cb = base * 1.15 }
        engine.world:add_object(wp, CELL * 0.5)
      end
    end
  end end end

  -- --- Exit marker: a bright tesseract in the far-corner room. ---
  goalCell = gidx(G - 2, G - 2, G - 2)
  goalPos  = cellWorld(G - 2, G - 2, G - 2, CELL * 0.5)

  -- --- Minimap frame bounds (map space; W -> vertical). ---
  local B = ((G - 2) - (G - 1) * 0.5) * CELL + CELL * 0.5     -- 14 for defaults
  boundsMin = vec3(-B, -B, -B)
  boundsMax = vec3( B,  B,  B)
end

-- Rebuild visWalls/visFloors/goalVisible from the player's current cell (local open
-- pocket BFS + straight corridor line-of-sight). Cheap no-op when the cell is unchanged.
local bfsVisited = {}
local function rebuildVisible()
  local p = engine.cam4d.pos
  local pi = clampG(math.floor(p.x / CELL + C + 0.5))
  local pj = clampG(math.floor(p.z / CELL + C + 0.5))
  local pk = clampG(math.floor(p.w / CELL + C + 0.5))

  -- If we land on a solid cell, snap to the nearest open neighbour; else fail safe.
  if solid[gidx(pi, pj, pk)] == 1 then
    local found = false
    for d = 1, 6 do
      local ni, nj, nk = pi + NI[d], pj + NJ[d], pk + NK[d]
      if in_grid(ni, nj, nk) and solid[gidx(ni, nj, nk)] == 0 then
        pi, pj, pk = ni, nj, nk; found = true; break
      end
    end
    if not found then
      lastPlayerCell = -1
      visWalls:clear(); visFloors:clear()
      for _, w in pairs(wallData) do visWalls:add(w.pos, I, w.ca, w.cb) end
      for _, f in pairs(floorData) do visFloors:add(f.pos, I, f.ca, f.cb) end
      goalVisible = true
      return
    end
  end

  local startCell = gidx(pi, pj, pk)
  if startCell == lastPlayerCell then return end   -- cache: same cell -> same set
  lastPlayerCell = startCell

  bfsVisited = {}
  visWalls:clear(); visFloors:clear()
  goalVisible = false
  local queue, qn = {}, 0

  local function addWall(cell)
    if bfsVisited[cell] then return end
    bfsVisited[cell] = true
    local w = wallData[cell]
    if w then visWalls:add(w.pos, I, w.ca, w.cb) end
  end
  local function addOpen(cell, ci, cj, ck)
    local f = floorData[cell]
    if f then visFloors:add(f.pos, I, f.ca, f.cb) end
    if cell == goalCell then goalVisible = true end
    for d = 1, 6 do
      local ni, nj, nk = ci + NI[d], cj + NJ[d], ck + NK[d]
      if in_grid(ni, nj, nk) and solid[gidx(ni, nj, nk)] == 1 then
        addWall(gidx(ni, nj, nk))
      end
    end
  end

  -- Local pocket: level-synchronous BFS through open cells out to VIS_DEPTH.
  qn = qn + 1; queue[qn] = startCell
  bfsVisited[startCell] = true
  addOpen(startCell, pi, pj, pk)
  local levelEnd = qn
  local depth = 0
  local head = 1
  while head <= qn do
    if head == levelEnd + 1 then depth = depth + 1; levelEnd = qn end
    local cell = queue[head]
    local ci = cell // (G * G)
    local cj = (cell // G) % G
    local ck = cell % G
    if depth < VIS_DEPTH then
      for d = 1, 6 do
        local ni, nj, nk = ci + NI[d], cj + NJ[d], ck + NK[d]
        if in_grid(ni, nj, nk) then
          local ncell = gidx(ni, nj, nk)
          if solid[ncell] == 0 and not bfsVisited[ncell] then
            bfsVisited[ncell] = true
            addOpen(ncell, ni, nj, nk)
            qn = qn + 1; queue[qn] = ncell
          end
        end
      end
    end
    head = head + 1
  end

  -- Corridor line-of-sight: march straight down each axis through open cells.
  for d = 1, 6 do
    local ci, cj, ck = pi, pj, pk
    while true do
      local ni, nj, nk = ci + NI[d], cj + NJ[d], ck + NK[d]
      if not in_grid(ni, nj, nk) then break end
      local ncell = gidx(ni, nj, nk)
      if solid[ncell] == 1 then addWall(ncell); break end
      if not bfsVisited[ncell] then
        bfsVisited[ncell] = true
        addOpen(ncell, ni, nj, nk)
      end
      ci, cj, ck = ni, nj, nk
    end
  end
end

function update()
  -- runCameraInput runs automatically (use_standard_input).
  if engine.inside_mode() then
    local p = engine.cam4d.pos
    engine.minimap_trace(vec3(p.x, p.w, p.z))     -- map space: W -> up
    if not won and hdist(p, goalPos) < ARRIVE_DIST then
      won = true; engine.set_won()
    end
  end
end

function render()
  rebuildVisible()

  if visFloors:size() > 0 then
    engine.draw_instances(floorMesh, visFloors, wallMesh, visWalls)  -- occluded by walls
  end
  if visWalls:size() > 0 then
    engine.draw_instances(wallMesh, visWalls)
  end
  if goalVisible then
    local goalSet = engine.instance_set()
    goalSet:add(goalPos, Rotor4D.from_xw(0.4), vec3(1.0, 0.2, 0.15), vec3(1.0, 0.6, 0.2))
    engine.draw_instances(engine.hyper_mesh, goalSet, wallMesh, visWalls)
  end

  engine.draw_outer_cube()

  -- Minimap (bottom +X,+Z corner of the outer box; outer box is +/-0.5).
  local p = engine.cam4d.pos
  local player = vec3(p.x, p.w, p.z)
  local fw = engine.cam4d:get_orientation():forward()
  local facing = vec3(fw.x, fw.w, fw.z)
  local ah = 0.14
  local anchor = vec3(0.5 - ah, -0.5 + ah, 0.5 - ah)
  engine.minimap_draw(player, facing, anchor, ah, boundsMin, boundsMax)
end

function render_hud()
  local lines = {
    "Maze - find the bright red cube at the far corner. The maze branches in X, Z and W; there is no up.",
  }
  if not engine.inside_mode() then
    lines[#lines + 1] = "Press TAB to step inside the 4D view."
  else
    lines[#lines + 1] = "Move: E/A (forward/back), Q/D (strafe), W/S (along W). Turn: J/O and U/L. Look: I/K."
    lines[#lines + 1] = "The small cube in the bottom corner is your map: white = your path, height = depth in W, cyan arrow = heading."
  end
  if won then lines[#lines + 1] = "You reached the exit - level complete!" end
  engine.hud_window("Maze", lines)
end
