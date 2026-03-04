-- demo.lua — 1-D particle bounce simulation
-- Loaded from SD card by the ESP32 SD Card demo.
-- All output via print() (appears on serial monitor).

local particles = {}
local frame = 0
local bounds = 100.0

-- Initialize 5 particles with different positions and velocities
for i = 1, 5 do
    particles[i] = {
        id   = i,
        pos  = i * 15.0,
        vel  = 20.0 + i * 10.0,
        mass = 0.5 + i * 0.3,
        bounces = 0,
    }
end

print("[demo.lua] Particle simulation started")
print(string.format("  bounds: 0..%.0f | particles: %d", bounds, #particles))
for _, p in ipairs(particles) do
    print(string.format("  p%d: pos=%.1f vel=%.1f mass=%.2f", p.id, p.pos, p.vel, p.mass))
end

function update(dt)
    frame = frame + 1

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
