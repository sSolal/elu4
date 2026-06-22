-- Level 5 (Lua port) — "Leap". The Y axis is special (up / gravity).
-- Run the avatar forward along -W across a chain of beams, jumping the gaps. Fall
-- past the kill-plane and respawn at the last beam. Dwell on the goal beam to win.

local FLOOR_HALF_Y, BEAM_HALF, AVATAR_R, JUMP_VEL, KILL_Y = 0.3, 0.9, 0.4, 4.5, -12.0
local FOLLOW_DIST, FOLLOW_HEIGHT, CAM_AIM_HEIGHT, CAM_SMOOTH, WIN_DWELL = 6.0, 4.0, 2.0, 5.0, 0.4

local BEAM_A, BEAM_B = vec3(0.42, 0.46, 0.60), vec3(0.26, 0.30, 0.46)
local GOAL_C = vec3(0.18, 0.95, 0.35)
local AV_WARM, AV_COOL = vec3(1.00, 0.55, 0.20), vec3(0.30, 0.60, 1.00)
local DEC_A, DEC_B = vec3(0.50, 0.42, 0.60), vec3(0.30, 0.46, 0.70)

local I = Rotor4D.identity()
local avatarMesh, decorMesh, goalMesh
local decorSet, goalSet, avatarSet
local platforms = {}        -- {mesh, drawSet, top, w0, w1, goal, anchor}
local goalCenter
local checkpoint, checkpointIdx = nil, 1
local onGoal, won, spaceWasDown, winTimer, camInit = false, false, false, 0.0, false

local function v4copy(v) return vec4(v.x, v.y, v.z, v.w) end
local function mix4(a, b, k) return a * (1.0 - k) + b * k end
local function mix3(a, b, k) return a * (1.0 - k) + b * k end

function load()
  avatarMesh = engine.make_hypercube()
  engine.avatar.pos = vec4(0, AVATAR_R, 0, 0)
  engine.avatar.yaw = Rotor4D.identity()
  engine.avatar.pitch = 0.0
  engine.avatar.speed = 6.0
  engine.body.pos = vec4(0, AVATAR_R, 0, 0)
  engine.body.radius = AVATAR_R
  engine.controls.headReturn = true
  engine.controls.headReturnStrength = 6.0
  engine.controls.defaultYaw = Rotor4D.identity()
  engine.controls.defaultPitch = 0.0

  local cursorW, top = 2.0, 0.0
  local function add_beam(len, dTop, gap, goal)
    cursorW = cursorW - gap; top = top + dTop
    local w0, w1 = cursorW, cursorW - len
    local mesh = engine.make_box(vec4(BEAM_HALF, FLOOR_HALF_Y, BEAM_HALF, 0.5 * len))
    local i = #platforms
    local a = ((i & 1) == 1) and BEAM_B or BEAM_A
    local b = ((i & 1) == 1) and BEAM_A or BEAM_B
    local set = engine.instance_set()
    set:add(vec4(0, top - FLOOR_HALF_Y, 0, 0.5 * (w0 + w1)), I, a, b)
    platforms[#platforms + 1] = { mesh = mesh, drawSet = set, top = top,
      w0 = w0, w1 = w1, goal = goal, anchor = vec4(0, top + AVATAR_R, 0, 0.5 * (w0 + w1)) }
    cursorW = w1
    -- Colliders tiled along the span.
    local span = w0 - w1
    local n = math.max(1, math.ceil(span / (2.0 * BEAM_HALF)))
    for j = 0, n - 1 do
      local t = (n == 1) and 0.5 or j / (n - 1)
      local wc = w0 - BEAM_HALF - t * (span - 2.0 * BEAM_HALF)
      engine.world:add_object(vec4(0, top - BEAM_HALF, 0, wc), BEAM_HALF)
    end
  end
  add_beam(8.0,  0.0, 0.0, false)
  add_beam(4.0,  0.7, 2.5, false)
  add_beam(6.0, -0.6, 3.0, false)
  add_beam(3.0,  0.6, 2.5, false)
  add_beam(5.0, -0.8, 3.2, false)
  add_beam(3.0,  0.7, 2.5, false)
  add_beam(4.0, -0.5, 3.0, false)
  add_beam(6.0,  0.5, 2.5, true)

  local g = platforms[#platforms]
  goalCenter = vec4(0, g.top + 0.06, 0, 0.5 * (g.w0 + g.w1))
  goalMesh = engine.make_box(vec4(BEAM_HALF, 0.05, BEAM_HALF, 0.5 * (g.w0 - g.w1)))
  goalSet, avatarSet = engine.instance_set(), engine.instance_set()

  -- Sky decorations (matched LCG, seed 1337).
  decorMesh = engine.make_box(vec4(0.6, 0.6, 0.6, 0.6))
  decorSet = engine.instance_set()
  local wMax = 4.0
  local wMin = platforms[#platforms].w1 - 4.0
  local seed = 1337
  local function rnd()
    seed = (seed * 1664525 + 1013904223) & 0xFFFFFFFF
    return ((seed >> 8) & 0xFFFFFF) / 0x1000000
  end
  for _ = 1, 32 do
    local w = wMax + (wMin - wMax) * rnd()
    local side = (rnd() < 0.5) and -1.0 or 1.0
    local x = side * (4.0 + 9.0 * rnd())
    local z = (rnd() - 0.5) * 24.0
    local y = -2.0 + 11.0 * rnd()
    local ori = Rotor4D.from_xw(rnd() * 6.28318) * Rotor4D.from_yz(rnd() * 6.28318)
    decorSet:add(vec4(x, y, z, w), ori, DEC_A, DEC_B)
  end

  checkpoint = platforms[1].anchor
  checkpointIdx = 1
end

function update()
  if engine.inside_mode() then
    local space = engine.key_down(engine.keys.SPACE)
    if space and not spaceWasDown and engine.body.on_ground then engine.body.velY = JUMP_VEL end
    spaceWasDown = space
    engine.drive_avatar()

    local a = engine.avatar.pos
    if engine.body.on_ground then
      for i, p in ipairs(platforms) do
        local inside = math.abs(a.x) < BEAM_HALF + AVATAR_R
                   and math.abs(a.z) < BEAM_HALF + AVATAR_R
                   and a.w < p.w0 + AVATAR_R and a.w > p.w1 - AVATAR_R
                   and math.abs(a.y - (p.top + AVATAR_R)) < 0.5
        if inside then checkpoint = p.anchor; checkpointIdx = i; onGoal = p.goal; break end
      end
    end

    if a.y < KILL_Y then
      engine.avatar.pos = checkpoint
      engine.body.pos = checkpoint
      engine.body.velY = 0.0
      winTimer = 0.0; onGoal = false; camInit = false
    end
  else
    engine.observer_input()
  end

  local facing = engine.avatar.yaw:forward()
  local fh = vec4(facing.x, 0, facing.z, facing.w)
  local fl = fh:length()
  if fl > 1e-4 then fh = fh * (1.0 / fl) end
  local targetPos = engine.avatar.pos - fh * FOLLOW_DIST + vec4(0, FOLLOW_HEIGHT, 0, 0)
  local targetYaw = engine.avatar.yaw
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

  local onGoalNow = onGoal and engine.body.on_ground
  winTimer = onGoalNow and (winTimer + engine.dt()) or 0.0
  if winTimer > WIN_DWELL then won = true; engine.set_won() end
end

function render()
  engine.draw_instances(decorMesh, decorSet)
  for _, p in ipairs(platforms) do engine.draw_instances(p.mesh, p.drawSet) end

  local col = GOAL_C
  if winTimer > 0.0 then
    local pulse = 0.5 + 0.5 * math.sin(engine.time() * 6.0)
    col = mix3(GOAL_C, vec3(1, 1, 1), 0.4 * pulse)
  end
  goalSet:clear(); goalSet:add(goalCenter, I, col, col)
  engine.draw_instances(goalMesh, goalSet)

  avatarSet:clear(); avatarSet:add(engine.avatar.pos, engine.avatar.yaw, AV_WARM, AV_COOL)
  engine.draw_instances(avatarMesh, avatarSet)

  engine.draw_outer_cube()
end

function render_hud()
  local lines = { "Leap - jump the avatar forward from beam to beam. Up (Y) is special." }
  if not engine.inside_mode() then
    lines[#lines + 1] = "Press TAB to step inside the 4D view."
  else
    lines[#lines + 1] = "W run forward, E/A and Q/D nudge sideways, Space to jump."
    lines[#lines + 1] = "Stay on the beams - drift off the side or miss a jump and you fall."
    lines[#lines + 1] = string.format("Beam %d / %d.", checkpointIdx, #platforms)
  end
  if won then lines[#lines + 1] = "On the goal - level complete!"
  elseif winTimer > 0.0 then lines[#lines + 1] = "On the goal - hold it!" end
  engine.hud_window("Leap", lines)
  engine.hud_facing(engine.cam4d:get_orientation():forward())
  engine.hud_coord(engine.cam4d.pos, "Camera")
end
