-- scripts/text_demo.lua
-- Visual demo of the Lua text API: default 5x7 font, default8 font,
-- setTextSize, text(), textWrapped(), getTextWidth, getTextHeight.
--
-- Run with a host that loads this script (e.g. sprite_sdl_test --lua script path).

function update(self, dt)
    -- no-op; could drive a simple animation (e.g. size/color) here
end

function draw(self)
    local w, h = gfx.getWidth(), gfx.getHeight()
    gfx.clear(0)

    -- Title bar
    gfx.setColor(7)
    gfx.setTextSize(1)
    gfx.setFont("default")
    gfx.text("TEXT DEMO", 4, 2)

    -- Default 5x7 at size 1
    gfx.setColor(15)
    gfx.setTextSize(1)
    gfx.setFont("default")
    gfx.text("Default 5x7 size 1", 4, 14)

    -- Default 5x7 at size 2
    gfx.setColor(12)
    gfx.setTextSize(2)
    gfx.text("Size 2", 4, 24)

    -- Default 5x7 at size 3
    gfx.setColor(11)
    gfx.setTextSize(3)
    gfx.text("Size 3", 4, 42)

    -- Switch to default8 (8pt proportional)
    gfx.setColor(14)
    gfx.setTextSize(1)
    gfx.setFont("default8")
    gfx.text("default8 font", 4, 62)

    -- Wrapped text in a column
    gfx.setColor(9)
    gfx.setFont("default")
    gfx.setTextSize(1)
    gfx.textWrapped("The quick brown fox jumps over the lazy dog.", 4, 76, w - 8)

    -- Measurement example: center a short label
    local msg = "Centered"
    gfx.setColor(15)
    local tw = gfx.getTextWidth(msg)
    local th = gfx.getTextHeight()
    gfx.text(msg, math.floor((w - tw) / 2), h - th - 4)

    -- Small label at bottom-right using measurement
    gfx.setTextSize(1)
    local footer = "getTextWidth/getTextHeight"
    gfx.setColor(8)
    gfx.text(footer, w - gfx.getTextWidth(footer) - 4, h - 8)
end
