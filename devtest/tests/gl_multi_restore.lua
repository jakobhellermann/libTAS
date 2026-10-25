-- Lives in the libTAS program, so `loads` survives the state loads.
local loads = 0
local LOADS = 30

local function onFrame()
    local f = movie.currentFrame()
    if f == 10 and loads == 0 then
        io.stderr:write("LUA: save at " .. f .. "\n")
        runtime.saveState(1)
    elseif f == 20 and loads < LOADS then
        loads = loads + 1
        io.stderr:write("LUA: load " .. loads .. " at " .. f .. "\n")
        runtime.loadState(1)
    end
end

callback.onFrame(onFrame)
