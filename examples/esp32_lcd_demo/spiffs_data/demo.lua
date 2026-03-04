-- demo.lua — Graphical particle bounce on ILI9341 LCD
-- Loaded from SPIFFS by the ESP32 LCD demo.
-- Draws colored circles on the 320x240 display.

local particles = {}
local frame = 0
local bounds = 100.0

-- Palette color indices for particles (PICO-8-style)
-- 1=dark blue, 2=dark purple, 3=dark green, 4=brown,
-- 5=dark grey, 6=light grey, 7=white, 8=red,
-- 9=orange, 10=yellow, 11=green, 12=blue,
-- 13=indigo, 14=pink
local colors = {8, 9, 10, 11, 12}

-- Initialize 5 particles with different positions, velocities, and sizes
for i = 1, 5 do
    particles[i] = {
        id     = i,
        pos    = i * 15.0,
        vel    = 20.0 + i * 10.0,
        mass   = 0.5 + i * 0.3,
        bounces = 0,
        color  = colors[i],
        radius = 4 + i * 2,
    }
end

print("[demo.lua] Graphical particle simulation started")
print(string.format("  bounds: 0..%.0f | particles: %d", bounds, #particles))
for _, p in ipairs(particles) do
    print(string.format("  p%d: pos=%.1f vel=%.1f mass=%.2f color=%d r=%d",
        p.id, p.pos, p.vel, p.mass, p.color, p.radius))
end

function update(dt)
    frame = frame + 1

    -- Clear background layer to black
    clear(0)

    -- Integrate positions and handle wall bounces
    for _, p in ipairs(particles) do
        p.pos = p.pos + p.vel * dt

        if p.pos > bounds then
            p.pos = bounds - (p.pos - bounds)
            p.vel = -p.vel * 0.9  -- 10% energy loss on bounce
            p.bounces = p.bounces + 1
        elseif p.pos < 0 then
            p.pos = -p.pos
            p.vel = -p.vel * 0.9
            p.bounces = p.bounces + 1
        end
    end

    -- Draw particles as filled circles on the display
    for _, p in ipairs(particles) do
        setColor(p.color)
        -- Map simulation position (0..100) to screen X (0..320)
        local sx = p.pos * 3.2
        -- Spread particles vertically by index
        local sy = 80 + (p.id - 1) * 30
        circle(sx, sy, p.radius, "fill")
    end

    -- Draw title text
    setColor(7)  -- white
    text("Enjin2 LCD Demo", 80, 10)

    -- Draw ground line
    setColor(5)  -- dark grey
    line(0, 230, 319, 230)

    -- Print status every 60 frames (~1 second)
    if frame % 60 == 0 then
        local total_energy = 0
        for _, p in ipairs(particles) do
            total_energy = total_energy + 0.5 * p.mass * p.vel * p.vel
        end
        print(string.format("[frame %d] energy=%.1f", frame, total_energy))
        for _, p in ipairs(particles) do
            print(string.format("  p%d: pos=%6.1f vel=%6.1f bounces=%d",
                p.id, p.pos, p.vel, p.bounces))
        end
    end

    -- Final summary at frame 300
    if frame == 300 then
        print("--- Demo complete ---")
        local total_bounces = 0
        for _, p in ipairs(particles) do
            total_bounces = total_bounces + p.bounces
        end
        print(string.format("Total frames: %d | Total bounces: %d", frame, total_bounces))
        local mem = collectgarbage("count")
        print(string.format("Lua memory: %.1f KB", mem))
    end
end
