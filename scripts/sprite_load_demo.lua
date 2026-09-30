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
local last_a, last_b, last_enc = false, false, false

function update(dt)
    if sprite >= 0 then
        gfx.updateSprite(sprite, dt)
    end
    local a = input and input.button("a") or false
    local b = input and input.button("b") or false
    local enc = input and input.button("enc") or false
    if a and not last_a then flipH = not flipH end
    if b and not last_b then rot90 = not rot90 end
    if enc and not last_enc then flipV = not flipV end
    last_a, last_b, last_enc = a, b, enc
end

function draw()
    gfx.clear(0)

    if sprite >= 0 then
        local cx = gfx.getWidth() / 2
        local cy = gfx.getHeight() / 2
        gfx.drawSprite(sprite, cx - 19, cy - 19, flipH, flipV, rot90)
    end

    gfx.setColor(15)
    gfx.text("A:FlipH B:Rot90", 2, 2)
    gfx.text("Enc:FlipV", 2, 12)
end
