-- Game scene 1 — "Bedroom" (guided intro). A 4D room with a ground, first-person
-- camera, and one thing on each of its six horizontal walls. The scene opens as a
-- guided sequence: you arrive facing the window, an unseen voice (your aunt — never
-- named in the UI) tells you to look at her, then walks you through the room using the
-- Face & Turn mechanics (guiding dot + directional hints + facing detection, see
-- scripts/levels/turnface.lua), and finally teaches W to walk forward as you follow
-- her down the corridor to scene 2 ("hall").
--
-- Coordinate convention (see Camera4D): X/Z are the horizontal strafe plane, W is
-- forward/depth, Y is up (gravity). The gaze is the camera's -W nose;
--   forward(from_xw(t)) = ( sin t, 0, 0, -cos t)   forward(from_zw(t)) = (0, 0, sin t, -cos t)
-- One thing per horizontal direction, ordered so nothing is spoiled until she points:
--   +W window (front at start) · +X aunt · -X bed · +Z desk · -W closet · -Z door
local I  = Rotor4D.identity()
local PI = math.pi

local YAW_WINDOW = Rotor4D.from_xw(PI)         -- nose -> +W (arrive facing the window)
local YAW_DOOR   = Rotor4D.from_zw(-PI * 0.5)  -- nose -> -Z (face/settle on the corridor)

-- Facing + pacing tuning.
local CONE        = 14.0   -- facing half-angle, degrees
local DWELL_OBJ   = 1.0    -- hold the gaze on a thing this long to satisfy it
local DWELL_NPC   = 0.5
local DWELL_DOOR  = 0.6
local STEP_SAY    = 1.6    -- she speaks; you wait before the dot appears
local STEP_AFTER  = 1.8    -- pause after you've looked, before the next thing
local INTRO_WAIT  = 2.0
local AUNT_HOLD   = 3.2
local HURRY_HOLD  = 3.0
local FOLLOW_PAUSE = 1.5   -- after "Follow me", before the view settles
local ALIGN_TIME  = 1.2    -- ease-to-aligned duration
local PRE_WALK    = 1.2    -- settled beat before she starts moving
local LEAD        = 3.0    -- how far ahead the aunt walks down the corridor

-- Furniture world anchors (assets authored in local space; see assets/*.json).
local BED_AT    = vec4(-3.4, 0, 0,    0)
local DESK_AT   = vec4(0,    0, 4.0,  0)
local STOOL_AT  = vec4(0,    0, 3.0,  0)
local CLOSET_AT = vec4(0,    0, 0,   -4.2)
local WINDOW_AT = vec4(-2.2, 1.7, 0,  4.9)

-- Gaze targets (aim points, raised to ~eye/torso height so a level gaze faces them).
local BED    = vec4(-3.5, 0.7, 0,    0)
local DESK   = vec4(0,    0.9, 4.3,  0)
local CLOSET = vec4(0,    1.1, 0,   -4.4)
local DOOR   = vec4(0,    1.0, -7.0, 0)

-- The aunt: the shared NPC figure, standing in front of the +X wall. Her position is
-- mutable so she can walk out into the corridor at the end.
local auntPos = vec4(2.8, 0, 0, 0)
local function auntHead() return vec4(auntPos.x, 1.0, auntPos.z, auntPos.w) end

-- The three things she points at (in order), with the line she says for each.
local steps = {
  { target = BED,    say = "You have a bed —" },
  { target = CLOSET, say = "a closet for your things —" },
  { target = DESK,   say = "and a desk to work at." },
}

-- Every placed box is one (mesh, instance_set) pair; render() walks the list.
local parts = {}
local bedA, closetA, deskA, stoolA, windowA, auntA
local function box(cx, cy, cz, cw, hx, hy, hz, hw, colA, colB, solid)
  local mesh = engine.make_box(vec4(hx, hy, hz, hw))
  local set  = engine.instance_set()
  set:add(vec4(cx, cy, cz, cw), I, colA, colB or colA)
  parts[#parts + 1] = { mesh = mesh, set = set }
  if solid then
    engine.world:add_box(vec4(cx, cy, cz, cw), vec4(hx, hy, hz, hw))
  end
end

local FLOOR_A = vec3(0.55, 0.45, 0.35)
local FLOOR_B = vec3(0.42, 0.34, 0.26)
local WALL_A  = vec3(0.34, 0.37, 0.45)
local WALL_B  = vec3(0.24, 0.27, 0.34)

local ROOM      = 5.0     -- interior half-extent in X/Z/W
local WALL_T    = 0.2     -- wall thickness (half)
local WALL_H    = 1.5     -- wall half-height (room is 3 units tall)
local DOOR_H    = 1.2     -- doorway half-size (the corridor mouth)
local CORR      = 29.0    -- corridor extends from z=-ROOM out to z=-CORR (~3x the old)
local TRIGGER_Z = CORR - 3.0  -- walk past z = -TRIGGER_Z -> next scene

-- --- Guided-sequence state --------------------------------------------------
local state   = "intro"
local t       = 0.0      -- seconds elapsed in the current step
local dwell   = 0.0      -- seconds the gaze has been held on `active`
local speech  = ""       -- current subtitle line (no speaker shown)
local hint    = ""       -- current directional nudge
local active  = nil      -- current gaze target (vec4) or nil (hides the dot)
local stepIdx = 1
-- Follow-the-aunt bookkeeping.
local auntGoalZ, bobPhase, bobY = 0.0, 0.0, 0.0

-- ---- Face & Turn helpers (adapted from scripts/levels/turnface.lua) ---------
local function isFaced(target)
  local cs = engine.cam4d:get_orientation() * (target - engine.cam4d.pos)
  local len = cs:length()
  if len < 1e-4 then return true end
  return (-cs.w / len) > math.cos(math.rad(CONE))
end

local function faceDot(target)
  if not (engine.inside_mode() and target) then return nil end
  local cs = engine.cam4d:get_orientation() * (target - engine.cam4d.pos)
  local lat = vec3(cs.x, cs.y, cs.z)
  if lat:length() <= 1e-3 then return nil end
  lat = lat:normalize()
  local m = math.max(math.abs(lat.x), math.abs(lat.y), math.abs(lat.z))
  return lat * (0.48 / m)
end

-- Directional nudge in the aunt's voice. J/O = left/right, U/L = ana/kata.
local function hintFor(target)
  local rel = target - engine.cam4d.pos
  local a = math.rad(20.0)
  local y = engine.cam4d.yaw
  local cands = {
    { yaw = Rotor4D.from_xw(-a) * y, text = "A little bit to your left (J)" },
    { yaw = Rotor4D.from_xw( a) * y, text = "A little bit to your right (O)" },
    { yaw = Rotor4D.from_zw(-a) * y, text = "A little bit ana (U)" },
    { yaw = Rotor4D.from_zw( a) * y, text = "A little bit kata (L)" },
  }
  local best, bestText = -2.0, ""
  for _, c in ipairs(cands) do
    local cs = c.yaw * rel
    local len = cs:length()
    local cosF = (len > 1e-4) and (-cs.w / len) or -1.0
    if cosF > best then best = cosF; bestText = c.text end
  end
  return bestText
end

-- ---- Gate / lock helpers ---------------------------------------------------
-- A plane passed `true` turns freely; `false` is soft-locked (nudges ~30° and springs
-- back), so the player learns the key exists before it's available.
local function gates(xw, zw, pitch)
  engine.controls.turnXW = xw
  engine.controls.turnZW = zw
  engine.controls.pitch  = pitch
end

local function lockAllMovement()
  engine.controls.lockedAxes = engine.AxisLock.X | engine.AxisLock.Y
                             | engine.AxisLock.Z | engine.AxisLock.W
  engine.controls.lockIsWorldSpace = true
end

-- Aligned to -Z: leave only world-Z free, so W (which moves along the -W nose = -Z
-- here) walks forward down the corridor; E/A/Q/D map to the locked X/W axes.
local function allowForwardOnly()
  engine.controls.lockedAxes = engine.AxisLock.X | engine.AxisLock.Y | engine.AxisLock.W
  engine.controls.lockIsWorldSpace = true
end

-- ---------------------------------------------------------------------------
function load()
  engine.use_standard_input(true)
  engine.set_focal(2.1)                       -- a touch more zoomed out (default 1.5)
  engine.body.radius = 0.8
  engine.body.gravityScale = 1.0
  engine.cam4d.pos = vec4(0, 0.8, 0, 0)       -- room centre, eye ~0.8 above the floor
  engine.cam4d.yaw = YAW_WINDOW               -- arrive looking at the window (+W)
  engine.cam4d.pitch = 0.0

  gates(false, false, false)                  -- open soft-locked: every key teaches, none frees
  lockAllMovement()
  engine.controls.headReturn = false

  -- Ground: one big non-occluding slab + a flat collider (covers the long corridor).
  do
    local mesh = engine.make_ground(vec4(34, 0.1, 34, 34))
    local set  = engine.instance_set()
    set:add(vec4(0, -0.1, 0, 0), I, FLOOR_A, FLOOR_B)
    parts[#parts + 1] = { mesh = mesh, set = set }
    engine.world:add_flat_ground(0.0, 60.0)
  end

  -- --- Room shell (walls are solid; Y spans 0..3). Five solid walls; -Z has the door.
  box( ROOM, WALL_H, 0, 0, WALL_T, WALL_H, ROOM, ROOM, WALL_A, WALL_B, true)  -- +X (aunt wall)
  box(-ROOM, WALL_H, 0, 0, WALL_T, WALL_H, ROOM, ROOM, WALL_A, WALL_B, true)  -- -X (bed wall)
  box(0, WALL_H,  ROOM, 0, ROOM, WALL_H, WALL_T, ROOM, WALL_A, WALL_B, true)  -- +Z (desk wall)
  box(0, WALL_H, 0,  ROOM, ROOM, WALL_H, ROOM, WALL_T, WALL_A, WALL_B, true)  -- +W (window wall)
  box(0, WALL_H, 0, -ROOM, ROOM, WALL_H, ROOM, WALL_T, WALL_A, WALL_B, true)  -- -W (closet wall)

  -- -Z wall with a central doorway (hole of half DOOR_H in X and W, height 0..2.4),
  -- built as a ring of four strips plus a lintel above.
  local edgeHalf = (ROOM - DOOR_H) * 0.5
  local doorTop = 2.4
  box( DOOR_H + edgeHalf, doorTop * 0.5, -ROOM, 0, edgeHalf, doorTop * 0.5, WALL_T, ROOM, WALL_A, WALL_B, true)
  box(-(DOOR_H + edgeHalf), doorTop * 0.5, -ROOM, 0, edgeHalf, doorTop * 0.5, WALL_T, ROOM, WALL_A, WALL_B, true)
  box(0, doorTop * 0.5, -ROOM,  DOOR_H + edgeHalf, DOOR_H, doorTop * 0.5, WALL_T, edgeHalf, WALL_A, WALL_B, true)
  box(0, doorTop * 0.5, -ROOM, -(DOOR_H + edgeHalf), DOOR_H, doorTop * 0.5, WALL_T, edgeHalf, WALL_A, WALL_B, true)
  box(0, (doorTop + WALL_H * 2) * 0.5, -ROOM, 0,
      DOOR_H, (WALL_H * 2 - doorTop) * 0.5, WALL_T, DOOR_H, WALL_A, WALL_B, true)

  -- --- Corridor: a tube along -Z from z=-ROOM out to z=-CORR --------------------
  do
    local cMid  = -(ROOM + CORR) * 0.5
    local cHalf = (CORR - ROOM) * 0.5
    local CW    = 1.4    -- corridor half-width in X and W
    box( CW, WALL_H, cMid, 0, WALL_T, WALL_H, cHalf, CW, WALL_A, WALL_B, true)
    box(-CW, WALL_H, cMid, 0, WALL_T, WALL_H, cHalf, CW, WALL_A, WALL_B, true)
    box(0, WALL_H, cMid,  CW, CW, WALL_H, cHalf, WALL_T, WALL_A, WALL_B, true)
    box(0, WALL_H, cMid, -CW, CW, WALL_H, cHalf, WALL_T, WALL_A, WALL_B, true)
    box(0, 2.9, cMid, 0, CW, 0.2, cHalf, CW, WALL_A, WALL_B, true)
  end

  -- --- Furniture: loaded from the shared asset library -------------------------
  bedA    = engine.load_asset("assets/bed.json")
  closetA = engine.load_asset("assets/closet.json")
  deskA   = engine.load_asset("assets/desk.json")
  stoolA  = engine.load_asset("assets/stool.json")
  windowA = engine.load_asset("assets/window.json")
  auntA   = engine.load_asset("assets/npc.json")
  engine.asset_colliders(closetA, CLOSET_AT)
end

-- Begin pointing the player at room thing `i` (or move on to the exit).
local function beginStep(i)
  stepIdx = i
  if i > #steps then
    state, t, active = "hurry", 0.0, nil
    speech = "We're in a hurry — let me show you the rest of the building. This way."
    return
  end
  state, t, dwell, active = "step_say", 0.0, 0.0, nil
  speech = steps[i].say
  gates(true, true, false)   -- free yaw, pitch soft-locked
end

function update()
  local dt = engine.dt()
  t = t + dt

  if state == "intro" then
    if t > INTRO_WAIT then
      speech = "Hey, look at me."
      gates(true, false, false)              -- one axis only (XW / J/O); rest soft-locked
      active = auntHead()
      state, t, dwell = "face_aunt", 0.0, 0.0
    end

  elseif state == "face_aunt" then
    active = auntHead()
    local faced = isFaced(active)
    dwell = faced and (dwell + dt) or 0.0
    hint  = faced and "" or hintFor(active)
    if dwell > DWELL_NPC then
      gates(false, false, false)             -- smooth-lock on her while she talks
      active, hint = nil, ""
      speech = "I hope your travel wasn't too long. Here's where you're going to stay."
      state, t = "aunt_intro", 0.0
    end

  elseif state == "aunt_intro" then
    if t > AUNT_HOLD then beginStep(1) end

  elseif state == "step_say" then
    -- She has named the thing; after a beat the guiding dot appears.
    if t > STEP_SAY then
      active = steps[stepIdx].target
      state, dwell = "step_look", 0.0
    end

  elseif state == "step_look" then
    local faced = isFaced(active)
    dwell = faced and (dwell + dt) or 0.0
    hint  = faced and "" or hintFor(active)
    if dwell > DWELL_OBJ then
      active, hint = nil, ""
      state, t = "step_after", 0.0
    end

  elseif state == "step_after" then
    if t > STEP_AFTER then beginStep(stepIdx + 1) end

  elseif state == "hurry" then
    if t > HURRY_HOLD then
      speech = "Through there. Look at the doorway."
      active = DOOR
      gates(true, true, false)
      state, t, dwell = "door_look", 0.0, 0.0
    end

  elseif state == "door_look" then
    local faced = isFaced(DOOR)
    dwell = faced and (dwell + dt) or 0.0
    hint  = faced and "" or hintFor(DOOR)
    if dwell > DWELL_DOOR then
      speech, active, hint = "Follow me.", nil, ""
      state, t = "follow_pause", 0.0
    end

  elseif state == "follow_pause" then
    -- A pause, then the view settles smooth-locked onto the corridor.
    if t > FOLLOW_PAUSE then
      gates(false, false, false)
      engine.controls.headReturn         = true
      engine.controls.headReturnStrength = 6.0
      engine.controls.defaultYaw         = YAW_DOOR
      engine.controls.defaultPitch       = 0.0
      state, t = "align", 0.0
    end

  elseif state == "align" then
    if t > ALIGN_TIME then
      engine.cam4d.yaw   = YAW_DOOR           -- guarantee perfect alignment
      engine.cam4d.pitch = 0.0
      engine.controls.headReturn = false
      state, t = "pre_walk", 0.0
    end

  elseif state == "pre_walk" then
    -- Settled beat — then she starts moving and you're free to walk.
    if t > PRE_WALK then
      allowForwardOnly()
      hint = "Press W to walk forward."
      auntGoalZ, bobPhase, bobY = auntPos.z, 0.0, 0.0
      state, t = "walk", 0.0
    end

  elseif state == "walk" then
    local p = engine.cam4d.pos
    -- Lead the player by LEAD down the corridor, with an initial push to z=-6 so she
    -- visibly steps in first. She only ever advances (-Z); if you stop she eases to a
    -- halt; when you close the gap she sets off again.
    local wantZ = math.min(p.z - LEAD, -6.0)
    auntGoalZ = math.min(auntGoalZ, wantZ)
    local k  = math.min(1.0, 2.2 * dt)
    local nx = auntPos.x + (0.0 - auntPos.x) * k
    local nz = auntPos.z + (auntGoalZ - auntPos.z) * k
    local moving = (auntPos.z - auntGoalZ) > 0.03 or math.abs(auntPos.x) > 0.03
    -- Up/down bob while moving; eases back down to the floor when she stops.
    if moving then bobPhase = bobPhase + dt * 7.0 else bobPhase = 0.0 end
    local bobTarget = moving and (0.12 * (0.5 - 0.5 * math.cos(bobPhase))) or 0.0
    bobY = bobY + (bobTarget - bobY) * math.min(1.0, 10.0 * dt)
    auntPos = vec4(nx, bobY, nz, 0.0)

    if hint ~= "" and p.z < -2.0 then hint = "" end   -- they've got it
    if p.z < -TRIGGER_Z then engine.goto_scene("hall") end
  end
end

function render()
  for _, p in ipairs(parts) do
    engine.draw_instances(p.mesh, p.set)
  end
  engine.draw_asset(bedA,    BED_AT)
  engine.draw_asset(closetA, CLOSET_AT)
  engine.draw_asset(deskA,   DESK_AT)
  engine.draw_asset(stoolA,  STOOL_AT)
  engine.draw_asset(windowA, WINDOW_AT)
  engine.draw_asset(auntA,   auntPos)
  engine.draw_outer_cube()

  if active then
    local dot = faceDot(active)
    if dot then
      local pulse = 0.5 + 0.5 * math.sin(engine.time() * 5.0)
      engine.draw_marker(dot, 0.045, vec3(1, 1, 1), 0.55 + 0.45 * pulse)
    end
  end
end

function render_hud()
  if not engine.inside_mode() then
    engine.hud_window("bedroom", { "Press TAB to step inside the 4D view." })
    return
  end
  local lines = {}
  if speech ~= "" then lines[#lines + 1] = speech end
  if hint   ~= "" then lines[#lines + 1] = hint end
  if #lines > 0 then engine.hud_window("aunt", lines) end
end
