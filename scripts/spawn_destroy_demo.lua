-- scripts/spawn_destroy_demo.lua
-- Interactive demo: spawn and destroy objects with keyboard
--
-- Controls:
--   Z (btn 4)  = spawn a new particle at a random position
--   X (btn 5)  = destroy the oldest live particle
--   Arrows     = move the cursor
--
-- Run with: ./sprite_sdl_test --script scripts/spawn_destroy_demo.lua

-- State
local W, H = 128, 128
local objects = {}       -- array of {proxy=ObjectProxy, color=int, size=int, age=float}
local spawn_count = 0
local destroy_count = 0
local cursor_x, cursor_y = 64, 64
local flash_timer = 0    -- brief flash on spawn/destroy
local flash_color = 0

engine.random.seed(42)

-- UPDATE
function update(dt)
    W = gfx.getWidth()
    H = gfx.getHeight()

    -- Move cursor with arrows
    local spd = 60
    if engine.input.held(0) then cursor_y = cursor_y - spd * dt end  -- up
    if engine.input.held(1) then cursor_y = cursor_y + spd * dt end  -- down
    if engine.input.held(2) then cursor_x = cursor_x - spd * dt end  -- left
    if engine.input.held(3) then cursor_x = cursor_x + spd * dt end  -- right

    -- Clamp cursor
    if cursor_x < 4 then cursor_x = 4 end
    if cursor_x > W - 4 then cursor_x = W - 4 end
    if cursor_y < 4 then cursor_y = 4 end
    if cursor_y > H - 20 then cursor_y = H - 20 end

    -- Z = spawn
    if engine.input.just_pressed(4) then
        local name = "obj_" .. tostring(spawn_count)
        local proxy = engine.scene.spawn(name)
        if proxy then
            -- Place at cursor position
            proxy.position = {
                x = math.floor(cursor_x),
                y = math.floor(cursor_y)
            }
            local color = engine.random.integer(1, 14)
            local size = engine.random.integer(2, 5)
            table.insert(objects, {
                proxy = proxy,
                color = color,
                size = size,
                age = 0,
                name = name,
            })
            spawn_count = spawn_count + 1
            flash_timer = 0.15
            flash_color = 10  -- green flash
        end
    end

    -- X = destroy oldest
    if engine.input.just_pressed(5) and #objects > 0 then
        local oldest = table.remove(objects, 1)
        engine.scene.destroy(oldest.proxy)
        destroy_count = destroy_count + 1
        flash_timer = 0.15
        flash_color = 9   -- red flash
    end

    -- Age all objects (for pulsing animation)
    for i = 1, #objects do
        objects[i].age = objects[i].age + dt
    end

    -- Decay flash
    if flash_timer > 0 then
        flash_timer = flash_timer - dt
    end
end

-- DRAW
function draw()
    -- Background: flash or black
    if flash_timer > 0 then
        gfx.clear(flash_color)
    else
        gfx.clear(0)
    end

    -- Draw all spawned objects as pulsing shapes
    for i = 1, #objects do
        local o = objects[i]
        local pos = o.proxy.position
        local x = pos.x
        local y = pos.y

        -- Pulsing size based on age
        local pulse = math.sin(o.age * 4) * 1
        local r = o.size + math.floor(pulse)
        if r < 1 then r = 1 end

        gfx.setColor(o.color)
        gfx.circle(x, y, r)

        -- Number label
        gfx.setColor(7)
        gfx.text(tostring(i), x - 2, y - o.size - 6)
    end

    -- Draw cursor crosshair
    gfx.setColor(7)
    local cx = math.floor(cursor_x)
    local cy = math.floor(cursor_y)
    gfx.line(cx - 3, cy, cx + 3, cy)
    gfx.line(cx, cy - 3, cx, cy + 3)

    -- HUD
    gfx.setColor(7)
    gfx.text("Z=spawn  X=destroy", 1, 1)
    gfx.text("alive:" .. #objects, 1, H - 16)
    gfx.text("s:" .. spawn_count .. " d:" .. destroy_count, 1, H - 8)
end
