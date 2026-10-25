-- Fast-forward with some frames rendered: libTAS skips presents in between.
-- Lives in the libTAS program, so `loaded` survives the state load.
local loaded = false

local function onFrame()
    local f = movie.currentFrame()
    if f == 1 then
        runtime.setFastForward(1)
    elseif f == 10 and not loaded then
        io.stderr:write("LUA: save at " .. f .. "\n")
        runtime.saveState(1)
    elseif f == 20 and not loaded then
        io.stderr:write("LUA: load at " .. f .. "\n")
        loaded = true
        runtime.loadState(1)
    end
end

callback.onFrame(onFrame)
