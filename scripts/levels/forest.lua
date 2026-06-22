-- Level 8 (Lua port) — "Forest Fetch". The forest FLOOR is the X-Z-W 3-volume.
-- Navigate by named landmark trees. Two-leg quest: fetch the golden cube (past the
-- Tall Red Pine, +W), carry it back to the NPC, then reach the clearing (past the
-- Crooked Blue Tree, -X). Full first-person controls; Space drives the quest.

local SURFACE_Y, EYE_HEIGHT, TILE, TILE_Y, NT, STEP = 0.0, 1.6, 6.0, 0.25, 2, 12.0
local TREE_SCALE, LM_SCALE, TREE_N, CLEARING_R = 2.5, 4.5, 40, 7.0
local INTERACT_DIST, ARRIVE_DIST, HOLD_DIST = 4.5, 5.0, 1.6

local GROUND_A, GROUND_B = vec3(0.30, 0.40, 0.30), vec3(0.22, 0.33, 0.24)
local TREE_LO, TREE_HI = vec3(0.10, 0.30, 0.14), vec3(0.22, 0.50, 0.26)
local NPC_COL, GOLD_COL = vec3(0.30, 0.90, 0.95), vec3(1.00, 0.82, 0.10)

local I = Rotor4D.identity()
local treeMesh, landmarkMesh, groundMesh, npcMesh, goldMesh
local mergedSet, landmarkSet, npcSet, goldSet
local landmarks = {}
local npcPos, goldPos, finalPos
local quest, held, won, spaceWasDown = "ask", false, false, false
local nearNpc, nearGold, atFinal = false, false, false

local function hdist(a, b)
  local dx, dz, dw = a.x - b.x, a.z - b.z, a.w - b.w
  return math.sqrt(dx * dx + dz * dz + dw * dw)
end

function load()
  engine.use_standard_input(true)
  engine.cam4d.pos = vec4(0, EYE_HEIGHT, 0, 0)
  engine.cam4d.speed = 4.5
  engine.body.radius = EYE_HEIGHT
  engine.controls.lockedAxes = engine.AxisLock.NONE
  engine.controls.headReturn = false
  engine.set_scene_far(120.0, 0.30)

  local rawTree = engine.load_mesh("assets/tree.json")
  local treeBase = engine.make_scaled(rawTree, TREE_SCALE)
  landmarkMesh = engine.make_scaled(rawTree, LM_SCALE)
  npcMesh  = engine.asset_mesh(engine.load_asset("assets/npc.json"))
  goldMesh = engine.asset_mesh(engine.load_asset("assets/gold_cube.json"))
  mergedSet = engine.instance_set(); mergedSet:add(vec4(0,0,0,0), I, vec3(1,1,1), vec3(1,1,1))
  landmarkSet, npcSet, goldSet = engine.instance_set(), engine.instance_set(), engine.instance_set()

  npcPos   = vec4(0, 1.2, 0, 0)
  goldPos  = vec4(2, 1.4, -3, 24)
  finalPos = vec4(-24, SURFACE_Y, 6, -3)

  landmarks = {
    { pos = vec4(0,   LM_SCALE,  0,  16), color = vec3(0.85, 0.18, 0.15) },  -- Tall Red Pine
    { pos = vec4(-16, LM_SCALE,  5,   0), color = vec3(0.20, 0.40, 0.95) },  -- Crooked Blue Tree
    { pos = vec4(13,  LM_SCALE, -13, -9), color = vec3(0.90, 0.65, 0.12) },  -- Amber Willow
  }
  for _, lm in ipairs(landmarks) do
    landmarkSet:add(lm.pos, I, lm.color, lm.color * 1.3)
  end

  -- Ground tiles baked into one mesh + one flat collider.
  local groundTile = engine.make_ground(vec4(TILE, TILE_Y, TILE, TILE))
  local visualY = SURFACE_Y - TILE_Y
  local tiles = engine.instance_set()
  for ix = -NT, NT do for iz = -NT, NT do for iw = -NT, NT do
    local parity = (ix + iz + iw) & 1
    local shade = (parity == 1) and GROUND_A or GROUND_B
    tiles:add(vec4(ix * STEP, visualY, iz * STEP, iw * STEP), I, shade, shade)
  end end end
  groundMesh = engine.make_merged(groundTile, tiles)
  engine.world:add_flat_ground(SURFACE_Y, NT * STEP + TILE)

  -- Scatter ordinary trees, kept out of the clearings, baked into one mesh.
  local REGION = NT * STEP + TILE - 2.0
  local seed = 1337
  local function rnd()
    seed = (seed * 1664525 + 1013904223) & 0xFFFFFFFF
    return ((seed >> 8) & 0xFFFFFF) / 0x1000000
  end
  local function blocked(p)
    if hdist(p, npcPos) < CLEARING_R or hdist(p, goldPos) < CLEARING_R
       or hdist(p, finalPos) < CLEARING_R then return true end
    for _, lm in ipairs(landmarks) do if hdist(p, lm.pos) < CLEARING_R then return true end end
    return false
  end
  local trees = engine.instance_set()
  local placed, guard = 0, 0
  while placed < TREE_N and guard < TREE_N * 20 do
    guard = guard + 1
    local p = vec4(-REGION + 2 * REGION * rnd(), TREE_SCALE,
                   -REGION + 2 * REGION * rnd(), -REGION + 2 * REGION * rnd())
    if not blocked(p) then trees:add(p, I, TREE_LO, TREE_HI); placed = placed + 1 end
  end
  treeMesh = engine.make_merged(treeBase, trees)
end

function update()
  -- runCameraInput runs automatically.
  local p = engine.cam4d.pos
  nearNpc  = engine.inside_mode() and hdist(p, npcPos) < INTERACT_DIST
  nearGold = engine.inside_mode() and not held and quest == "fetching" and hdist(p, goldPos) < INTERACT_DIST
  atFinal  = engine.inside_mode() and quest == "directed" and hdist(p, finalPos) < ARRIVE_DIST

  local space = engine.key_down(engine.keys.SPACE)
  local pressed = space and not spaceWasDown
  spaceWasDown = space

  if engine.inside_mode() and pressed then
    if quest == "ask" and nearNpc then quest = "fetching"
    elseif quest == "fetching" and nearGold then held = true; quest = "returning"
    elseif quest == "returning" and nearNpc then held = false; quest = "directed" end
  end
  if quest == "directed" and atFinal then quest = "done"; won = true; engine.set_won() end
end

function render()
  engine.draw_instances(groundMesh, mergedSet)
  engine.draw_instances(treeMesh, mergedSet)
  engine.draw_instances(landmarkMesh, landmarkSet)

  npcSet:clear(); npcSet:add(npcPos, I, NPC_COL, NPC_COL * 0.7)
  engine.draw_instances(npcMesh, npcSet)

  local goldAt
  if held then
    local fw = engine.cam4d:get_orientation():forward()
    goldAt = engine.cam4d.pos + fw * HOLD_DIST + vec4(0, -0.4, 0, 0)
  elseif quest == "ask" or quest == "fetching" then
    goldAt = goldPos
  else
    goldAt = npcPos + vec4(1.2, 0.2, 0, 0)
  end
  goldSet:clear(); goldSet:add(goldAt, I, GOLD_COL, GOLD_COL)
  engine.draw_instances(goldMesh, goldSet)

  engine.draw_outer_cube()
end

function render_hud()
  local lines = { "Forest Fetch - the ground is the X-Z-W volume; navigate by the landmark trees." }
  if not engine.inside_mode() then
    lines[#lines + 1] = "Press TAB to step inside the 4D view."
  else
    lines[#lines + 1] = "Move: E/A, Q/D, W/S. Turn: J/O and U/L. Look: I/K. Use the gauges (bottom-right)."
    if quest == "ask" then
      lines[#lines + 1] = "NPC: \"Fetch the golden cube. It lies deep toward +W, past the Tall Red Pine.\""
    elseif quest == "fetching" then
      lines[#lines + 1] = "Objective: find the golden cube (+W, past the Tall Red Pine) and pick it up."
    elseif quest == "returning" then
      lines[#lines + 1] = "Objective: carry the cube back to the NPC at the hub (-W)."
    elseif quest == "directed" then
      lines[#lines + 1] = "NPC: \"Well done. Now seek the clearing toward -X, past the Crooked Blue Tree.\""
    end
  end
  if not won and engine.inside_mode() then
    if quest == "ask" and nearNpc then lines[#lines + 1] = "Press SPACE to accept the task."
    elseif quest == "fetching" and nearGold then lines[#lines + 1] = "Press SPACE to pick up the golden cube."
    elseif quest == "returning" and nearNpc then lines[#lines + 1] = "Press SPACE to deliver the cube." end
  end
  if won then lines[#lines + 1] = "You reached the clearing - level complete!" end
  engine.hud_window("Forest Fetch", lines)
  engine.hud_facing(engine.cam4d:get_orientation():forward())
  engine.hud_coord(engine.cam4d.pos, "You")
end
