-- Writes the TasInfo payload's info page of every frame to $LIBTAS_TRACE
-- (page layout: "TASINFO1" magic, u64 write count, u32 length, UTF-8 text)
local path = os.getenv("LIBTAS_TRACE")
if path == nil or path == "" then
    error("LIBTAS_TRACE env var must be set to the trace output path")
end
assert(io.open(path, "w")):close()
-- the game inherits this fd and loading a state restores its offset, appending ignores that
local out = assert(io.open(path, "a"))

local PAGE = 0x7ef000000000
local MAGIC = 0x314f464e49534154 -- "TASINFO1" read as a little-endian u64

local function readPage()
    if memory.readu64(PAGE) ~= MAGIC then
        return nil
    end
    local len = memory.readu32(PAGE + 16)
    local bytes = {}
    for i = 0, len - 1 do
        bytes[#bytes + 1] = string.char(memory.readu8(PAGE + 20 + i))
    end
    return table.concat(bytes)
end

callback.onFrame(function()
    out:write(movie.currentFrame(), "\t", movie.isDraw() == 1 and "draw" or "nodraw", "\t",
        readPage() or "<no info>", "\n")
    out:flush()
end)
