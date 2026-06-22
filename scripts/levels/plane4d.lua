-- Level 7 (Lua port) — "Plane (4D course)". Genuine 4D flight.
-- Like the Plane level but pitch (Y-W, I/K) is unlocked and the gates are tilted
-- into 4D: rotate your approach onto each gate's facing, not just its position.

local PLANE_SPEED, TURN_SPEED = 6.0, 50.0
local HOOP_OPENING, FACE_T, FACE_W, PLANE_HALF_W = 2.5, 0.14, 0.18, 2.6
local FOLLOW_DIST, FOLLOW_HEIGHT, CAM_AIM_HEIGHT, CAM_SMOOTH = 9.5, 3.0, 1.0, 5.0
local GUIDE_SEGMENTS, GUIDE_HANDLE, GUIDE_NOSE_OUT = 16, 0.55, 2.9
local GUIDE_COLOR = vec3(1.0, 0.12, 0.10)

local HOOP_ACTIVE = vec3(0.15, 0.95, 0.30)
local HOOP_PASSED = vec3(0.34, 0.34, 0.40)
local HOOP_NEXT   = vec3(0.45, 0.55, 0.85)
local AV_WARM, AV_COOL = vec3(1.00, 0.55, 0.20), vec3(0.30, 0.60, 1.00)
local DEC_A, DEC_B = vec3(0.26, 0.28, 0.34), vec3(0.18, 0.20, 0.26)

local pos     = vec4(0, 0, 0, 0)
local prevPos = vec4(0, 0, 0, 0)
local heading = Rotor4D.identity()
local hoops   = {}          -- {center=vec4, orient=Rotor4D}
local current = 1
local won, camInit = false, false

local planeMesh, faceXMesh, faceYMesh, faceZMesh, decorMesh, guideMesh
local decorSet, fxSet, fySet, fzSet, planeSet

local function mix4(a, b, k) return a * (1.0 - k) + b * k end

local function build_guide()
  if current > #hoops then return {} end
  local nose = heading:forward()
  local nl = nose:length()
  if nl < 1e-5 then return {} end
  nose = nose * (1.0 / nl)
  local g = hoops[current]
  local gnose = g.orient:forward()           -- gate pass-through direction
  local gl = gnose:length()
  if gl > 1e-5 then gnose = gnose * (1.0 / gl) end
  local P0 = pos + nose * GUIDE_NOSE_OUT
  local C  = g.center
  local chord = (C - P0):length()
  if chord < 1e-3 then return {} end
  local hh = GUIDE_HANDLE * chord
  local B0, B1, B2, B3 = P0, P0 + nose * hh, C - gnose * hh, C
  local pts = {}
  for i = 0, GUIDE_SEGMENTS do
    local t = i / GUIDE_SEGMENTS
    local u = 1.0 - t
    pts[#pts + 1] = B0*(u*u*u) + B1*(3*u*u*t) + B2*(3*u*t*t) + B3*(t*t*t)
  end
  return pts
end

function load()
  engine.body.gravityScale = 0.0
  engine.set_scene_far(120.0, 0.18)

  planeMesh = engine.make_box(vec4(0.5, 0.5, 0.5, PLANE_HALF_W))
  faceXMesh = engine.make_box(vec4(FACE_T, HOOP_OPENING, HOOP_OPENING, FACE_W))
  faceYMesh = engine.make_box(vec4(HOOP_OPENING, FACE_T, HOOP_OPENING, FACE_W))
  faceZMesh = engine.make_box(vec4(HOOP_OPENING, HOOP_OPENING, FACE_T, FACE_W))
  decorMesh = engine.make_box(vec4(0.6, 0.6, 0.6, 0.6))
  guideMesh = engine.make_polyline(GUIDE_SEGMENTS + 1)
  decorSet, fxSet, fySet, fzSet, planeSet =
    engine.instance_set(), engine.instance_set(), engine.instance_set(),
    engine.instance_set(), engine.instance_set()

  -- Course: "fly" it with the engine's factory rotors so each gate's orientation
  -- is a reachable craft heading.
  local h = Rotor4D.identity()
  local p = vec4(0, 0, 0, 0)
  local function add_hoop(axDeg, ayDeg, azDeg, gap)
    h = Rotor4D.from_xw(math.rad(axDeg)) * Rotor4D.from_yw(math.rad(ayDeg))
      * Rotor4D.from_zw(math.rad(azDeg)) * h
    h:normalize()
    local nose = h:forward()
    p = p + nose * gap
    hoops[#hoops + 1] = { center = p, orient = h }
  end
  add_hoop(  0,   0,   0, 56)
  add_hoop( 18,   0,   0, 64)
  add_hoop(  0,  16,   0, 64)
  add_hoop(  0,   0,  20, 64)
  add_hoop(-22,   0,  14, 68)
  add_hoop(  0, -20,   0, 64)
  add_hoop( 20,  18,   0, 68)
  add_hoop(-16,   0, -22, 68)
  add_hoop(  0,  20,  18, 68)
  add_hoop( 24, -16,   0, 68)
  add_hoop(-20,   0,  20, 68)
  add_hoop(  0, -18, -20, 68)
  add_hoop( 16,  16,  16, 68)

  -- Sky decorations bounding the course (matched LCG, seed 9281).
  local lo, hi = hoops[1].center, hoops[1].center
  for _, hp in ipairs(hoops) do
    lo = vec4(math.min(lo.x, hp.center.x), math.min(lo.y, hp.center.y),
              math.min(lo.z, hp.center.z), math.min(lo.w, hp.center.w))
    hi = vec4(math.max(hi.x, hp.center.x), math.max(hi.y, hp.center.y),
              math.max(hi.z, hp.center.z), math.max(hi.w, hp.center.w))
  end
  lo = lo - vec4(10, 10, 10, 10); hi = hi + vec4(10, 10, 10, 10)
  local seed = 9281
  local function rnd()
    seed = (seed * 1664525 + 1013904223) & 0xFFFFFFFF
    return ((seed >> 8) & 0xFFFFFF) / 0x1000000
  end
  for _ = 1, 60 do
    local q = vec4(lo.x + (hi.x - lo.x) * rnd(), lo.y + (hi.y - lo.y) * rnd(),
                   lo.z + (hi.z - lo.z) * rnd(), lo.w + (hi.w - lo.w) * rnd())
    local ori = Rotor4D.from_xw(rnd() * 6.2831853) * Rotor4D.from_yz(rnd() * 6.2831853)
    decorSet:add(q, ori, DEC_A, DEC_B)
  end
end

function update()
  if engine.inside_mode() and not won then
    local a = math.rad(TURN_SPEED * engine.dt())
    if engine.key_down(engine.keys.J) then heading = Rotor4D.from_xw(-a) * heading end
    if engine.key_down(engine.keys.O) then heading = Rotor4D.from_xw( a) * heading end
    if engine.key_down(engine.keys.U) then heading = Rotor4D.from_zw(-a) * heading end
    if engine.key_down(engine.keys.L) then heading = Rotor4D.from_zw( a) * heading end
    if engine.key_down(engine.keys.I) then heading = Rotor4D.from_yw( a) * heading end
    if engine.key_down(engine.keys.K) then heading = Rotor4D.from_yw(-a) * heading end
    heading:normalize()

    local nose = heading:forward()
    prevPos = pos
    pos = pos + nose * (PLANE_SPEED * engine.dt())

    local g = hoops[current]
    if g then
      local lc = g.orient * (pos - g.center)        -- into gate-local frame
      local lp = g.orient * (prevPos - g.center)
      local crossed = lp.w > 0.0 and lc.w <= 0.0
      local inside  = math.abs(lc.x) < HOOP_OPENING and math.abs(lc.y) < HOOP_OPENING
                  and math.abs(lc.z) < HOOP_OPENING
      if crossed and inside then
        current = current + 1
        if current > #hoops then won = true; engine.set_won() end
      end
    end
  elseif not engine.inside_mode() then
    engine.observer_input()
  end

  local nose = heading:forward()
  local targetPos = pos - nose * FOLLOW_DIST + vec4(0, FOLLOW_HEIGHT, 0, 0)
  local targetYaw = heading
  local targetPitch = -math.deg(math.atan(FOLLOW_HEIGHT - CAM_AIM_HEIGHT, FOLLOW_DIST))
  local cam = engine.cam4d
  if not camInit then
    cam.pos = targetPos; cam.yaw = targetYaw; cam.pitch = targetPitch; camInit = true
  else
    local k = 1.0 - math.exp(-CAM_SMOOTH * engine.dt())
    cam.pos = mix4(cam.pos, targetPos, k)
    cam.yaw = Rotor4D.nlerp(cam.yaw, targetYaw, k); cam.yaw:normalize()
    cam.pitch = cam.pitch + (targetPitch - cam.pitch) * k
  end
end

function render()
  engine.draw_instances(decorMesh, decorSet)

  local R = HOOP_OPENING
  fxSet:clear(); fySet:clear(); fzSet:clear()
  for i = 1, #hoops do
    local col = (i < current) and HOOP_PASSED or (i == current) and HOOP_ACTIVE or HOOP_NEXT
    local g = hoops[i]
    local q = g.orient:reverse()
    local function panel(dx, dy, dz) return g.center + q * vec4(dx, dy, dz, 0), q end
    local w, o
    w, o = panel( R, 0, 0); fxSet:add(w, o, col, col)
    w, o = panel(-R, 0, 0); fxSet:add(w, o, col, col)
    w, o = panel(0,  R, 0); fySet:add(w, o, col, col)
    w, o = panel(0, -R, 0); fySet:add(w, o, col, col)
    w, o = panel(0, 0,  R); fzSet:add(w, o, col, col)
    w, o = panel(0, 0, -R); fzSet:add(w, o, col, col)
  end
  engine.draw_instances(faceXMesh, fxSet)
  engine.draw_instances(faceYMesh, fySet)
  engine.draw_instances(faceZMesh, fzSet)

  local guide = build_guide()
  if #guide > 0 then engine.draw_polyline(guideMesh, guide, GUIDE_COLOR, 2.5) end

  planeSet:clear()
  planeSet:add(pos, heading:reverse(), AV_WARM, AV_COOL)
  engine.draw_instances(planeMesh, planeSet)

  engine.draw_outer_cube()
end

function render_hud()
  local lines = { "Plane (4D course) - thread the tilted gates in order." }
  if not engine.inside_mode() then
    lines[#lines + 1] = "Press TAB to step inside the 4D view."
  else
    lines[#lines + 1] = "J/O (X-W), U/L (Z-W), I/K pitch (Y-W) - full 4D flight."
    lines[#lines + 1] = "Gates are tilted: rotate to line up with each gate's facing."
    lines[#lines + 1] = string.format("Gate %d / %d.", math.min(current, #hoops), #hoops)
  end
  if won then lines[#lines + 1] = "All gates cleared - level complete!" end
  engine.hud_window("Plane 4D", lines)
  engine.hud_facing(engine.cam4d:get_orientation():forward())
  engine.hud_coord(pos, "Plane")
end
