-- Runs savestate steps from $LIBTAS_SAVESTATE_TEST in order, e.g. "s1:50 s2:100 l2:150 l1:130":
-- s<slot>:<frame> saves and l<slot>:<frame> loads once the previous step ran and the frame is reached
local spec = os.getenv("LIBTAS_SAVESTATE_TEST")
if spec == nil then
    error("LIBTAS_SAVESTATE_TEST env var must list steps like \"s1:50 l1:100\"")
end

local steps = {}
for kind, slot, frame in spec:gmatch("([sl])(%d+):(%d+)") do
    table.insert(steps, {kind = kind, slot = tonumber(slot), frame = tonumber(frame)})
end
if #steps == 0 then
    error("no steps in LIBTAS_SAVESTATE_TEST: " .. spec)
end

local next_step = 1
callback.onFrame(function()
    local step = steps[next_step]
    if step == nil or movie.currentFrame() ~= step.frame then
        return
    end
    print(string.format("savestate step %d: %s slot %d at frame %d", next_step,
        step.kind == "s" and "save" or "load", step.slot, step.frame))
    if step.kind == "s" then
        runtime.saveState(step.slot)
    else
        runtime.loadState(step.slot)
    end
    next_step = next_step + 1
end)
