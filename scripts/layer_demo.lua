-- layer_demo.lua -- Multi-layer composition demo for enjin2
-- Draws different content on each layer to verify compositor.

function update(self, dt)
    -- no-op for this demo
end

function draw(self)
    -- Layer 1 (BG): fill with dark blue (palette index 1)
    setLayer(LAYER_BG)
    clear(1)

    -- Layer 2 (MID): draw a green rectangle in the center
    setLayer(LAYER_MID)
    setColor(11)  -- green
    rectangle("fill", 32, 32, 64, 64)

    -- Layer 3 (FG): draw a red circle overlapping the rectangle
    setLayer(LAYER_FG)
    setColor(8)  -- red
    circle("fill", 64, 64, 24)

    -- Layer 4 (UI): draw score text area at top
    setLayer(LAYER_UI)
    setColor(7)  -- white
    rectangle("fill", 0, 0, 128, 12)
    setColor(0)  -- black
    -- Draw "SCORE" as pixel dots (simplified)
    for i = 0, 4 do
        point(4 + i * 2, 4)
    end

    -- Report layer info
    local count = getLayerCount()
    local current = getLayer()
    print("Layers: " .. count .. ", current: " .. current)
end
