-- Level 1 (Lua port) — "Corridor". W-translation = zoom, in isolation.
-- Strafe (X/Z) locked, head springs back to centre; a tiled tube runs along W to a
-- red goal cube. Reach it and press Space. H hides the walls (floor stays).

local WIDTH    = 2.0
local H        = WIDTH * 0.5
local W_HALF   = H
local GAP      = 0.04
local N        = 5
local REARMOST = (N - 0.5) * WIDTH
local GOAL_W   = -9.0

local floorShade = { vec3(0.62, 0.34, 0.13), vec3(0.48, 0.24, 0.08) }
local ceilShade  = { vec3(0.28, 0.47, 0.78), vec3(0.20, 0.36, 0.64) }
local wallShade  = { vec3(0.24, 0.56, 0.48), vec3(0.16, 0.43, 0.36) }

local I = Rotor4D.identity()
local tileMesh, floorSet, wallSet, goalSet
local showWalls, hWasDown, nearGoal, won = true, false, false, false
local goalPos

local function parityAt(w) return math.floor((REARMOST - w) / WIDTH + 0.5) & 1 end

function load()
  engine.use_standard_input(true)
  engine.cam4d.pos = vec4(0, 0, 0, 0)
  engine.body.radius = 0.75
  engine.controls.lockedAxes = engine.AxisLock.X | engine.AxisLock.Z
  engine.controls.lockIsWorldSpace = true
  engine.controls.headReturn = true
  engine.controls.headReturnStrength = 7.0
  engine.controls.defaultYaw = Rotor4D.identity()
  engine.controls.defaultPitch = 0.0

  tileMesh = engine.make_box(vec4(H - GAP, H - GAP, H - GAP, W_HALF - GAP))
  floorSet, wallSet, goalSet =
    engine.instance_set(), engine.instance_set(), engine.instance_set()

  -- Floor row: rear platform (w>0) + corridor (w<0), one cube per segment.
  for s = 0, 2 * N - 1 do
    local w = REARMOST - s * WIDTH
    local sh = floorShade[(s & 1) + 1]
    floorSet:add(vec4(0, -WIDTH, 0, w), I, sh, sh)
    engine.world:add_object(vec4(0, -WIDTH, 0, w), H)   -- only the floor is solid
  end
  -- Walls + ceiling: corridor segments only.
  for s = 0, N - 1 do
    local w = -(s + 0.5) * WIDTH
    local p = parityAt(w) + 1
    wallSet:add(vec4(0,  WIDTH, 0, w), I, ceilShade[p], ceilShade[p])
    wallSet:add(vec4( WIDTH, 0, 0, w), I, wallShade[p], wallShade[p])
    wallSet:add(vec4(-WIDTH, 0, 0, w), I, wallShade[p], wallShade[p])
    wallSet:add(vec4(0, 0,  WIDTH, w), I, wallShade[p], wallShade[p])
    wallSet:add(vec4(0, 0, -WIDTH, w), I, wallShade[p], wallShade[p])
  end

  goalPos = vec4(0, 0, 0, GOAL_W)
  goalSet:add(goalPos, Rotor4D.from_xw(0.4), vec3(1.0, 0.15, 0.15), vec3(1.0, 0.55, 0.2))
end

function update()
  -- runCameraInput runs automatically (standard input).
  local h = engine.key_down(engine.keys.H)
  if h and not hWasDown then showWalls = not showWalls end
  hWasDown = h

  local dw = math.abs(engine.cam4d.pos.w - goalPos.w)
  nearGoal = engine.inside_mode() and dw < 1.5
  if nearGoal and engine.key_down(engine.keys.SPACE) then won = true; engine.set_won() end
end

function render()
  engine.draw_instances(tileMesh, floorSet)
  if showWalls then engine.draw_instances(tileMesh, wallSet) end
  engine.draw_instances(engine.hyper_mesh, goalSet)
  engine.draw_outer_cube()
end

function render_hud()
  local lines = { "Corridor - moving forward zooms in." }
  if not engine.inside_mode() then
    lines[#lines + 1] = "Press TAB to step inside the 4D view."
  else
    lines[#lines + 1] = "W/S: move forward/back along W. Strafing is locked."
    lines[#lines + 1] = "Back out (S) onto the rear floor to observe the tube."
    lines[#lines + 1] = "H: hide/show the walls (the floor stays)."
  end
  if won then lines[#lines + 1] = "Level complete!"
  elseif nearGoal then lines[#lines + 1] = "Press SPACE to interact with the red cube." end
  engine.hud_window("Corridor", lines)
end
