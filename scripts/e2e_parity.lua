-- scripts/e2e_parity.lua
-- E2E parity test: 15-color palette grid + button-0 and axis-0 input indicators.
-- Runs identically on SDL3, WASM, and ESP32.
-- Host calls update(dt) and draw() each frame.

local COLS   = 5
local CELL_W = 24
local CELL_H = 24

local function draw_color_grid()
    -- 5x3 grid: 15 cells covering palette indices 0-14.
    -- Positioned at top-left (0, 0) — occupies 120x72 canvas pixels.
    for i = 0, 14 do
        local col = i % COLS
        local row = math.floor(i / COLS)
        setColor(i)
        rectangle(col * CELL_W, row * CELL_H, CELL_W, CELL_H)
    end
end

local function draw_input_indicators()
    local y = getHeight() - CELL_H

    -- Button-0 indicator: bottom-right corner.
    -- Color index 7 (bright) when button 0 held; index 2 (dim) when not held.
    local bx = getWidth() - CELL_W
    if isButtonHeld(0) then
        setColor(7)
    else
        setColor(2)
    end
    rectangle(bx, y, CELL_W, CELL_H)

    -- Axis-0 indicator: one cell to the left of the button indicator.
    -- Color index 10 (active) when abs(getAxis(0)) > 0.1; index 1 (idle) otherwise.
    local ax = bx - CELL_W
    if math.abs(getAxis(0)) > 0.1 then
        setColor(10)
    else
        setColor(1)
    end
    rectangle(ax, y, CELL_W, CELL_H)
end

function update(dt)
    -- No per-frame state to update.
    -- Input is polled by the host and written to InputState before this call.
end

function draw()
    clear(0)
    draw_color_grid()
    draw_input_indicators()
end
