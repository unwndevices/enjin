-- scripts/text_demo.lua
-- Visual demo of the Lua text API (ADR-0029): the three faces, setTextSize,
-- gfx.text's align, setTextWrap's wrap / ellipsis, getTextWidth. y is the
-- top of the line for every face.
--
-- Run with a host that loads this script (e.g. sprite_sdl_test --lua script path).

function update(self, dt)
    -- no-op; could drive a simple animation (e.g. size/color) here
end

function draw(self)
    local w, h = gfx.getWidth(), gfx.getHeight()
    gfx.clear(0)

    -- Title bar, in monogram (every layer starts in it)
    gfx.setColor(7)
    gfx.setFont("monogram")
    gfx.setTextSize(1)
    gfx.text("TEXT DEMO", 4, 0)

    -- monogram at sizes 1, 2 and 3
    gfx.setColor(15)
    gfx.text("monogram size 1", 4, 12)
    gfx.setColor(12)
    gfx.setTextSize(2)
    gfx.text("Size 2", 4, 22)
    gfx.setColor(11)
    gfx.setTextSize(3)
    gfx.text("Size 3", 4, 44)
    gfx.setTextSize(1)

    -- The other two faces
    gfx.setColor(14)
    gfx.setFont("body")
    gfx.text("body face", 4, 82)
    gfx.setFont("display")
    gfx.text("display", w - 4, 76, "right")

    -- Wrapped at a width, then one line ending in ...
    gfx.setColor(9)
    gfx.setFont("monogram")
    local lines = gfx.text("The quick brown fox jumps over the lazy dog.", 4, 98)
    gfx.setTextWrap("ellipsis", w - 8)
    gfx.text("This line is too long for the screen, so it ends in dots", 4, 98 + lines * 13)
    gfx.setTextWrap("wrap")

    -- Measurement: a label right-aligned by hand, and one by align
    local footer = "getTextWidth"
    gfx.setColor(8)
    gfx.text(footer, w - gfx.getTextWidth(footer) - 4, h - 14)
    gfx.setColor(15)
    gfx.text("Centered", w // 2, h - 28, "center")
end
