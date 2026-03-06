-- scripts/pikachu_demo.lua
-- Sprite rendering demo: loads pikachu using the Lua sprite pool API.
-- Requires the C++ host to push PIKACHU_DATA (lightuserdata), PIKACHU_W, PIKACHU_H
-- as globals before loading this script.
--
-- Run with: ./sprite_sdl_test --lua

local sprite = -1  -- sprite handle (0-15)

function update(self, dt)
    if sprite >= 0 then
        gfx.updateSprite(sprite, dt)  -- dt is seconds; updateSprite expects seconds (Phase 28)
    end
end

function draw(self)
    gfx.clear(0)

    -- Draw pikachu centered on screen
    if sprite >= 0 then
        local cx = math.floor((gfx.getWidth() - PIKACHU_W) / 2)
        local cy = math.floor((gfx.getHeight() - PIKACHU_H) / 2) - 10
        gfx.drawSprite(sprite, cx, cy)
    else
        -- Fallback: draw error indicator
        gfx.setColor(2)
        gfx.rectangle(0, 0, gfx.getWidth(), gfx.getHeight())
    end

    -- Draw palette strip at the bottom for visual verification
    draw_palette_strip()
end

function draw_palette_strip()
    local strip_h = 8
    local strip_y = gfx.getHeight() - strip_h
    local cell_w = 8

    for i = 0, 14 do
        gfx.setColor(i)
        gfx.rectangle(i * cell_w, strip_y, cell_w, strip_h)
    end
end

-- Initialize sprite on script load
if PIKACHU_DATA then
    sprite = gfx.newSprite(PIKACHU_DATA, PIKACHU_W, PIKACHU_H, 1, 1)
    if sprite >= 0 then
        gfx.setFrame(sprite, 0)
    end
end
