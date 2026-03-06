-- layer_demo.lua -- Multi-layer composition demo for enjin2
-- Draws different content on each layer to verify compositor.

function update(self, dt)
    -- no-op for this demo
end

function draw(self)
    -- Layer 1 (BG): fill with dark blue (palette index 1)
    gfx.setLayer(gfx.LAYER_BG)
    gfx.clear(1)

    -- Layer 2 (MID): draw a green rectangle in the center
    gfx.setLayer(gfx.LAYER_MID)
    gfx.setColor(11)  -- green
    gfx.rectangle("fill", 32, 32, 64, 64)

    -- Layer 3 (FG): draw a red circle overlapping the rectangle
    gfx.setLayer(gfx.LAYER_FG)
    gfx.setColor(8)  -- red
    gfx.circle("fill", 64, 64, 24)

    -- Layer 4 (UI): draw score text area at top
    gfx.setLayer(gfx.LAYER_UI)
    gfx.setColor(7)  -- white
    gfx.rectangle("fill", 0, 0, 128, 12)
    gfx.setColor(0)  -- black
    -- Draw "SCORE" as pixel dots (simplified)
    for i = 0, 4 do
        gfx.point(4 + i * 2, 4)
    end

    -- Report layer info
    local count = gfx.getLayerCount()
    local current = gfx.getLayer()
    print("Layers: " .. count .. ", current: " .. current)
end
