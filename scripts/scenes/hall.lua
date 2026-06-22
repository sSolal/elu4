-- Game scene 2 — "Hall". A bare ground with a gate. Walk into the gate and the
-- engine switches back to the "bedroom" scene. (Placeholder second room; will grow.)
--
-- Coordinate convention (see Camera4D): X/Z horizontal strafe plane, W forward/depth,
-- Y up (gravity). The player spawns facing +X with the gate straight ahead.

local I = Rotor4D.identity()

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

local GROUND_A = vec3(0.30, 0.40, 0.36)
local GROUND_B = vec3(0.20, 0.28, 0.25)
local GATE     = vec3(0.62, 0.56, 0.42)
local GATE_B   = vec3(0.44, 0.39, 0.28)
local THRESH   = vec3(0.96, 0.78, 0.28)

local GATE_X = 6.0    -- gate sits this far ahead along +X
local GATE_W = 1.5    -- gate opening half-width (in Z and W)

function load()
  engine.use_standard_input(true)
  engine.body.radius = 0.8
  engine.body.gravityScale = 1.0
  engine.cam4d.pos = vec4(0, 0.8, 0, 0)
  engine.cam4d.yaw = Rotor4D.identity()   -- face +X (toward the gate)
  engine.cam4d.pitch = 0.0

  -- Ground slab + flat collider (top at y=0).
  do
    local mesh = engine.make_ground(vec4(20, 0.1, 20, 20))
    local set  = engine.instance_set()
    set:add(vec4(GATE_X * 0.5, -0.1, 0, 0), I, GROUND_A, GROUND_B)
    parts[#parts + 1] = { mesh = mesh, set = set }
    engine.world:add_flat_ground(0.0, 80.0)
  end

  -- Gate: two posts (solid) + a lintel + a bright threshold pad on the ground.
  box(GATE_X, 1.6, GATE_W, 0, 0.3, 1.6, 0.3, 0.3, GATE, GATE_B, true)
  box(GATE_X, 1.6, -GATE_W, 0, 0.3, 1.6, 0.3, 0.3, GATE, GATE_B, true)
  box(GATE_X, 3.4, 0, 0, 0.3, 0.3, GATE_W + 0.3, 0.3, GATE, GATE_B, true)
  box(GATE_X, 0.02, 0, 0, 0.4, 0.02, GATE_W, GATE_W, THRESH, GATE)
end

function update()
  -- Step into the gate opening -> return to the bedroom.
  local p = engine.cam4d.pos
  if math.abs(p.x - GATE_X) < 0.7 and math.abs(p.z) < GATE_W and math.abs(p.w) < GATE_W then
    engine.goto_scene("bedroom")
  end
end

function render()
  for _, part in ipairs(parts) do
    engine.draw_instances(part.mesh, part.set)
  end
  engine.draw_outer_cube()
end

function render_hud()
  local lines = {}
  if not engine.inside_mode() then
    lines[#lines + 1] = "Hall. Press TAB to step inside the 4D view."
  else
    lines[#lines + 1] = "Hall. Walk forward (E) into the gate to return to the bedroom."
  end
  engine.hud_window("Hall", lines)
end
