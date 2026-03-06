-- reload_test.lua -- Hot-reload test script for enjin2
-- Usage: ./build/bin/enjin2_sdl --script scripts/reload_test.lua
-- Edit the parameters below, save, press F5 to see changes.

-- ===================== TWEAK THESE AND PRESS F5 =====================

local BG_COLOR = 3 -- background layer color    (0-15)
local CIRCLE_COLOR = 2 -- circle palette index       (try: 8=red, 11=green, 12=blue)
local CIRCLE_X = 64 -- circle center X            (0-128)
local CIRCLE_Y = 64 -- circle center Y            (0-128)
local CIRCLE_RADIUS = 20 -- circle radius              (5-60)
local RECT_COLOR = 11 -- rectangle palette index    (0-15)
local RECT_X = 40 -- rectangle top-left X       (0-128)
local RECT_Y = 30 -- rectangle top-left Y       (0-128)
local RECT_W = 40 -- rectangle width            (1-128)
local RECT_H = 30 -- rectangle height           (1-128)

-- ====================================================================

function update(self, dt)
	-- Nothing to update -- this is a static scene.
	-- Add animation here if you want to test reload mid-motion.
end

function draw(self)
	-- Layer 1 (BG): solid fill
	gfx.setLayer(gfx.LAYER_BG)
	gfx.clear(BG_COLOR)

	-- Layer 2 (MID): rectangle
	gfx.setLayer(gfx.LAYER_MID)
	gfx.setColor(RECT_COLOR)
	gfx.rectangle("fill", RECT_X, RECT_Y, RECT_W, RECT_H)

	-- Layer 3 (FG): circle
	gfx.setLayer(gfx.LAYER_FG)
	gfx.setColor(CIRCLE_COLOR)
	gfx.circle("fill", CIRCLE_X, CIRCLE_Y, CIRCLE_RADIUS)

	-- Layer 4 (UI): parameter readout as colored bar
	gfx.setLayer(gfx.LAYER_UI)
	gfx.setColor(7) -- white
	gfx.rectangle("fill", 0, 0, 128, 8)
	gfx.setColor(0) -- black dots showing reload worked
	for i = 0, CIRCLE_RADIUS / 3 do
		gfx.point(2 + i * 3, 3)
	end
end

-- Print parameters on load so console confirms F5 worked
print("--- reload_test.lua loaded ---")
print("  BG_COLOR=" .. BG_COLOR)
print("  CIRCLE: color=" .. CIRCLE_COLOR .. " pos=(" .. CIRCLE_X .. "," .. CIRCLE_Y .. ") r=" .. CIRCLE_RADIUS)
print("  RECT: color=" .. RECT_COLOR .. " pos=(" .. RECT_X .. "," .. RECT_Y .. ") size=" .. RECT_W .. "x" .. RECT_H)
