-- Test script for C_LuaScript component
-- Demonstrates basic drawing and animation capabilities

print("Loading test script for C_LuaScript component...")

-- Global variables
local frame = 0
local colors = {1, 3, 7, 11, 15}  -- Different gray levels
local colorIndex = 1

-- Initialize function (called once when script loads)
function init()
    print("Script initialized!")
    print("Canvas size:", COMPONENT_WIDTH, "x", COMPONENT_HEIGHT)
end

-- Update function (called every frame)
function update(dt)
    frame = frame + 1
    
    -- Change color every 60 frames (1 second at 60fps)
    if frame % 60 == 0 then
        colorIndex = colorIndex + 1
        if colorIndex > #colors then
            colorIndex = 1
        end
    end
end

-- Draw function (called every frame)
function draw()
    -- Clear with black background
    clear(0)
    
    -- Get current time for animations
    local t = time or 0
    
    -- Draw animated elements
    local centerX = COMPONENT_WIDTH / 2
    local centerY = COMPONENT_HEIGHT / 2
    local currentColor = colors[colorIndex]
    
    -- Animated circle
    local radius = 15 + math.sin(t * 2) * 5
    fillCircle(centerX, centerY, radius, currentColor)
    
    -- Orbiting dots
    for i = 1, 4 do
        local angle = t + i * (math.pi / 2)
        local x = centerX + math.cos(angle) * 25
        local y = centerY + math.sin(angle) * 25
        fillCircle(x, y, 3, 15 - currentColor)  -- Inverse color
    end
    
    -- Corner indicators
    fillRect(2, 2, 4, 4, currentColor)
    fillRect(COMPONENT_WIDTH - 6, 2, 4, 4, currentColor)
    fillRect(2, COMPONENT_HEIGHT - 6, 4, 4, currentColor)
    fillRect(COMPONENT_WIDTH - 6, COMPONENT_HEIGHT - 6, 4, 4, currentColor)
    
    -- Frame counter (simple text using rectangles)
    drawFrameNumber(frame, 10, 10, currentColor)
end

-- Simple function to draw a number using rectangles
function drawFrameNumber(num, x, y, color)
    -- Just draw a simple indicator that changes
    local blockSize = (num % 10) + 1
    fillRect(x, y, blockSize * 2, 3, color)
end

print("Test script loaded successfully!")