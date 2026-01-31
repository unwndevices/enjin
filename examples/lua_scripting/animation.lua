-- Enjin 2.0 Animation Demo Script
-- Demonstrates real-time Lua-driven animations

print("Loading animation.lua...")

-- Global variables for animation state
animTime = 0
particles = {}

-- Initialize particles
function initParticles()
    particles = {}
    for i = 1, 10 do
        particles[i] = {
            x = math.random(0, getWidth() - 1),
            y = math.random(0, getHeight() - 1),
            vx = (math.random() - 0.5) * 2,
            vy = (math.random() - 0.5) * 2,
            color = math.random(8, 15),
            life = 1.0
        }
    end
    print("Initialized", #particles, "particles")
end

-- Update particle system
function updateParticles(dt)
    for i = 1, #particles do
        local p = particles[i]
        
        -- Update position
        p.x = p.x + p.vx * dt * 10
        p.y = p.y + p.vy * dt * 10
        
        -- Bounce off walls
        if p.x < 0 or p.x >= getWidth() then
            p.vx = -p.vx
            p.x = math.max(0, math.min(getWidth() - 1, p.x))
        end
        if p.y < 0 or p.y >= getHeight() then
            p.vy = -p.vy
            p.y = math.max(0, math.min(getHeight() - 1, p.y))
        end
        
        -- Update life
        p.life = p.life - dt * 0.1
        if p.life <= 0 then
            -- Respawn particle
            p.x = math.random(0, getWidth() - 1)
            p.y = math.random(0, getHeight() - 1)
            p.vx = (math.random() - 0.5) * 2
            p.vy = (math.random() - 0.5) * 2
            p.life = 1.0
        end
        
        p.color = math.floor(8 + p.life * 7)  -- Fade from bright to dim
    end
end

-- Draw particle system
function drawParticles()
    for i = 1, #particles do
        local p = particles[i]
        setColor(p.color)
        point(math.floor(p.x), math.floor(p.y))
        
        -- Draw trail
        setColor(p.color - 4)
        point(math.floor(p.x - p.vx), math.floor(p.y - p.vy))
    end
end

-- Main animation function
function animate()
    local currentTime = time()
    local dt = currentTime - animTime
    animTime = currentTime
    
    -- Clear canvas
    clear(0)
    
    -- Update and draw particles
    updateParticles(dt)
    drawParticles()
    
    -- Draw HUD
    setColor(15)
    rectangle("line", 0, 0, getWidth() - 1, getHeight() - 1)
    
    -- Draw time indicator
    local timeIndicator = math.floor((animTime * 10) % (getWidth() - 4))
    setColor(12)
    rectangle("fill", 2 + timeIndicator, 1, 2, 1)
end

-- Initialize the animation
initParticles()
print("Animation system initialized")

-- Run one frame
animate()