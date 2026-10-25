-- Writes the SilkTasInfo text of every frame, without hitbox lines, to $LIBTAS_TRACE
local path = os.getenv("LIBTAS_TRACE")
if path == nil or path == "" then
    error("LIBTAS_TRACE env var must be set to the trace output path")
end
assert(io.open(path, "w")):close()
-- the game inherits this fd and loading a state restores its offset, appending ignores that
local out = assert(io.open(path, "a"))

local function infoAddress()
    local tasInfoMap = 0x7f0000001000
    if memory.readu64(tasInfoMap) ~= 0x1234567812345678 then
        return 0
    end
    local marker = memory.readu64(tasInfoMap + 8)
    if memory.readu64(marker) ~= 1234567890123456789 then
        return 0
    end
    return memory.readu64(marker + 8)
end

-- C# string: length at +0x10, UTF-16 chars from +0x14
local function readString(address)
    local text = {}
    local len = memory.readu16(address + 0x10)
    for i = 1, len do
        text[i] = string.char(memory.readu16(address + 0x12 + i * 2))
    end
    return table.concat(text)
end

callback.onFrame(function()
    local frame = movie.currentFrame()
    out:write(frame, "\t", movie.isDraw() == 1 and "draw" or "nodraw", "\t")
    local addr = infoAddress()
    if addr == 0 then
        out:write("<no info>\n")
    else
        local lines = {}
        for line in readString(addr):gmatch("[^\r\n]+") do
            if not line:find("^LineHitbox=") and not line:find("^CircleHitbox=") then
                table.insert(lines, line)
            end
        end
        out:write(table.concat(lines, " | "), "\n")
    end
    out:flush()
end)
