-- Lives in the libTAS program, so `loaded` survives the state load.
local loaded = false

local function onFrame()
    local f = movie.currentFrame()
    if f == 10 and not loaded then
        io.stderr:write("LUA: save at " .. f .. "\n")
        runtime.saveState(1)
    elseif f == 11 and not loaded then
        -- kumquat has to remove its partial snapshot, it holds the space that ran out.
        local partial = io.open(os.getenv("TMPDIR") .. "/libtas-gpu/state1")
        if partial then
            partial:close()
            io.stderr:write("TEST: FAIL partial GPU host snapshot left behind\n")
        end
    elseif f == 20 and not loaded then
        io.stderr:write("LUA: load at " .. f .. "\n")
        loaded = true
        runtime.loadState(1)
    end
end

callback.onFrame(onFrame)
