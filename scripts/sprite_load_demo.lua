-- scripts/sprite_load_demo.lua
-- Sprite runtime loading demo: loads pikachu using the new .njn asset loader
--
-- Run with (from project root):
--   ./build/tests/sprite_sdl_test --script scripts/sprite_load_demo.lua

-- Load sprite at script load time (SDL runner doesn't call init)
local sprite = engine.sprite.load("test_pikachu")
if sprite < 0 then
    engine.log("Failed to load test_pikachu.njn!")
else
    engine.log("Loaded pikachu.njn at handle: " .. tostring(sprite))
end

local flipH, flipV, rot90 = false, false, false

function update(dt)
    if sprite >= 0 then
        gfx.updateSprite(sprite, dt)
    end
    if engine.input.just_pressed(1) then flipH = not flipH end
    if engine.input.just_pressed(2) then rot90 = not rot90 end
    if engine.input.just_pressed(3) then flipV = not flipV end
end

function draw()
    gfx.clear(0)

    if sprite >= 0 then
        local cx = gfx.getWidth() / 2
        local cy = gfx.getHeight() / 2
        gfx.drawSprite(sprite, cx - 19, cy - 19, flipH, flipV, rot90)
    end

    gfx.setColor(15)
    gfx.text("Btn1:FlipH Btn2:Rot90", 2, 2)
    gfx.text("Btn3:FlipV", 2, 12)
end
