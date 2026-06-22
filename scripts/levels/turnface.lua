-- Level 3 (Lua port) — "Turn & Face". Retained heading (4D rotation) in isolation.
-- All translation is locked; only turning does anything. Bring the active beacon
-- into the forward (-W) cone and press Space. Face all 15 to win. (Beacon layout is
-- randomised with a fixed seed, so it is stable but not identical to the C++ build.)

local EYE_HEIGHT, GROUND_XZW, GROUND_Y, FLOOR_TOP = 3.0, 5.0, 0.3, 0.0
local NUM_LANDMARKS, CONE_DEG, HINT_DELAY = 15, 9.0, 10.0
local DIST_MIN, DIST_MAX = 4.0, 22.0
local PITCH_RETURN_DELAY, PITCH_RETURN_RATE = 2.0, 0.6

local COL_ACTIVE, COL_HIGHLIGHT, COL_FACED =
  vec3(0.15, 0.95, 0.30), vec3(1.00, 0.55, 0.10), vec3(0.25, 0.45, 0.95)

local I = Rotor4D.identity()
local beaconMesh, groundMesh, groundSet, beaconSet
local marks = {}
local activeIdx, activeTimer, idleTimer = 1, 0.0, 0.0
local pointing, spaceWasDown, won = false, false, false
local nextHint, staticHint = "", ""

local function clamp(v, a, b) return v < a and a or (v > b and b or v) end
local function mix3(a, b, k) return a * (1.0 - k) + b * k end
local function gauss() return math.sqrt(-2 * math.log(math.random())) * math.cos(6.2831853 * math.random()) end

local function isFaced(lm)
  local cs = engine.cam4d:get_orientation() * (lm.pos - engine.cam4d.pos)
  local len = cs:length()
  if len < 1e-4 then return true end
  return (-cs.w / len) > math.cos(math.rad(CONE_DEG))
end

local function hintFor(lm)
  local rel = lm.pos - engine.cam4d.pos
  local a = math.rad(20.0)
  local y, p = engine.cam4d.yaw, engine.cam4d.pitch
  local cands = {
    { yaw = Rotor4D.from_xw(-a) * y, pitch = p, text = "Look left (J)" },
    { yaw = Rotor4D.from_xw( a) * y, pitch = p, text = "Look right (O)" },
    { yaw = Rotor4D.from_zw(-a) * y, pitch = p, text = "Look ana (U)" },
    { yaw = Rotor4D.from_zw( a) * y, pitch = p, text = "Look kata (L)" },
    { yaw = y, pitch = clamp(p + 20.0, -89.0, 89.0), text = "Look above (I)" },
    { yaw = y, pitch = clamp(p - 20.0, -89.0, 89.0), text = "Look below (K)" },
  }
  local best, bestText = -2.0, ""
  for _, c in ipairs(cands) do
    local ori = Rotor4D.from_yw(math.rad(c.pitch)) * c.yaw
    local cs = ori * rel
    local len = cs:length()
    local cosF = (len > 1e-4) and (-cs.w / len) or -1.0
    if cosF > best then best = cosF; bestText = c.text end
  end
  return bestText
end

function load()
  math.randomseed(20260616)
  engine.use_standard_input(true)
  engine.cam4d.pos = vec4(0, EYE_HEIGHT, 0, 0)
  engine.cam4d.yaw = Rotor4D.identity()
  engine.cam4d.pitch = 0.0
  engine.body.radius = EYE_HEIGHT
  engine.controls.lockedAxes = engine.AxisLock.X | engine.AxisLock.Y | engine.AxisLock.Z | engine.AxisLock.W
  engine.controls.lockIsWorldSpace = true
  engine.controls.headReturn = false
  engine.set_depth_cue("normal")     -- beacons must read vivid at any distance

  beaconMesh = engine.make_hypersphere(0.5)
  beaconSet  = engine.instance_set()
  groundMesh = engine.make_ground(vec4(GROUND_XZW, GROUND_Y, GROUND_XZW, GROUND_XZW))
  groundSet  = engine.instance_set()
  groundSet:add(vec4(0, FLOOR_TOP - GROUND_Y, 0, 0), I, vec3(0.28, 0.31, 0.40), vec3(0.18, 0.20, 0.27))
  engine.world:add_object(vec4(0, FLOOR_TOP - EYE_HEIGHT, 0, 0), EYE_HEIGHT)

  local cp = engine.cam4d.pos
  for _ = 1, NUM_LANDMARKS do
    local h = vec4(gauss(), 0, gauss(), gauss())
    local hl = h:length()
    if hl < 1e-4 then h = vec4(0, 0, 0, -1); hl = 1.0 end
    h = h * (1.0 / hl)
    local e = math.rad(-45.0 + 90.0 * math.random())
    local d = h * math.cos(e) + vec4(0, 1, 0, 0) * math.sin(e)
    local dist = DIST_MIN + (DIST_MAX - DIST_MIN) * math.random()
    marks[#marks + 1] = { pos = cp + d * dist, faced = false }
  end
  marks[1].pos = cp + vec4(0, 0, 0, -9)     -- first beacon dead ahead
  staticHint = hintFor(marks[1])
end

local function faceDot()
  if not (engine.inside_mode() and not won and activeIdx <= #marks
          and activeTimer > HINT_DELAY and not pointing) then return nil end
  local cs = engine.cam4d:get_orientation() * (marks[activeIdx].pos - engine.cam4d.pos)
  local lat = vec3(cs.x, cs.y, cs.z)
  if lat:length() <= 1e-3 then return nil end
  lat = lat:normalize()
  local m = math.max(math.abs(lat.x), math.abs(lat.y), math.abs(lat.z))
  return lat * (0.48 / m)
end

function update()
  -- runCameraInput runs automatically (movement is a no-op; only looking acts).
  local function down(k) return engine.key_down(k) end
  local K = engine.keys
  local looking = down(K.J) or down(K.O) or down(K.U) or down(K.L) or down(K.I) or down(K.K)
  idleTimer = looking and 0.0 or (idleTimer + engine.dt())
  if idleTimer > PITCH_RETURN_DELAY then
    engine.cam4d.pitch = engine.cam4d.pitch * math.exp(-PITCH_RETURN_RATE * engine.dt())
  end

  if won or #marks == 0 then return end
  local active = marks[activeIdx]
  activeTimer = activeTimer + engine.dt()
  pointing = engine.inside_mode() and isFaced(active)

  if pointing then nextHint = ""
  elseif activeTimer < HINT_DELAY then nextHint = staticHint
  else nextHint = hintFor(active) end

  local space = engine.key_down(engine.keys.SPACE)
  if pointing and space and not spaceWasDown then
    active.faced = true
    activeIdx = activeIdx + 1
    activeTimer = 0.0
    if activeIdx > #marks then won = true; engine.set_won()
    else staticHint = hintFor(marks[activeIdx]) end
  end
  spaceWasDown = space
end

function render()
  beaconSet:clear()
  for i = 1, #marks do
    if i ~= activeIdx and marks[i].faced then
      beaconSet:add(marks[i].pos, I, COL_FACED, COL_FACED)
    end
  end
  if not won and activeIdx <= #marks then
    local col = COL_ACTIVE
    if pointing then
      local pulse = 0.5 + 0.5 * math.sin(engine.time() * 6.0)
      col = mix3(COL_HIGHLIGHT, vec3(1, 1, 1), 0.25 * pulse)
    end
    beaconSet:add(marks[activeIdx].pos, I, col, col)
  end
  engine.draw_instances(groundMesh, groundSet)
  engine.draw_instances(beaconMesh, beaconSet)
  engine.draw_outer_cube()

  local dot = faceDot()
  if dot then
    local pulse = 0.5 + 0.5 * math.sin(engine.time() * 5.0)
    engine.draw_marker(dot, 0.045, vec3(1, 1, 1), 0.55 + 0.45 * pulse)
  end
end

function render_hud()
  local lines = { "Turn & Face - your head stays where you point it." }
  if not engine.inside_mode() then
    lines[#lines + 1] = "Press TAB to step inside the 4D view."
  else
    lines[#lines + 1] = "Look: J/O left/right, U/L ana/kata, I/K up/down. You can't move - only turn."
    lines[#lines + 1] = string.format("Faced %d / %d", activeIdx - 1, NUM_LANDMARKS)
    if nextHint ~= "" then lines[#lines + 1] = nextHint end
  end
  if won then lines[#lines + 1] = "All landmarks faced - level complete!"
  elseif pointing then lines[#lines + 1] = "On target - press SPACE to mark it." end
  engine.hud_window("Turn & Face", lines)
  engine.hud_facing(engine.cam4d:get_orientation():forward())
end
