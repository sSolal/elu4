-- Game scene 2 — "Neighbourhood". A long room on the X axis with eight doors along
-- its +W wall, each opening onto a short corridor. Above every door floats a small
-- hypersphere in a distinct colour. Most corridors are dead ends ("There's nothing
-- there."). The BLUE corridor leads back to the bedroom; the ORANGE one leads to
-- Anna's place (scene 3). The player arrives in front of the blue corridor, and the
-- aunt names the neighbourhood on that first arrival.
--
-- Coordinate convention (see Camera4D): X/Z are the horizontal strafe plane, W is
-- forward/depth, Y is up (gravity). X is the long axis here; the doors line the +W
-- wall and the corridors run out along +W. The default nose points -W, so a player
-- spawned with the identity yaw at a door looks back into the room.

local I  = Rotor4D.identity()
local PI = math.pi

-- Room shell -----------------------------------------------------------------
local HX     = 20.0   -- room half-length along X (the long axis)
local DZ     = 3.5    -- room half-depth in Z
local DW     = 3.5    -- room half-depth in W (the doors are in the +W wall)
local WALL_T = 0.2    -- wall thickness (half)
local WALL_H = 1.5    -- wall half-height (room is 3 units tall)
local TOP    = WALL_H * 2          -- room height
local DOOR_TOP = 2.4               -- doorway opening height
local CW     = 1.4    -- corridor / doorway half-width (matches the bedroom corridor)

-- Corridor lengths: the blue one is long so the swap into the bedroom corridor is
-- seamless; the others are short stubs.
local LONG, MED, SHORT = 12.0, 8.0, 5.0

local WALL_A   = vec3(0.30, 0.34, 0.42)
local WALL_B   = vec3(0.20, 0.24, 0.30)
local GROUND_A = vec3(0.34, 0.30, 0.26)
local GROUND_B = vec3(0.24, 0.21, 0.18)

-- The eight doors, left to right along X. `kind` routes the corridor: "bedroom"
-- and "anna" transition to other scenes; "dead" just reports nothing there.
local doors = {
  { x = -17.5, col = vec3(0.85, 0.25, 0.25), kind = "dead",    len = SHORT },  -- red
  { x = -12.5, col = vec3(0.30, 0.75, 0.35), kind = "dead",    len = SHORT },  -- green
  { x =  -7.5, col = vec3(0.90, 0.80, 0.25), kind = "dead",    len = SHORT },  -- yellow
  { x =  -2.5, col = vec3(0.25, 0.45, 0.95), kind = "bedroom", len = LONG  },  -- blue
  { x =   2.5, col = vec3(0.95, 0.55, 0.15), kind = "anna",    len = MED   },  -- orange
  { x =   7.5, col = vec3(0.65, 0.35, 0.85), kind = "dead",    len = SHORT },  -- purple
  { x =  12.5, col = vec3(0.25, 0.80, 0.85), kind = "dead",    len = SHORT },  -- cyan
  { x =  17.5, col = vec3(0.90, 0.40, 0.70), kind = "dead",    len = SHORT },  -- pink
}
local blue, orange
for _, d in ipairs(doors) do
  if d.kind == "bedroom" then blue   = d end
  if d.kind == "anna"    then orange = d end
end

-- Every placed box is one (mesh, instance_set) pair; render() walks the list.
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

-- Scene state ----------------------------------------------------------------
local sphereA, auntA
local auntPos = vec4(-2.5, 0, 0, 0)   -- set in load() relative to the blue door
local message  = ""                   -- "There's nothing there." while in a dead end
local auntLine = ""                   -- the aunt's first-arrival line
local auntT    = 0.0                  -- counts down the aunt line

function load()
  engine.use_standard_input(true)
  engine.set_focal(2.1)               -- match the bedroom so the blue swap doesn't jump
  engine.body.radius = 0.8
  engine.body.gravityScale = 1.0

  sphereA = engine.load_asset("assets/hypersphere.json")
  auntA   = engine.load_asset("assets/npc.json")

  -- Ground: one big slab spanning the room and all corridors + a flat collider.
  do
    local mesh = engine.make_ground(vec4(HX + 4, 0.1, DZ + 2, DW + LONG + 4))
    local set  = engine.instance_set()
    set:add(vec4(0, -0.1, 0, DW * 0.5), I, GROUND_A, GROUND_B)
    parts[#parts + 1] = { mesh = mesh, set = set }
    engine.world:add_flat_ground(0.0, 120.0)
  end

  -- Room shell: the two X end walls, the back (-W) wall, and the two Z side walls.
  box( HX, WALL_H, 0, 0, WALL_T, WALL_H, DZ, DW, WALL_A, WALL_B, true)   -- +X end
  box(-HX, WALL_H, 0, 0, WALL_T, WALL_H, DZ, DW, WALL_A, WALL_B, true)   -- -X end
  box(0, WALL_H, 0, -DW, HX, WALL_H, DZ, WALL_T, WALL_A, WALL_B, true)   -- -W back wall
  box(0, WALL_H,  DZ, 0, HX, WALL_H, WALL_T, DW, WALL_A, WALL_B, true)   -- +Z side wall
  box(0, WALL_H, -DZ, 0, HX, WALL_H, WALL_T, DW, WALL_A, WALL_B, true)   -- -Z side wall

  -- The +W wall, carrying the eight doorways. Outside the corridor Z-band (|z|>CW)
  -- it's solid full-length; inside the band it's solid X-strips between the doors,
  -- with a lintel above each opening.
  local zStrip = (DZ - CW) * 0.5
  box(0, WALL_H,  (CW + DZ) * 0.5, DW, HX, WALL_H, zStrip, WALL_T, WALL_A, WALL_B, true)
  box(0, WALL_H, -(CW + DZ) * 0.5, DW, HX, WALL_H, zStrip, WALL_T, WALL_A, WALL_B, true)

  local prev = -HX
  for _, d in ipairs(doors) do
    local a, b = prev, d.x - CW         -- solid X-segment up to this doorway
    if b > a then
      box((a + b) * 0.5, WALL_H, 0, DW, (b - a) * 0.5, WALL_H, CW, WALL_T, WALL_A, WALL_B, true)
    end
    -- Lintel above the doorway (DOOR_TOP..TOP).
    box(d.x, (DOOR_TOP + TOP) * 0.5, 0, DW, CW, (TOP - DOOR_TOP) * 0.5, CW, WALL_T,
        WALL_A, WALL_B, true)
    prev = d.x + CW
  end
  if HX > prev then  -- final solid X-segment past the last doorway
    box((prev + HX) * 0.5, WALL_H, 0, DW, (HX - prev) * 0.5, WALL_H, CW, WALL_T,
        WALL_A, WALL_B, true)
  end

  -- The corridors: a tube per door running out along +W, capped at the far end.
  for _, d in ipairs(doors) do
    local mid  = DW + d.len * 0.5
    local half = d.len * 0.5
    box(d.x + CW, WALL_H, 0, mid, WALL_T, WALL_H, CW, half, WALL_A, WALL_B, true)  -- +X wall
    box(d.x - CW, WALL_H, 0, mid, WALL_T, WALL_H, CW, half, WALL_A, WALL_B, true)  -- -X wall
    box(d.x, WALL_H,  CW, mid, CW, WALL_H, WALL_T, half, WALL_A, WALL_B, true)     -- +Z wall
    box(d.x, WALL_H, -CW, mid, CW, WALL_H, WALL_T, half, WALL_A, WALL_B, true)     -- -Z wall
    box(d.x, 2.9, 0, mid, CW, 0.2, CW, half, WALL_A, WALL_B, true)                 -- ceiling
    box(d.x, WALL_H, 0, DW + d.len, CW, WALL_H, CW, WALL_T, WALL_A, WALL_B, true)  -- far cap
  end

  -- Spawn depends on which corridor we came through (the shared "entry" handshake).
  local entry = engine.save_get("entry", "fresh")
  if entry == "from_anna" then
    -- Returned from Anna's: appear inside the orange corridor facing the room, so
    -- walking forward carries you back out into the big room.
    engine.cam4d.pos = vec4(orange.x, 0.8, 0, DW + 3.0)
    engine.cam4d.yaw = I                      -- nose -> -W, into the room
  else
    -- First arrival (from the bedroom): appear in the room at the mouth of the blue
    -- corridor, facing the room. Turning around and walking in returns to the bedroom.
    engine.cam4d.pos = vec4(blue.x, 0.8, 0, DW - 0.6)
    engine.cam4d.yaw = I
  end
  engine.cam4d.pitch = 0.0

  auntPos = vec4(blue.x + 2.5, 0, 0, 0)

  -- The aunt's tour line, the first time the player reaches the neighbourhood.
  if entry ~= "from_anna" and not engine.save_get("met_neighbourhood", false) then
    engine.save_set("met_neighbourhood", true)
    auntLine = "Alright! This is our neighbourhood! You'll get to know people. " ..
               "My room is the purple one, and I think you should go and meet " ..
               "Anna, who lives in the orange room."
    auntT = 22.0
  end
end

function update()
  local dt = engine.dt()
  local p  = engine.cam4d.pos

  if auntT > 0.0 then
    auntT = auntT - dt
    if auntT <= 0.0 then auntLine = "" end
  end

  -- Which corridor (if any) is the player standing in?
  message = ""
  for _, d in ipairs(doors) do
    if math.abs(p.x - d.x) < CW + 0.2 and p.w > DW + 0.2 then
      local deep = p.w > DW + d.len - 1.2
      if d.kind == "bedroom" then
        if deep then
          engine.save_set("entry", "from_neighbourhood")
          engine.goto_scene("bedroom")
        end
      elseif d.kind == "anna" then
        if deep then
          engine.save_set("entry", "from_neighbourhood")
          engine.goto_scene("anna")
        end
      else
        message = "There's nothing there."
      end
      break
    end
  end
end

function render()
  for _, part in ipairs(parts) do
    engine.draw_instances(part.mesh, part.set)
  end

  -- A floating, gently bobbing hypersphere above each doorway, in the door's colour.
  local bob = 0.12 * math.sin(engine.time() * 1.6)
  for _, d in ipairs(doors) do
    engine.draw_asset(sphereA, vec4(d.x, 2.5 + bob, 0, DW - 0.8), I, d.col, d.col * 0.7)
  end

  engine.draw_asset(auntA, auntPos)
  engine.draw_outer_cube()
end

function render_hud()
  if not engine.inside_mode() then
    engine.hud_window("neighbourhood", { "Press TAB to step inside the 4D view." })
    return
  end
  local lines = {}
  if auntLine ~= "" then lines[#lines + 1] = auntLine end
  if message  ~= "" then lines[#lines + 1] = message  end
  if #lines > 0 then engine.hud_window("aunt", lines) end
end
