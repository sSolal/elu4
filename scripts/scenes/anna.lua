-- Game scene 3 — "Anna's". A small placeholder room reached through the orange
-- corridor of the neighbourhood. For now it just holds a single arch: step through
-- it and the engine returns you to the neighbourhood, dropping you back in the
-- orange corridor facing the room so you walk straight back out into it.
--
-- Coordinate convention (see Camera4D): X/Z horizontal strafe plane, W forward/depth,
-- Y up (gravity). The player arrives facing +X with the arch straight ahead.

local I  = Rotor4D.identity()
local PI = math.pi

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

local GROUND_A = vec3(0.34, 0.30, 0.38)
local GROUND_B = vec3(0.23, 0.20, 0.27)
local WALL_A   = vec3(0.40, 0.34, 0.46)
local WALL_B   = vec3(0.28, 0.23, 0.33)
local ARCH     = vec3(0.95, 0.55, 0.15)   -- orange, echoing the corridor it returns to
local ARCH_B   = vec3(0.66, 0.37, 0.10)
local THRESH   = vec3(0.98, 0.74, 0.30)

local ROOM   = 5.0    -- room half-extent in X/Z/W
local WALL_H = 1.5    -- room is 3 units tall
local ARCH_X = 4.0    -- the arch sits this far ahead along +X
local ARCH_W = 1.5    -- arch opening half-width (in Z and W)

function load()
  engine.use_standard_input(true)
  engine.set_focal(2.1)
  engine.body.radius = 0.8
  engine.body.gravityScale = 1.0
  engine.cam4d.pos   = vec4(-2.0, 0.8, 0, 0)
  engine.cam4d.yaw   = Rotor4D.from_xw(PI * 0.5)   -- nose -> +X (toward the arch)
  engine.cam4d.pitch = 0.0

  -- Ground slab + flat collider.
  do
    local mesh = engine.make_ground(vec4(ROOM + 1, 0.1, ROOM + 1, ROOM + 1))
    local set  = engine.instance_set()
    set:add(vec4(0, -0.1, 0, 0), I, GROUND_A, GROUND_B)
    parts[#parts + 1] = { mesh = mesh, set = set }
    engine.world:add_flat_ground(0.0, 40.0)
  end

  -- Four enclosing walls (Y spans 0..3).
  box( ROOM, WALL_H, 0, 0, 0.2, WALL_H, ROOM, ROOM, WALL_A, WALL_B, true)  -- +X
  box(-ROOM, WALL_H, 0, 0, 0.2, WALL_H, ROOM, ROOM, WALL_A, WALL_B, true)  -- -X
  box(0, WALL_H, 0,  ROOM, ROOM, WALL_H, ROOM, 0.2, WALL_A, WALL_B, true)  -- +W
  box(0, WALL_H, 0, -ROOM, ROOM, WALL_H, ROOM, 0.2, WALL_A, WALL_B, true)  -- -W

  -- The arch: two posts (solid) + a lintel + a bright threshold pad on the ground.
  box(ARCH_X, 1.6, 0,  ARCH_W, 0.3, 1.6, 0.3, 0.3, ARCH, ARCH_B, true)
  box(ARCH_X, 1.6, 0, -ARCH_W, 0.3, 1.6, 0.3, 0.3, ARCH, ARCH_B, true)
  box(ARCH_X, 3.4, 0, 0, 0.3, 0.3, 0.3, ARCH_W + 0.3, ARCH, ARCH_B, true)
  box(ARCH_X, 0.02, 0, 0, 0.4, 0.02, ARCH_W, ARCH_W, THRESH, ARCH)
end

function update()
  -- Step into the arch opening -> return to the neighbourhood's orange corridor.
  local p = engine.cam4d.pos
  if math.abs(p.x - ARCH_X) < 0.7 and math.abs(p.z) < ARCH_W and math.abs(p.w) < ARCH_W then
    engine.save_set("entry", "from_anna")
    engine.goto_scene("neighbourhood")
  end
end

function render()
  for _, part in ipairs(parts) do
    engine.draw_instances(part.mesh, part.set)
  end
  engine.draw_outer_cube()
end

function render_hud()
  if not engine.inside_mode() then
    engine.hud_window("Anna's", { "Press TAB to step inside the 4D view." })
    return
  end
  engine.hud_window("Anna's",
    { "Nobody's home yet. Walk through the arch to head back to the neighbourhood." })
end
