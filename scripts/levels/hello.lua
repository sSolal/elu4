-- Smoke-test level for the Lua scripting host (phase 2).
-- Proves the hook forwarding works end-to-end: load runs once, update/render run
-- per frame, and the outer cube draws so there is something on screen. It uses no
-- meshes or physics yet — those arrive with the full engine API in phase 3.

local frames = 0

function load()
  engine.log("hello.lua loaded")
end

function update()
  frames = frames + 1
  -- engine.dt() and engine.inside_mode() are live here.
end

function render()
  engine.draw_outer_cube()
end

function check_win()
  return false  -- back out via the menu / Esc
end
