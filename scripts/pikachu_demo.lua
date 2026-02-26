-- scripts/pikachu_demo.lua
-- Sprite rendering demo: loads pikachu using the Lua sprite pool API.
-- Requires the C++ host to push PIKACHU_DATA (lightuserdata), PIKACHU_W, PIKACHU_H
-- as globals before loading this script.
--
-- Run with: ./sprite_sdl_test --lua

local sprite = -1  -- sprite handle (0-15)

function update(dt)
    if sprite >= 0 then
        updateSprite(sprite, dt * 1000)  -- dt is seconds, updateSprite wants ms
    end
end

function draw()
    clear(0)

    -- Draw pikachu centered on screen
    if sprite >= 0 then
        local cx = math.floor((getWidth() - PIKACHU_W) / 2)
        local cy = math.floor((getHeight() - PIKACHU_H) / 2) - 10
        drawSprite(sprite, cx, cy)
    else
        -- Fallback: draw error indicator
        setColor(2)
        rectangle(0, 0, getWidth(), getHeight())
    end

    -- Draw palette strip at the bottom for visual verification
    draw_palette_strip()
end

function draw_palette_strip()
    local strip_h = 8
    local strip_y = getHeight() - strip_h
    local cell_w = 8

    for i = 0, 14 do
        setColor(i)
        rectangle(i * cell_w, strip_y, cell_w, strip_h)
    end
end

-- Initialize sprite on script load
if PIKACHU_DATA then
    sprite = newSprite(PIKACHU_DATA, PIKACHU_W, PIKACHU_H, 1, 1)
    if sprite >= 0 then
        setFrame(sprite, 0)
    end
end
