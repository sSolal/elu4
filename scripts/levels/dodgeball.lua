-- Level 2 (Lua port) — "Dodgeball". The X/Z strafe plane in isolation.
-- Balls (hyperspheres) approach head-on down +W at random X/Z offsets; strafe to
-- dodge. Survive 45 s of escalating waves. (Wave timing is stochastic, so this is
-- not pixel-identical to the C++ version, but plays the same.)

local TILE, STEP, TOP_Y, TILE_HALF_Y = 2.0, 4.0, 0.0, 0.5
local NX, NZ, W_NEAR, W_FAR = 2, 2, 0.0, -28.0
local EYE_HEIGHT, PLAYER_R = 1.7, 0.5
local W_SPAWN, W_DESPAWN = -40.0, 3.0
local ARC_PEAK_MIN, ARC_PEAK_MAX = 1.5, 5.0
local WIN_SECONDS = 45.0

local I = Rotor4D.identity()
local floorMesh, ballMesh, mergedSet, ballSet
local balls = {}
local survival, spawnTimer, nextSpawn, hitFlash, won = 0.0, 0.0, 1.0, 0.0, false

local function v4copy(v) return vec4(v.x, v.y, v.z, v.w) end
local function clamp(v, a, b) return v < a and a or (v > b and b or v) end
-- Standard-normal sample (Box-Muller).
local function gauss() return math.sqrt(-2 * math.log(math.random())) * math.cos(6.2831853 * math.random()) end

function load()
  engine.use_standard_input(true)
  engine.cam4d.pos = vec4(0, EYE_HEIGHT, 0, 0)
  engine.cam4d.speed = 3.0
  engine.body.radius = EYE_HEIGHT
  engine.controls.lockedAxes = engine.AxisLock.NONE
  engine.controls.headReturn = true
  engine.controls.headReturnStrength = 7.0
  engine.set_depth_cue("fog")

  ballMesh = engine.make_hypersphere(0.5)
  ballSet  = engine.instance_set()

  -- Floor: a green checkerboard baked into one mesh + one flat collider.
  local floorTile = engine.make_ground(vec4(TILE, TILE_HALF_Y, TILE, TILE))
  local shade = { vec3(0.26, 0.36, 0.28), vec3(0.17, 0.26, 0.20) }
  local tiles = engine.instance_set()
  local visualY = TOP_Y - TILE_HALF_Y
  local wi = 0
  local w = W_NEAR
  while w >= W_FAR do
    for ix = -NX, NX do
      for iz = -NZ, NZ do
        local sh = shade[((ix + iz + wi) & 1) + 1]
        tiles:add(vec4(ix * STEP, visualY, iz * STEP, w), I, sh, sh)
      end
    end
    w = w - STEP; wi = wi + 1
  end
  floorMesh = engine.make_merged(floorTile, tiles)
  mergedSet = engine.instance_set()
  mergedSet:add(vec4(0, 0, 0, 0), I, vec3(1, 1, 1), vec3(1, 1, 1))
  engine.world:add_flat_ground(TOP_Y, 32.0)
end

local function spawn_ball()
  local t = math.min(survival / WIN_SECONDS, 1.0)
  local sigma = 0.65 - 0.15 * t
  local speed = 8.0 + 8.0 * t
  local cp = engine.cam4d.pos
  local p = vec4(cp.x + gauss() * sigma, 0, cp.z + gauss() * sigma, W_SPAWN)
  local yLand = math.random() * cp.y
  local A     = ARC_PEAK_MIN + (ARC_PEAK_MAX - ARC_PEAK_MIN) * math.random()
  local travel = cp.w - W_SPAWN
  local thit = travel / speed
  local D = yLand - cp.y
  local vY0 = (2.0 / thit) * (A + math.sqrt(A * (A - D)))
  local gravY = vY0 * vY0 / (2.0 * A)
  balls[#balls + 1] = { pos = p, vel = vec4(0, vY0, 0, speed),
                        gravY = gravY, radius = 0.5, color = vec3(0.95, 0.35, 0.20) }
end

local function on_hit()
  balls = {}; survival = 0.0; spawnTimer = 0.0; nextSpawn = 1.0; hitFlash = 1.5
end

function update()
  -- runCameraInput runs automatically.
  if hitFlash > 0.0 then hitFlash = math.max(0.0, hitFlash - engine.dt()) end
  if won or not engine.inside_mode() then return end

  survival = survival + engine.dt()
  if survival >= WIN_SECONDS then won = true; engine.set_won(); return end

  local t = survival / WIN_SECONDS
  local mean = 2.0 - 1.6 * t
  spawnTimer = spawnTimer + engine.dt()
  if spawnTimer >= nextSpawn then
    spawnTimer = 0.0
    spawn_ball()
    nextSpawn = -mean * math.log(math.random())     -- exponential (Poisson) gap
  end

  local cp = engine.cam4d.pos
  local kept = {}
  for _, b in ipairs(balls) do
    b.vel = vec4(b.vel.x, b.vel.y - b.gravY * engine.dt(), b.vel.z, b.vel.w)
    b.pos = b.pos + b.vel * engine.dt()
    local nearest = vec4(cp.x, clamp(b.pos.y, 0.0, cp.y), cp.z, cp.w)
    if (b.pos - nearest):length() < b.radius + PLAYER_R then
      on_hit(); return
    end
    if b.pos.w <= cp.w + W_DESPAWN then kept[#kept + 1] = b end
  end
  balls = kept
end

function render()
  engine.draw_instances(floorMesh, mergedSet)
  ballSet:clear()
  for _, b in ipairs(balls) do ballSet:add(b.pos, I, b.color, b.color) end
  if ballSet:size() > 0 then engine.draw_instances(ballMesh, ballSet) end
  engine.draw_outer_cube()
end

function render_hud()
  local lines = { "Dodgeball - balls come straight at you down the depth axis." }
  if not engine.inside_mode() then
    lines[#lines + 1] = "Press TAB to step inside the 4D view."
  else
    lines[#lines + 1] = "Strafe the four horizontal directions to dodge. Moving along W is useless."
    if won then lines[#lines + 1] = "Level complete - you survived!"
    else lines[#lines + 1] = string.format("Survive: %.0f s left", math.max(0.0, WIN_SECONDS - survival)) end
    if hitFlash > 0.0 and not won then lines[#lines + 1] = "Hit! Wave reset - try again." end
  end
  engine.hud_window("Dodgeball", lines)
end
