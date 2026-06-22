-- Level 6 (Lua port) — "Plane (3D-like)".
-- A faithful port of src/levels/PlaneLevel.cpp: a craft under continuous forward
-- thrust whose pitch is locked, so it flies the {X,Z,W} 3-space like an ordinary
-- 3D plane. Gravity is disabled on the body (engine.body.gravityScale = 0) and the
-- whole flight model lives here in script. Steer with J/O (X-W) and U/L (Z-W);
-- thread the hoops in order.

-- --- Tunables (mirror PlaneLevel.h) ----------------------------------------
local FLIGHT_Y      = 0.0
local PLANE_SPEED   = 6.0
local TURN_SPEED    = 50.0
local HOOP_OPENING  = 2.5
local FACE_T        = 0.14
local FACE_W        = 0.18
local PLANE_HALF_W  = 2.6
local FOLLOW_DIST   = 9.5
local FOLLOW_HEIGHT = 3.0
local CAM_AIM_HEIGHT= 1.0
local CAM_SMOOTH    = 5.0
local GUIDE_SEGMENTS= 16
local GUIDE_HANDLE  = 0.55
local GUIDE_NOSE_OUT= 2.9
local GUIDE_COLOR   = vec3(1.0, 0.12, 0.10)

-- Colours
local HOOP_ACTIVE = vec3(0.15, 0.95, 0.30)
local HOOP_PASSED = vec3(0.34, 0.34, 0.40)
local HOOP_NEXT   = vec3(0.45, 0.55, 0.85)
local AV_WARM     = vec3(1.00, 0.55, 0.20)
local AV_COOL     = vec3(0.30, 0.60, 1.00)
local DEC_A       = vec3(0.26, 0.28, 0.34)
local DEC_B       = vec3(0.18, 0.20, 0.26)

-- --- State -----------------------------------------------------------------
local pos       = vec4(0.0, FLIGHT_Y, 0.0, 0.0)
local heading   = Rotor4D.identity()
local hoops     = {}            -- list of hoop centres (vec4)
local current   = 1             -- 1-based index of the next hoop
local won       = false
local camInit   = false

-- meshes / instance sets
local planeMesh, faceXMesh, faceYMesh, faceZMesh, decorMesh, guideMesh
local decorSet, fxSet, fySet, fzSet, planeSet

local I = Rotor4D.identity()

-- Nose direction in world space (local -W). Matches heading.reverse()*(-W).
local function nose_of(h) return h:forward() end

-- Cubic-Bezier trajectory guide from the nose to the active hoop, sampled into
-- GUIDE_SEGMENTS+1 points (all at Y = FLIGHT_Y). Empty when no hoop remains.
local function build_guide()
  if current > #hoops then return {} end
  local nose = nose_of(heading)
  local nl = nose:length()
  if nl < 1e-5 then return {} end
  nose = nose * (1.0 / nl)
  local P0 = pos + nose * GUIDE_NOSE_OUT
  local C  = hoops[current]
  local chord = (C - P0):length()
  if chord < 1e-3 then return {} end
  local h = GUIDE_HANDLE * chord
  local B0 = P0
  local B1 = P0 + nose * h
  local B2 = C + vec4(0.0, 0.0, 0.0, h)
  local B3 = C
  local pts = {}
  for i = 0, GUIDE_SEGMENTS do
    local t = i / GUIDE_SEGMENTS
    local u = 1.0 - t
    local p = B0 * (u*u*u) + B1 * (3*u*u*t) + B2 * (3*u*t*t) + B3 * (t*t*t)
    p.y = FLIGHT_Y
    pts[#pts + 1] = p
  end
  return pts
end

function load()
  engine.body.gravityScale = 0.0          -- gravity off; movement is scripted
  engine.set_scene_far(120.0, 0.18)       -- keep distant hoops readable

  planeMesh = engine.asset_mesh(engine.load_asset("assets/plane.json"))
  faceXMesh = engine.make_box(vec4(FACE_T, HOOP_OPENING, HOOP_OPENING, FACE_W))
  faceYMesh = engine.make_box(vec4(HOOP_OPENING, FACE_T, HOOP_OPENING, FACE_W))
  faceZMesh = engine.make_box(vec4(HOOP_OPENING, HOOP_OPENING, FACE_T, FACE_W))
  decorMesh = engine.make_box(vec4(0.6, 0.6, 0.6, 0.6))
  guideMesh = engine.make_polyline(GUIDE_SEGMENTS + 1)

  decorSet, fxSet, fySet, fzSet, planeSet =
    engine.instance_set(), engine.instance_set(), engine.instance_set(),
    engine.instance_set(), engine.instance_set()

  -- Course: hoops strung along -W, offset in X/Z so you must steer to line up.
  local cw, cx, cz = 0.0, 0.0, 0.0
  local function add_hoop(dx, dz, gap)
    cw = cw - gap; cx = cx + dx; cz = cz + dz
    hoops[#hoops + 1] = vec4(cx, FLIGHT_Y, cz, cw)
  end
  add_hoop( 0.0,  0.0, 30.0)
  add_hoop( 2.6,  0.0, 34.0)
  add_hoop( 0.0,  3.0, 34.0)
  add_hoop(-2.8,  0.0, 34.0)
  add_hoop( 0.0, -3.2, 34.0)
  add_hoop( 3.0,  2.2, 34.0)
  add_hoop(-2.6, -2.6, 34.0)
  add_hoop( 0.0,  3.0, 34.0)

  -- Sky decorations: floating hypercubes (matched LCG, seed 9281).
  local seed = 9281
  local function rnd()
    seed = (seed * 1664525 + 1013904223) & 0xFFFFFFFF
    return ((seed >> 8) & 0xFFFFFF) / 0x1000000
  end
  local wMax = 4.0
  local wMin = hoops[#hoops].w - 6.0
  for _ = 1, 40 do
    local w = wMax + (wMin - wMax) * rnd()
    local x = (rnd() - 0.5) * 48.0
    local z = (rnd() - 0.5) * 48.0
    local y = FLIGHT_Y + (rnd() - 0.5) * 16.0
    local ori = Rotor4D.from_xw(rnd() * 6.2831853) * Rotor4D.from_yz(rnd() * 6.2831853)
    decorSet:add(vec4(x, y, z, w), ori, DEC_A, DEC_B)
  end
end

function update()
  if engine.inside_mode() and not won then
    local a = math.rad(TURN_SPEED * engine.dt())
    if engine.key_down(engine.keys.J) then heading = Rotor4D.from_xw(-a) * heading end
    if engine.key_down(engine.keys.O) then heading = Rotor4D.from_xw( a) * heading end
    if engine.key_down(engine.keys.U) then heading = Rotor4D.from_zw(-a) * heading end
    if engine.key_down(engine.keys.L) then heading = Rotor4D.from_zw( a) * heading end
    heading:normalize()

    local nose = nose_of(heading)
    local prevW = pos.w
    pos = pos + nose * (PLANE_SPEED * engine.dt())
    pos.y = FLIGHT_Y

    local c = hoops[current]
    if c then
      local crossed = prevW > c.w and pos.w <= c.w
      local inside  = math.abs(pos.x - c.x) < HOOP_OPENING
                  and math.abs(pos.z - c.z) < HOOP_OPENING
      if crossed and inside then
        current = current + 1
        if current > #hoops then won = true; engine.set_won() end
      end
    end
  elseif not engine.inside_mode() then
    engine.observer_input()
  end

  -- Chase camera: trail behind + above, looking gently down.
  local nose = nose_of(heading)
  local fh = vec4(nose.x, 0.0, nose.z, nose.w)
  local fl = fh:length()
  if fl > 1e-4 then fh = fh * (1.0 / fl) end
  local targetPos   = pos - fh * FOLLOW_DIST + vec4(0.0, FOLLOW_HEIGHT, 0.0, 0.0)
  local targetYaw   = heading
  local targetPitch = -math.deg(math.atan(FOLLOW_HEIGHT - CAM_AIM_HEIGHT, FOLLOW_DIST))

  local cam = engine.cam4d
  if not camInit then
    cam.pos = targetPos; cam.yaw = targetYaw; cam.pitch = targetPitch
    camInit = true
  else
    local k = 1.0 - math.exp(-CAM_SMOOTH * engine.dt())
    cam.pos   = cam.pos * (1.0 - k) + targetPos * k
    cam.yaw   = Rotor4D.nlerp(cam.yaw, targetYaw, k)
    cam.yaw:normalize()
    cam.pitch = cam.pitch + (targetPitch - cam.pitch) * k
  end
end

function render()
  -- Sky decorations first (farthest context).
  engine.draw_instances(decorMesh, decorSet)

  -- Hoops: six face panels per hollow cube, grouped per face-mesh; colour by state.
  local R = HOOP_OPENING
  fxSet:clear(); fySet:clear(); fzSet:clear()
  for i = 1, #hoops do
    local col = (i <  current) and HOOP_PASSED
             or (i == current) and HOOP_ACTIVE
             or HOOP_NEXT
    local c = hoops[i]
    fxSet:add(c + vec4( R, 0, 0, 0), I, col, col)
    fxSet:add(c + vec4(-R, 0, 0, 0), I, col, col)
    fySet:add(c + vec4(0,  R, 0, 0), I, col, col)
    fySet:add(c + vec4(0, -R, 0, 0), I, col, col)
    fzSet:add(c + vec4(0, 0,  R, 0), I, col, col)
    fzSet:add(c + vec4(0, 0, -R, 0), I, col, col)
  end
  engine.draw_instances(faceXMesh, fxSet)
  engine.draw_instances(faceYMesh, fySet)
  engine.draw_instances(faceZMesh, fzSet)

  -- Trajectory guide.
  local guide = build_guide()
  if #guide > 0 then
    engine.draw_polyline(guideMesh, guide, GUIDE_COLOR, 2.5)
  end

  -- Plane last: warm/cool W gradient, oriented heading:reverse() (see PlaneLevel).
  planeSet:clear()
  planeSet:add(pos, heading:reverse(), AV_WARM, AV_COOL)
  engine.draw_instances(planeMesh, planeSet)

  engine.draw_outer_cube()
end

function render_hud()
  local lines = { "Plane - constant forward thrust; fly through the hoops in order." }
  if not engine.inside_mode() then
    lines[#lines + 1] = "Press TAB to step inside the 4D view."
  else
    lines[#lines + 1] = "J/O and U/L steer (two horizontal turns); you can't climb or dive."
    lines[#lines + 1] = "Miss one? Bank around and come back through it."
    lines[#lines + 1] = string.format("Hoop %d / %d.", math.min(current, #hoops), #hoops)
  end
  if won then lines[#lines + 1] = "All hoops cleared - level complete!" end
  engine.hud_window("Plane", lines)

  engine.hud_facing(engine.cam4d:get_orientation():forward())
  engine.hud_coord(pos, "Plane")
end
