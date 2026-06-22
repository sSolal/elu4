-- Game scene 1 — "Bedroom". A 4D room with a ground, first-person camera, a 4D bed
-- on one side, a closet on the other, a window on one wall, a desk + stool against
-- another wall, and a corridor leading out of the +X wall. Walk far enough down the
-- corridor and the engine switches to the "hall" scene.
--
-- Coordinate convention (see Camera4D): X/Z are the horizontal strafe plane, W is
-- forward/depth, Y is up (gravity). At the default orientation the player faces +X,
-- so the corridor opens in the +X wall straight ahead.

local I = Rotor4D.identity()

-- Every placed box is one (mesh, instance_set) pair; render() walks the list. A box
-- is optionally registered as a solid collider so the player can't pass through it.
local parts = {}
local function box(cx, cy, cz, cw, hx, hy, hz, hw, colA, colB, solid)
  local mesh = engine.make_box(vec4(hx, hy, hz, hw))
  local set  = engine.instance_set()
  set:add(vec4(cx, cy, cz, cw), I, colA, colB or colA)
  parts[#parts + 1] = { mesh = mesh, set = set }
  if solid then
    engine.world:add_box(vec4(cx, cy, cz, cw), vec4(hx, hy, hz, hw))
  end
end

-- Palette (warm room, cool accents).
local FLOOR_A = vec3(0.55, 0.45, 0.35)
local FLOOR_B = vec3(0.42, 0.34, 0.26)
local WALL_A  = vec3(0.34, 0.37, 0.45)
local WALL_B  = vec3(0.24, 0.27, 0.34)
local BED_A   = vec3(0.78, 0.32, 0.32)
local BED_B   = vec3(0.55, 0.22, 0.22)
local WOOD_A  = vec3(0.50, 0.34, 0.20)
local WOOD_B  = vec3(0.36, 0.24, 0.14)
local CLOSET_A = vec3(0.38, 0.48, 0.58)
local CLOSET_B = vec3(0.26, 0.34, 0.42)
local GLASS    = vec3(0.58, 0.78, 0.92)
local STOOL    = vec3(0.42, 0.42, 0.50)

local ROOM   = 5.0    -- interior half-extent in X/Z/W
local WALL_T = 0.2    -- wall thickness (half)
local WALL_H = 1.5    -- wall half-height (room is 3 units tall)
local DOOR_H = 1.2    -- doorway half-size in Z and W (the corridor mouth)
local CORR_X = 13.0   -- corridor extends from x=ROOM out to here along +X
local TRIGGER_X = 11.0  -- walk past this x in the corridor -> next scene

function load()
  engine.use_standard_input(true)
  engine.body.radius = 0.8
  engine.body.gravityScale = 1.0
  engine.cam4d.pos = vec4(0, 0.8, 0, 0)   -- room centre, eye ~0.8 above the floor
  engine.cam4d.yaw = Rotor4D.identity()   -- face +X (toward the corridor)
  engine.cam4d.pitch = 0.0

  -- Ground: one big non-occluding slab + a flat collider (top at y=0).
  do
    local mesh = engine.make_ground(vec4(16, 0.1, 16, 16))
    local set  = engine.instance_set()
    set:add(vec4(2, -0.1, 0, 0), I, FLOOR_A, FLOOR_B)
    parts[#parts + 1] = { mesh = mesh, set = set }
    engine.world:add_flat_ground(0.0, 60.0)
  end

  -- --- Room shell (walls are solid; Y spans 0..3) -----------------------------
  -- Back wall (-X), behind the player.
  box(-ROOM, WALL_H, 0, 0, WALL_T, WALL_H, ROOM, ROOM, WALL_A, WALL_B, true)
  -- Bed wall (-Z) and closet wall (+Z).
  box(0, WALL_H, -ROOM, 0, ROOM, WALL_H, WALL_T, ROOM, WALL_A, WALL_B, true)
  box(0, WALL_H,  ROOM, 0, ROOM, WALL_H, WALL_T, ROOM, WALL_A, WALL_B, true)
  -- Window wall (+W) and desk wall (-W).
  box(0, WALL_H, 0,  ROOM, ROOM, WALL_H, ROOM, WALL_T, WALL_A, WALL_B, true)
  box(0, WALL_H, 0, -ROOM, ROOM, WALL_H, ROOM, WALL_T, WALL_A, WALL_B, true)

  -- Front wall (+X) with a central doorway (hole of half DOOR_H in Z and W, full
  -- height 0..2.4). Built as a ring of four pieces plus a lintel above the door.
  local edgeHalf = (ROOM - DOOR_H) * 0.5        -- half-extent of each side strip
  local doorTop = 2.4
  -- +Z and -Z side strips (full W, full lower height).
  box(ROOM, doorTop * 0.5,  DOOR_H + edgeHalf, 0, WALL_T, doorTop * 0.5, edgeHalf, ROOM, WALL_A, WALL_B, true)
  box(ROOM, doorTop * 0.5, -(DOOR_H + edgeHalf), 0, WALL_T, doorTop * 0.5, edgeHalf, ROOM, WALL_A, WALL_B, true)
  -- +W and -W side strips (only across the door's Z span, full lower height).
  box(ROOM, doorTop * 0.5, 0,  DOOR_H + edgeHalf, WALL_T, doorTop * 0.5, DOOR_H, edgeHalf, WALL_A, WALL_B, true)
  box(ROOM, doorTop * 0.5, 0, -(DOOR_H + edgeHalf), WALL_T, doorTop * 0.5, DOOR_H, edgeHalf, WALL_A, WALL_B, true)
  -- Lintel above the doorway (fills y from doorTop up to the wall top).
  box(ROOM, (doorTop + WALL_H * 2) * 0.5, 0, 0,
      WALL_T, (WALL_H * 2 - doorTop) * 0.5, DOOR_H, DOOR_H, WALL_A, WALL_B, true)

  -- --- Corridor: a tube along +X from x=ROOM out to x=CORR_X ------------------
  do
    local cMid  = (ROOM + CORR_X) * 0.5
    local cHalf = (CORR_X - ROOM) * 0.5
    local CW    = 1.4    -- corridor half-width in Z and W
    -- Side walls (±Z, ±W) and a ceiling; the main flat ground is the floor.
    box(cMid, WALL_H, CW, 0,  cHalf, WALL_H, WALL_T, CW, WALL_A, WALL_B, true)
    box(cMid, WALL_H, -CW, 0, cHalf, WALL_H, WALL_T, CW, WALL_A, WALL_B, true)
    box(cMid, WALL_H, 0, CW,  cHalf, WALL_H, CW, WALL_T, WALL_A, WALL_B, true)
    box(cMid, WALL_H, 0, -CW, cHalf, WALL_H, CW, WALL_T, WALL_A, WALL_B, true)
    box(cMid, 2.9, 0, 0, cHalf, 0.2, CW, CW, WALL_A, WALL_B, true)
  end

  -- --- Bed (against the -Z wall): mattress + headboard + 6 legs ---------------
  box(0, 0.5, -3.4, 0, 1.6, 0.25, 0.9, 1.6, BED_A, BED_B)              -- mattress (4D: extends in W)
  box(0, 0.95, -4.3, 0, 1.6, 0.55, 0.12, 1.6, WOOD_A, WOOD_B)         -- headboard at the wall
  for _, lx in ipairs({ -1.4, 1.4 }) do
    for _, lw in ipairs({ -1.4, 0, 1.4 }) do
      box(lx, 0.125, -3.4, lw, 0.12, 0.125, 0.12, 0.12, WOOD_A, WOOD_B)  -- 6 legs down -Y
    end
  end

  -- --- Closet (against the +Z wall): body + door face ------------------------
  box(2.6, 1.4, 4.2, 0, 1.0, 1.4, 0.55, 1.0, CLOSET_A, CLOSET_B, true)
  box(2.6, 1.4, 3.6, 0, 0.9, 1.2, 0.05, 0.9, GLASS, CLOSET_A)         -- door face (lighter)

  -- --- Window (on the +W wall): frame + pane --------------------------------
  box(-2.2, 1.7, 0, 4.9, 0.95, 0.65, 0.95, 0.06, WOOD_B, WOOD_A)      -- frame (darker, slightly proud)
  box(-2.2, 1.7, 0, 4.95, 0.78, 0.5, 0.78, 0.03, GLASS, vec3(0.80, 0.92, 1.0))  -- pane

  -- --- Desk + stool (against the -W wall) -----------------------------------
  box(0, 1.0, 0, -4.0, 1.5, 0.06, 0.9, 0.45, WOOD_A, WOOD_B)          -- desk top
  for _, lx in ipairs({ -1.4, 1.4 }) do
    for _, lw in ipairs({ -4.4, -4.0, -3.6 }) do
      box(lx, 0.47, 0, lw, 0.07, 0.47, 0.07, 0.07, WOOD_B, WOOD_A)    -- 6 desk legs down -Y
    end
  end
  -- Stool: a square thin-in-Y seat with 4 legs spread tetrahedrally in X/Z/W.
  box(0, 0.55, 0, -3.0, 0.4, 0.05, 0.4, 0.4, STOOL, WALL_B)
  local legs = { { 0.3, 0.3, 0.3 }, { -0.3, -0.3, 0.3 },
                 { -0.3, 0.3, -0.3 }, { 0.3, -0.3, -0.3 } }
  for _, p in ipairs(legs) do
    box(p[1], 0.25, p[2], -3.0 + p[3], 0.06, 0.25, 0.06, 0.06, STOOL, WALL_B)
  end
end

function update()
  -- Standard FPS input runs automatically. Walk far enough down the +X corridor
  -- and hand off to the hall scene.
  if engine.cam4d.pos.x > TRIGGER_X then
    engine.goto_scene("hall")
  end
end

function render()
  for _, p in ipairs(parts) do
    engine.draw_instances(p.mesh, p.set)
  end
  engine.draw_outer_cube()
end

function render_hud()
  local lines = {}
  if not engine.inside_mode() then
    lines[#lines + 1] = "Bedroom. Press TAB to step inside the 4D view."
  else
    lines[#lines + 1] = "Bedroom. E/A move, Q/D strafe, W/S move through the 4th dimension."
    lines[#lines + 1] = "Walk forward (E) down the corridor to leave."
  end
  engine.hud_window("Bedroom", lines)
end
