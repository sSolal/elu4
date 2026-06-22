-- Level 4 (Lua port) — "Third Person". Steer a 4D avatar from outside.
-- The avatar is driven by the shared FPS input path (engine.drive_avatar); the
-- view camera trails behind+above it. Follow the lollipop arrows to the pad.

local ARENA_HALF, FLOOR_DIV, FLOOR_HALF_Y, FLOOR_TOP = 32.0, 8, 0.3, 0.0
local WALL_HALF_Y, WALL_THIN, AVATAR_R, JUMP_VEL = 0.6, 0.1, 0.4, 4.5
local FOLLOW_DIST, FOLLOW_HEIGHT, CAM_AIM_HEIGHT, CAM_SMOOTH = 6.0, 4.0, 2.0, 5.0
local GOAL_HALF, WIN_DWELL = 1.2, 0.4
local SIGN_Y, SHAFT_LEN, SHAFT_THIN, HEAD_HALF, REACH_R = 1.5, 1.6, 0.12, 0.32, 2.0

local FLOOR_A, FLOOR_B = vec3(0.30, 0.33, 0.42), vec3(0.20, 0.22, 0.30)
local WALL_C, GOAL_C = vec3(0.34, 0.30, 0.46), vec3(0.18, 0.95, 0.35)
local ARROW_ACTIVE, ARROW_DONE = vec3(0.15, 0.95, 0.30), vec3(0.50, 0.50, 0.55)
local AV_WARM, AV_COOL = vec3(1.00, 0.55, 0.20), vec3(0.30, 0.60, 1.00)

local I = Rotor4D.identity()
local avatarMesh, floorMesh, goalMesh
local shaftXMesh, shaftZMesh, shaftWMesh, headMesh
local wallXMesh, wallZMesh, wallWMesh
local mergedSet, wallXSet, wallZSet, wallWSet
local shaftXSet, shaftZSet, shaftWSet, headSet, goalSet, avatarSet
local signs = {}
local reachedIdx, won, spaceWasDown, winTimer, camInit = 1, false, false, 0.0, false
local goalCenter

local function v4copy(v) return vec4(v.x, v.y, v.z, v.w) end
local function mix4(a, b, k) return a * (1.0 - k) + b * k end
local function mix3(a, b, k) return a * (1.0 - k) + b * k end

function load()
  avatarMesh = engine.make_hypercube()
  engine.avatar.pos = vec4(0, FLOOR_TOP + AVATAR_R, 0, 0)
  engine.avatar.yaw = Rotor4D.identity()
  engine.avatar.pitch = 0.0
  engine.avatar.speed = 6.0
  engine.body.pos = vec4(0, FLOOR_TOP + AVATAR_R, 0, 0)
  engine.body.radius = AVATAR_R

  -- Floor: FLOOR_DIV^3 checker tiles baked into one mesh + one big collider.
  local cell = ARENA_HALF / FLOOR_DIV
  local floorTile = engine.make_ground(vec4(cell, FLOOR_HALF_Y, cell, cell))
  local tiles = engine.instance_set()
  for ix = 0, FLOOR_DIV - 1 do for iz = 0, FLOOR_DIV - 1 do for iw = 0, FLOOR_DIV - 1 do
    local c = vec4((2*ix - (FLOOR_DIV-1)) * cell, FLOOR_TOP - FLOOR_HALF_Y,
                   (2*iz - (FLOOR_DIV-1)) * cell, (2*iw - (FLOOR_DIV-1)) * cell)
    local col = (((ix + iz + iw) & 1) == 1) and FLOOR_B or FLOOR_A
    tiles:add(c, I, col, col)
  end end end
  floorMesh = engine.make_merged(floorTile, tiles)
  mergedSet = engine.instance_set(); mergedSet:add(vec4(0,0,0,0), I, vec3(1,1,1), vec3(1,1,1))
  engine.world:add_object(vec4(0, FLOOR_TOP - ARENA_HALF, 0, 0), ARENA_HALF)

  -- Perimeter fence: one thin-slab mesh per axis, instanced at + and -.
  local wy = FLOOR_TOP + WALL_HALF_Y
  wallXMesh = engine.make_box(vec4(WALL_THIN, WALL_HALF_Y, ARENA_HALF, ARENA_HALF))
  wallZMesh = engine.make_box(vec4(ARENA_HALF, WALL_HALF_Y, WALL_THIN, ARENA_HALF))
  wallWMesh = engine.make_box(vec4(ARENA_HALF, WALL_HALF_Y, ARENA_HALF, WALL_THIN))
  wallXSet, wallZSet, wallWSet = engine.instance_set(), engine.instance_set(), engine.instance_set()
  for _, s in ipairs({ -1.0, 1.0 }) do
    wallXSet:add(vec4(s * ARENA_HALF, wy, 0, 0), I, WALL_C, WALL_C)
    wallZSet:add(vec4(0, wy, s * ARENA_HALF, 0), I, WALL_C, WALL_C)
    wallWSet:add(vec4(0, wy, 0, s * ARENA_HALF), I, WALL_C, WALL_C)
    engine.world:add_object(vec4(s * 2 * ARENA_HALF, 0, 0, 0), ARENA_HALF)
    engine.world:add_object(vec4(0, 0, s * 2 * ARENA_HALF, 0), ARENA_HALF)
    engine.world:add_object(vec4(0, 0, 0, s * 2 * ARENA_HALF), ARENA_HALF)
  end

  shaftXMesh = engine.make_box(vec4(SHAFT_LEN, SHAFT_THIN, SHAFT_THIN, SHAFT_THIN))
  shaftZMesh = engine.make_box(vec4(SHAFT_THIN, SHAFT_THIN, SHAFT_LEN, SHAFT_THIN))
  shaftWMesh = engine.make_box(vec4(SHAFT_THIN, SHAFT_THIN, SHAFT_THIN, SHAFT_LEN))
  headMesh   = engine.make_box(vec4(HEAD_HALF, HEAD_HALF, HEAD_HALF, HEAD_HALF))
  shaftXSet, shaftZSet, shaftWSet = engine.instance_set(), engine.instance_set(), engine.instance_set()
  headSet, goalSet, avatarSet = engine.instance_set(), engine.instance_set(), engine.instance_set()

  -- Waypoint trail: each sign's arrow points to the next.
  local y = FLOOR_TOP + SIGN_Y
  local LEFT, RIGHT = vec4(-1,0,0,0), vec4(1,0,0,0)
  local ANA, KATA, FORWARD = vec4(0,0,1,0), vec4(0,0,-1,0), vec4(0,0,0,-1)
  local p = vec4(0, y, 0, 0)
  local function step(move, dist, arrowDir)
    p = p + move * dist
    signs[#signs + 1] = { pos = v4copy(p), dir = arrowDir }
  end
  step(FORWARD, 10.0, LEFT)
  step(LEFT,    12.0, ANA)
  step(ANA,     12.0, RIGHT)
  step(RIGHT,   12.0, ANA)
  step(ANA,     12.0, LEFT)
  step(LEFT,    28.0, KATA)
  goalCenter = p + KATA * 12.0
  goalCenter.y = FLOOR_TOP

  goalMesh = engine.make_box(vec4(GOAL_HALF, 0.05, GOAL_HALF, GOAL_HALF))
end

function update()
  if engine.inside_mode() then
    local space = engine.key_down(engine.keys.SPACE)
    if space and not spaceWasDown and engine.body.on_ground then
      engine.body.velY = JUMP_VEL
    end
    spaceWasDown = space
    engine.drive_avatar()

    if reachedIdx <= #signs then
      local d = engine.avatar.pos - signs[reachedIdx].pos
      d.y = 0.0
      if d:length() < REACH_R then reachedIdx = reachedIdx + 1 end
    end
  else
    engine.observer_input()
  end

  -- Follow cam.
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

  local a = engine.avatar.pos
  local onPad = math.abs(a.x - goalCenter.x) < GOAL_HALF
            and math.abs(a.z - goalCenter.z) < GOAL_HALF
            and math.abs(a.w - goalCenter.w) < GOAL_HALF
            and engine.body.on_ground
  winTimer = onPad and (winTimer + engine.dt()) or 0.0
  if winTimer > WIN_DWELL then won = true; engine.set_won() end
end

function render()
  engine.draw_instances(floorMesh, mergedSet)
  engine.draw_instances(wallXMesh, wallXSet)
  engine.draw_instances(wallZMesh, wallZSet)
  engine.draw_instances(wallWMesh, wallWSet)

  shaftXSet:clear(); shaftZSet:clear(); shaftWSet:clear(); headSet:clear()
  local shown = math.min(reachedIdx, #signs)
  for i = 1, shown do
    local s = signs[i]
    local col = (i == reachedIdx) and ARROW_ACTIVE or ARROW_DONE
    local set = shaftWSet
    if s.dir.x ~= 0.0 then set = shaftXSet elseif s.dir.z ~= 0.0 then set = shaftZSet end
    set:add(s.pos, I, col, col)
    headSet:add(s.pos + s.dir * SHAFT_LEN, I, col, col)
  end
  engine.draw_instances(shaftXMesh, shaftXSet)
  engine.draw_instances(shaftZMesh, shaftZSet)
  engine.draw_instances(shaftWMesh, shaftWSet)
  engine.draw_instances(headMesh, headSet)

  if reachedIdx >= #signs then
    local col = GOAL_C
    if winTimer > 0.0 then
      local pulse = 0.5 + 0.5 * math.sin(engine.time() * 6.0)
      col = mix3(GOAL_C, vec3(1, 1, 1), 0.4 * pulse)
    end
    goalSet:clear()
    goalSet:add(vec4(goalCenter.x, FLOOR_TOP + 0.05, goalCenter.z, goalCenter.w), I, col, col)
    engine.draw_instances(goalMesh, goalSet)
  end

  avatarSet:clear()
  avatarSet:add(engine.avatar.pos, engine.avatar.yaw, AV_WARM, AV_COOL)
  engine.draw_instances(avatarMesh, avatarSet)

  engine.draw_outer_cube()
end

function render_hud()
  local lines = { "Third Person - you steer the avatar; the camera trails it." }
  if not engine.inside_mode() then
    lines[#lines + 1] = "Press TAB to step inside the 4D view."
  else
    lines[#lines + 1] = "Move: E/A run, Q/D strafe, W/S along W. Turn: J/O and U/L. Space to jump."
    if reachedIdx <= #signs then
      lines[#lines + 1] = string.format("Reach the arrow, then follow where it points. Sign %d / %d.", reachedIdx, #signs)
    else
      lines[#lines + 1] = "Last arrow pointed kata - step onto the pad."
    end
  end
  if won then lines[#lines + 1] = "On the pad - level complete!"
  elseif winTimer > 0.0 then lines[#lines + 1] = "On the pad - hold it!" end
  engine.hud_window("Third Person", lines)
  engine.hud_facing(engine.cam4d:get_orientation():forward())
  engine.hud_coord(engine.cam4d.pos, "Camera")
end
