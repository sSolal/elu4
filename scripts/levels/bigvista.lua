-- Level 10 (Lua port) — "Big Vista". A wide 4D landscape of rolling hills.
-- The terrain is built in script as one tetrahedral height-field mesh (engine.make_mesh)
-- and footing is analytic terrain-following. Find the far mountain (off the forward
-- axis - you must turn), climb it, and touch the summit orb.

local SURFACE_Y, EYE_HEIGHT, SPEED = 0.0, 1.6, 6.0
local NG, EXTENT, HILL_AMP = 16, 96.0, 3.0
local MTN_X, MTN_Z, MTN_W, MTN_H, MTN_R = 60.0, 52.0, -20.0, 28.0, 50.0
local INTERACT_DIST = 4.5

local VALLEY, PEAK, ORB_COL = vec3(0.24, 0.42, 0.26), vec3(0.78, 0.80, 0.84), vec3(1.00, 0.78, 0.18)

-- Kuhn 6-tet split of a cube and the 4 faces of a tet (from primitives.h).
local kBoxKuhn = { {0,1,3,7}, {0,3,2,7}, {0,2,6,7}, {0,6,4,7}, {0,4,5,7}, {0,5,1,7} }
local faceCombo = { {1,2,3}, {1,2,4}, {1,3,4}, {2,3,4} }   -- 1-based into a 4-vertex tet

local I = Rotor4D.identity()
local terrainMesh, sphereMesh, terrainSet, orbSet
local spherePos
local nearSphere, activated, spaceWasDown = false, false, false

local function clamp(v, a, b) return v < a and a or (v > b and b or v) end
local function mix3(a, b, k) return a * (1.0 - k) + b * k end

local function baseHills(x, z, w)
  local h = 1.00 * math.sin(0.040 * x + 0.7) * math.cos(0.035 * z)
          + 0.55 * math.sin(0.060 * z - 1.3) * math.cos(0.050 * w + 2.1)
          + 0.30 * math.sin(0.045 * (x + w) + 0.4) * math.cos(0.040 * z)
  return (h / 1.85) * HILL_AMP
end
local function mountainBump(x, z, w)
  local dx, dz, dw = x - MTN_X, z - MTN_Z, w - MTN_W
  local r = math.sqrt(dx * dx + dz * dz + dw * dw)
  if r >= MTN_R then return 0.0 end
  return MTN_H * 0.5 * (1.0 + math.cos(3.14159265 * (r / MTN_R)))
end
local function terrainHeight(x, z, w)
  return SURFACE_Y + baseHills(x, z, w) + mountainBump(x, z, w)
end
local function hdist(a, b)
  local dx, dz, dw = a.x - b.x, a.z - b.z, a.w - b.w
  return math.sqrt(dx * dx + dz * dz + dw * dw)
end

local function build_terrain()
  local N, P = NG, NG + 1
  local sp = 2.0 * EXTENT / N
  local low, high = SURFACE_Y - HILL_AMP, SURFACE_Y + HILL_AMP + MTN_H
  local function vid(i, j, k) return (i * P + j) * P + k end   -- 0-based

  local verts, colors = {}, {}
  for i = 0, P - 1 do for j = 0, P - 1 do for k = 0, P - 1 do
    local x, z, w = -EXTENT + i * sp, -EXTENT + j * sp, -EXTENT + k * sp
    local y = terrainHeight(x, z, w)
    verts[#verts + 1] = vec4(x, y, z, w)
    local t = clamp((y - low) / (high - low), 0.0, 1.0)
    colors[#colors + 1] = mix3(VALLEY, PEAK, t)
  end end end

  local function corner(i, j, k, code)
    return vid(i + (code & 1), j + ((code >> 1) & 1), k + ((code >> 2) & 1))
  end
  local tris, seen = {}, {}
  for i = 0, N - 1 do for j = 0, N - 1 do for k = 0, N - 1 do
    for t = 1, 6 do
      local v = {}
      for c = 1, 4 do v[c] = corner(i, j, k, kBoxKuhn[t][c]) end
      for f = 1, 4 do
        local a, b, c2 = v[faceCombo[f][1]], v[faceCombo[f][2]], v[faceCombo[f][3]]
        local lo = math.min(a, b, c2)
        local hi = math.max(a, b, c2)
        local mid = a + b + c2 - lo - hi
        local key = lo * 25000000 + mid * 5000 + hi
        if not seen[key] then
          seen[key] = true
          tris[#tris + 1] = a; tris[#tris + 1] = b; tris[#tris + 1] = c2
        end
      end
    end
  end end end

  return engine.make_mesh{ vertices = verts, colors = colors, triangles = tris, occludes = false }
end

function load()
  engine.use_standard_input(true)
  local h0 = terrainHeight(0, 0, 0)
  engine.cam4d.pos = vec4(0, h0 + EYE_HEIGHT, 0, 0)
  engine.cam4d.speed = SPEED
  engine.body.radius = EYE_HEIGHT
  engine.controls.lockedAxes = engine.AxisLock.NONE
  engine.controls.headReturn = false
  engine.set_scene_far(260.0, 0.22)

  terrainMesh = build_terrain()
  terrainSet = engine.instance_set(); terrainSet:add(vec4(0,0,0,0), I, vec3(1,1,1), vec3(1,1,1))

  sphereMesh = engine.make_hypersphere(1.2)
  orbSet = engine.instance_set()
  local hPeak = terrainHeight(MTN_X, MTN_Z, MTN_W)
  spherePos = vec4(MTN_X, hPeak + 1.5, MTN_Z, MTN_W)
end

function update()
  -- runCameraInput runs automatically; we overwrite Y with analytic terrain-following.
  if engine.inside_mode() then
    local cp = engine.cam4d.pos
    local px = clamp(cp.x, -EXTENT, EXTENT)
    local pz = clamp(cp.z, -EXTENT, EXTENT)
    local pw = clamp(cp.w, -EXTENT, EXTENT)
    local p = vec4(px, terrainHeight(px, pz, pw) + EYE_HEIGHT, pz, pw)
    engine.cam4d.pos = p
    engine.body.pos = p
    engine.body.velY = 0.0

    nearSphere = hdist(p, spherePos) < INTERACT_DIST
    local space = engine.key_down(engine.keys.SPACE)
    local pressed = space and not spaceWasDown
    spaceWasDown = space
    if pressed and nearSphere then activated = true; engine.set_won() end
  else
    nearSphere = false
  end
end

function render()
  engine.draw_instances(terrainMesh, terrainSet)
  local orb = activated and vec3(0.4, 1.0, 0.5) or ORB_COL
  orbSet:clear(); orbSet:add(spherePos, I, orb, orb * 1.3)
  engine.draw_instances(sphereMesh, orbSet)
  engine.draw_outer_cube()
end

function render_hud()
  local lines = { "Big Vista - a wide 4D landscape of rolling hills across the X-Z-W ground." }
  if not engine.inside_mode() then
    lines[#lines + 1] = "Press TAB to step inside the 4D view."
  else
    lines[#lines + 1] = "Move: E/A, Q/D, W/S. Turn: J/O and U/L. Look: I/K."
    if not activated then
      lines[#lines + 1] = "A mountain rises far off with a glowing orb at its peak. It is not straight ahead - turn, find it, and climb."
    end
  end
  if not activated and engine.inside_mode() and nearSphere then
    lines[#lines + 1] = "Press SPACE to touch the orb."
  end
  if activated then lines[#lines + 1] = "You reached the summit - level complete!" end
  engine.hud_window("Big Vista", lines)
  engine.hud_facing(engine.cam4d:get_orientation():forward())
  engine.hud_coord(engine.cam4d.pos, "You")
end
