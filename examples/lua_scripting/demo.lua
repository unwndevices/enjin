-- Enjin 2.0 Lua Demo Script
-- Phase 3 Lua Integration Demo

print("Loading demo.lua...")

-- Demo function: Draw a complex pattern
function drawPattern()
    clear(0)  -- Clear to black
    
    local w = getWidth()
    local h = getHeight()
    local t = time()
    
    -- Draw grid pattern
    setColor(4)  -- Dark gray
    for x = 0, w - 1, 8 do
        line(x, 0, x, h - 1)
    end
    for y = 0, h - 1, 8 do
        line(0, y, w - 1, y)
    end
    
    -- Draw animated circles
    setColor(15)  -- White
    local centerX = w / 2
    local centerY = h / 2
    
    for i = 1, 3 do
        local angle = t * (i * 0.5) + i * 2
        local radius = 8 + i * 3
        local x = math.floor(centerX + radius * math.cos(angle))
        local y = math.floor(centerY + radius * math.sin(angle))
        
        circle("line", x, y, 2 + i)
    end
    
    -- Draw some decorative elements
    setColor(10)  -- Light gray
    rectangle("fill", 2, 2, 8, 4)
    rectangle("fill", w - 10, 2, 8, 4)
    rectangle("fill", 2, h - 6, 8, 4)
    rectangle("fill", w - 10, h - 6, 8, 4)
    
    print("Pattern drawn at time:", t)
end

-- Draw the pattern
drawPattern()