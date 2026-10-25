-- Alternates a save and a load every 5 frames, so each load restores a state saved after the
-- previous load. Lives in the libTAS program, so its counters survive the loads.
local CYCLES = 30
local cycles = 0
local frames = 0
local last = -1

local function onFrame()
    local f = movie.currentFrame()
    -- libTAS calls onFrame more than once per frame.
    if f == last then return end
    last = f
    frames = frames + 1
    if cycles >= CYCLES then return end
    if frames % 10 == 5 then
        io.stderr:write("LUA: save at " .. f .. "\n")
        runtime.saveState(1)
    elseif frames % 10 == 0 then
        cycles = cycles + 1
        io.stderr:write("LUA: load " .. cycles .. " at " .. f .. "\n")
        runtime.loadState(1)
    end
end

callback.onFrame(onFrame)
