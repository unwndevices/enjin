-- scripts/tamagotchi.lua
-- Tamagotchi game for Enjin Lua
-- Note: all drawing functions are available via gfx.* namespace

local W, H = engine.config.resolution()

-- Game state
local stats = {
    hunger = 0,
    happiness = 100,
    energy = 100,
}

local msg_timer = 0
local msg_text = ""

local function clamp(val, lo, hi)
    if val < lo then return lo end
    if val > hi then return hi end
    return val
end

local function show_msg(str, duration)
    msg_text = str
    msg_timer = duration
end

local function reset()
    stats.hunger = 0
    stats.happiness = 100
    stats.energy = 100
    show_msg("HELLO!", 2.0)
    engine.state.switch("alive")
end

engine.state.switch("alive")

-- UPDATE

function update(dt)
    if dt > 0.05 then dt = 0.05 end

    if msg_timer > 0 then
        msg_timer = msg_timer - dt
    end

    local cur = engine.state.current()

    if cur == "dead" then
        if engine.input.just_pressed(BTN.START) or engine.input.just_pressed(BTN.A) then
            reset()
        end
        return
    end

    if cur == "sleeping" then
        stats.energy = clamp(stats.energy + 20 * dt, 0, 100)
        stats.hunger = clamp(stats.hunger + 1 * dt, 0, 100)
        stats.happiness = clamp(stats.happiness - 1 * dt, 0, 100)

        if stats.energy == 100 then
            show_msg("AWAKE!", 2.0)
            engine.state.switch("alive")
        end

        if engine.input.just_pressed(BTN.A) and stats.energy > 50 then
            show_msg("WAKEY WAKEY!", 2.0)
            engine.state.switch("alive")
        end
    elseif cur == "alive" then
        -- passive decay
        stats.hunger = clamp(stats.hunger + 4 * dt, 0, 100)
        stats.happiness = clamp(stats.happiness - 4 * dt, 0, 100)
        stats.energy = clamp(stats.energy - 2 * dt, 0, 100)

        if engine.input.just_pressed(BTN.LEFT) then
            -- Feed
            if stats.hunger > 0 then
                stats.hunger = clamp(stats.hunger - 20, 0, 100)
                stats.energy = clamp(stats.energy + 5, 0, 100)
                show_msg("YUM!", 1.0)
            else
                show_msg("FULL!", 1.0)
            end
        end

        if engine.input.just_pressed(BTN.RIGHT) then
            -- Play
            if stats.energy >= 15 then
                stats.happiness = clamp(stats.happiness + 20, 0, 100)
                stats.energy = clamp(stats.energy - 15, 0, 100)
                show_msg("WEEEE!", 1.0)
            else
                show_msg("TOO TIRED", 1.0)
            end
        end

        if engine.input.just_pressed(BTN.A) then
            -- Sleep
            show_msg("NIGHT NIGHT", 1.0)
            engine.state.switch("sleeping")
        end

        -- Check death conditions or forced sleep
        if stats.hunger >= 100 or stats.happiness <= 0 then
            engine.state.switch("dead")
        elseif stats.energy <= 0 then
            show_msg("PASSED OUT", 2.0)
            engine.state.switch("sleeping")
        end
    end
end

-- DRAW

local function drawProgressBar(x, y, w, h, val, color)
    gfx.setColor(gfx.COLOR.DARK_GRAY)
    gfx.rectangle(x, y, w, h)
    gfx.setColor(color)
    local fillW = (val / 100) * w
    if fillW > 0 then
        gfx.rectangle(x, y, fillW, h)
    end
end

function draw()
    gfx.clear(gfx.COLOR.BLACK)
    local cur = engine.state.current()

    -- Title
    gfx.setColor(gfx.COLOR.WHITE)
    gfx.textCentered("TAMAGOTCHI", 10, 2)

    -- Stats
    gfx.setColor(gfx.COLOR.GRAY)
    gfx.text("HUNGER:", 10, 34)
    drawProgressBar(80, 33, 100, 10, stats.hunger, gfx.COLOR.RED)

    gfx.text("HAPPINESS:", 10, 49)
    drawProgressBar(80, 48, 100, 10, stats.happiness, gfx.COLOR.GREEN)

    gfx.text("ENERGY:", 10, 64)
    drawProgressBar(80, 63, 100, 10, stats.energy, gfx.COLOR.BLUE)

    -- Draw pet
    local petX, petY = W / 2, H / 2 + 30

    gfx.setColor(gfx.COLOR.PINK)
    gfx.circle(petX, petY, 30)

    -- Face
    if cur == "alive" then
        gfx.setColor(gfx.COLOR.BLACK)
        -- eyes
        gfx.circle(petX - 10, petY - 5, 3)
        gfx.circle(petX + 10, petY - 5, 3)
        -- mouth
        if stats.happiness > 50 then
            gfx.line(petX - 5, petY + 10, petX + 5, petY + 10)
            gfx.setPixel(petX - 6, petY + 9)
            gfx.setPixel(petX + 6, petY + 9)
        else
            gfx.line(petX - 5, petY + 10, petX + 5, petY + 10)
            gfx.setPixel(petX - 6, petY + 11)
            gfx.setPixel(petX + 6, petY + 11)
        end
    elseif cur == "sleeping" then
        gfx.setColor(gfx.COLOR.BLACK)
        gfx.line(petX - 15, petY - 5, petX - 5, petY - 5)
        gfx.line(petX + 5, petY - 5, petX + 15, petY - 5)
        -- Zzz
        gfx.setColor(gfx.COLOR.WHITE)
        if math.floor(engine.time.now() * 2) % 2 == 0 then
            gfx.text("Z", petX + 35, petY - 30)
        end
        gfx.text("z", petX + 45, petY - 40)
    elseif cur == "dead" then
        gfx.setColor(gfx.COLOR.BLACK)
        -- dead eyes (X)
        gfx.line(petX - 15, petY - 10, petX - 5, petY)
        gfx.line(petX - 15, petY, petX - 5, petY - 10)
        gfx.line(petX + 5, petY - 10, petX + 15, petY)
        gfx.line(petX + 5, petY, petX + 15, petY - 10)
        -- dead mouth
        gfx.circle(petX, petY + 10, 4)
    end

    -- Interaction message
    if msg_timer > 0 then
        gfx.setColor(gfx.COLOR.YELLOW)
        gfx.textCentered(msg_text, petY - 50)
    end

    -- Controls prompt
    if cur == "dead" then
        gfx.setColor(gfx.COLOR.RED)
        gfx.textCentered("PET DIED! PRESS A OR START TO RESET", H - 20)
    elseif cur == "alive" then
        gfx.setColor(gfx.COLOR.GRAY)
        gfx.textCentered("LEFT: FEED   RIGHT: PLAY   A: SLEEP", H - 20)
    elseif cur == "sleeping" then
        gfx.setColor(gfx.COLOR.GRAY)
        gfx.textCentered("A: WAKE UP (IF ENERGY > 50)", H - 20)
    end
end
