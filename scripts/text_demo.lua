-- scripts/text_demo.lua
-- Visual demo of the Lua text API: default 5x7 font, default8 font,
-- setTextSize, text(), textWrapped(), getTextWidth, getTextHeight.
--
-- Run with a host that loads this script (e.g. sprite_sdl_test --lua script path).

function update(self, dt)
    -- no-op; could drive a simple animation (e.g. size/color) here
end

function draw(self)
    local w, h = getWidth(), getHeight()
    clear(0)

    -- Title bar
    setColor(7)
    setTextSize(1)
    setFont("default")
    text("TEXT DEMO", 4, 2)

    -- Default 5x7 at size 1
    setColor(15)
    setTextSize(1)
    setFont("default")
    text("Default 5x7 size 1", 4, 14)

    -- Default 5x7 at size 2
    setColor(12)
    setTextSize(2)
    text("Size 2", 4, 24)

    -- Default 5x7 at size 3
    setColor(11)
    setTextSize(3)
    text("Size 3", 4, 42)

    -- Switch to default8 (8pt proportional)
    setColor(14)
    setTextSize(1)
    setFont("default8")
    text("default8 font", 4, 62)

    -- Wrapped text in a column
    setColor(9)
    setFont("default")
    setTextSize(1)
    textWrapped("The quick brown fox jumps over the lazy dog.", 4, 76, w - 8)

    -- Measurement example: center a short label
    local msg = "Centered"
    setColor(15)
    local tw = getTextWidth(msg)
    local th = getTextHeight()
    text(msg, math.floor((w - tw) / 2), h - th - 4)

    -- Small label at bottom-right using measurement
    setTextSize(1)
    local footer = "getTextWidth/getTextHeight"
    setColor(8)
    text(footer, w - getTextWidth(footer) - 4, h - 8)
end
