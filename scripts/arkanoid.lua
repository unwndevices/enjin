-- scripts/arkanoid.lua
-- Arkanoid-like brick breaker for Enjin Lua
-- Run: ./build/tests/sprite_sdl_test --script scripts/arkanoid.lua
-- Controls: LEFT/RIGHT (or A/D) = move paddle, A = launch ball / restart
-- Note: all drawing globals are also available via engine.graphics.*

-- ═══════════════════════════════════════════════════════════════════════════
-- Constants
-- ═══════════════════════════════════════════════════════════════════════════
local W, H = engine.config.resolution()

-- Game area
local AREA_T = 16          -- top bar for score/lives
local AREA_L = 4           -- left wall
local AREA_R = W - 4       -- right wall inner
local AREA_B = H - 4       -- bottom death zone

-- Paddle
local PADDLE_Y     = H - 20
local PADDLE_W     = 40
local PADDLE_H     = 5
local PADDLE_SPEED = 160

-- Ball
local BALL_R     = 3
local BALL_SPEED = 100

-- Bricks
local BRICK_ROWS  = 6
local BRICK_COLS  = 14
local BRICK_W     = 20
local BRICK_H     = 8
local BRICK_GAP_X = 2
local BRICK_GAP_Y = 2
local BRICK_TOP   = AREA_T + 12

-- Center the brick grid horizontally
local GRID_W    = BRICK_COLS * BRICK_W + (BRICK_COLS - 1) * BRICK_GAP_X
local BRICK_LEFT = (W - GRID_W) / 2

-- Brick colors per row (top = hardest)
local ROW_COLORS = { COLOR.RED, COLOR.ORANGE, COLOR.YELLOW, COLOR.GREEN, COLOR.BLUE, COLOR.INDIGO }
local ROW_POINTS = { 60, 50, 40, 30, 20, 10 }

-- ═══════════════════════════════════════════════════════════════════════════
-- Game State
-- ═══════════════════════════════════════════════════════════════════════════
local paddle_x = 0
local ball = { x = 0, y = 0, vx = 0, vy = 0 }
local bricks = {}      -- [row][col] = true/false
local score = 0
local lives = 3
local bricks_left = 0
local particles = {}

engine.random.seed(42)

-- ═══════════════════════════════════════════════════════════════════════════
-- Initialization
-- ═══════════════════════════════════════════════════════════════════════════

local function reset_bricks()
    bricks = {}
    bricks_left = 0
    for r = 1, BRICK_ROWS do
        bricks[r] = {}
        for c = 1, BRICK_COLS do
            bricks[r][c] = true
            bricks_left = bricks_left + 1
        end
    end
end

local function reset_paddle()
    paddle_x = (W - PADDLE_W) / 2
end

local function attach_ball()
    ball.x = paddle_x + PADDLE_W / 2
    ball.y = PADDLE_Y - BALL_R - 1
    ball.vx = 0
    ball.vy = 0
end

local function launch_ball()
    local angle = engine.random.float(-0.4, 0.4)
    ball.vx = BALL_SPEED * math.sin(angle)
    ball.vy = -BALL_SPEED * math.cos(angle)
end

-- ═══════════════════════════════════════════════════════════════════════════
-- State Machine
-- ═══════════════════════════════════════════════════════════════════════════

engine.state.on_enter("serve", function()
    reset_paddle()
    attach_ball()
end)

engine.state.on_enter("play", function()
    launch_ball()
end)

local function start_game()
    score = 0
    lives = 3
    reset_bricks()
    particles = {}
    engine.state.switch("serve")
end

engine.state.switch("title")

-- ═══════════════════════════════════════════════════════════════════════════
-- Particles
-- ═══════════════════════════════════════════════════════════════════════════

local function spawn_particles(bx, by, color)
    for i = 1, 6 do
        local p = {
            x = bx,
            y = by,
            vx = engine.random.float(-40, 40),
            vy = engine.random.float(-50, 10),
            life = engine.random.float(0.25, 0.7),
            color = color,
        }
        particles[#particles + 1] = p
    end
end

local function update_particles(dt)
    local i = 1
    while i <= #particles do
        local p = particles[i]
        p.x = p.x + p.vx * dt
        p.y = p.y + p.vy * dt
        p.vy = p.vy + 120 * dt  -- gravity
        p.life = p.life - dt
        if p.life <= 0 then
            particles[i] = particles[#particles]
            particles[#particles] = nil
        else
            i = i + 1
        end
    end
end

-- ═══════════════════════════════════════════════════════════════════════════
-- Helpers
-- ═══════════════════════════════════════════════════════════════════════════

local function brick_rect(row, col)
    local bx = BRICK_LEFT + (col - 1) * (BRICK_W + BRICK_GAP_X)
    local by = BRICK_TOP + (row - 1) * (BRICK_H + BRICK_GAP_Y)
    return bx, by, BRICK_W, BRICK_H
end

local function clamp(v, lo, hi)
    if v < lo then return lo end
    if v > hi then return hi end
    return v
end

-- ═══════════════════════════════════════════════════════════════════════════
-- UPDATE
-- ═══════════════════════════════════════════════════════════════════════════

function update(dt)
    if dt > 0.05 then dt = 0.05 end
    local cur = engine.state.current()

    -- TITLE
    if cur == "title" then
        if engine.input.just_pressed(BTN.A) or engine.input.just_pressed(BTN.START) then
            start_game()
        end
        return
    end

    -- GAME OVER / WIN
    if cur == "gameover" or cur == "win" then
        update_particles(dt)
        if engine.input.just_pressed(BTN.A) or engine.input.just_pressed(BTN.START) then
            engine.state.switch("title")
        end
        return
    end

    -- Paddle movement
    if engine.input.held(BTN.LEFT) then
        paddle_x = paddle_x - PADDLE_SPEED * dt
    end
    if engine.input.held(BTN.RIGHT) then
        paddle_x = paddle_x + PADDLE_SPEED * dt
    end
    paddle_x = clamp(paddle_x, AREA_L, AREA_R - PADDLE_W)

    -- SERVE: ball sticks to paddle
    if cur == "serve" then
        attach_ball()
        if engine.input.just_pressed(BTN.A) or engine.input.just_pressed(BTN.START) then
            engine.state.switch("play")
        end
        return
    end

    -- === PLAY ===
    ball.x = ball.x + ball.vx * dt
    ball.y = ball.y + ball.vy * dt

    -- Wall collisions
    if ball.x - BALL_R < AREA_L then
        ball.x = AREA_L + BALL_R
        ball.vx = math.abs(ball.vx)
    end
    if ball.x + BALL_R > AREA_R then
        ball.x = AREA_R - BALL_R
        ball.vx = -math.abs(ball.vx)
    end
    if ball.y - BALL_R < AREA_T then
        ball.y = AREA_T + BALL_R
        ball.vy = math.abs(ball.vy)
    end

    -- Death
    if ball.y > AREA_B + 12 then
        lives = lives - 1
        if lives <= 0 then
            engine.state.switch("gameover")
        else
            engine.state.switch("serve")
        end
        return
    end

    -- Paddle collision
    if ball.vy > 0 then
        local hit = engine.collision.aabb(
            ball.x - BALL_R, ball.y - BALL_R, BALL_R * 2, BALL_R * 2,
            paddle_x, PADDLE_Y, PADDLE_W, PADDLE_H
        )
        if hit then
            ball.y = PADDLE_Y - BALL_R

            local hit_pos = (ball.x - paddle_x) / PADDLE_W  -- 0..1
            local angle = (hit_pos - 0.5) * 1.3
            local speed = math.sqrt(ball.vx * ball.vx + ball.vy * ball.vy)
            speed = math.min(speed * 1.02, BALL_SPEED * 1.6)

            ball.vx = speed * math.sin(angle)
            ball.vy = -speed * math.cos(angle)
            if ball.vy > -20 then ball.vy = -20 end
        end
    end

    -- Brick collisions (one per frame for cleaner bounces)
    local hit_brick = false
    for r = 1, BRICK_ROWS do
        for c = 1, BRICK_COLS do
            if bricks[r][c] then
                local bx, by, bw, bh = brick_rect(r, c)
                local hit, ox, oy, ow, oh = engine.collision.aabbOverlap(
                    ball.x - BALL_R, ball.y - BALL_R, BALL_R * 2, BALL_R * 2,
                    bx, by, bw, bh
                )
                if hit and ow and oh then
                    bricks[r][c] = false
                    bricks_left = bricks_left - 1
                    score = score + ROW_POINTS[r]
                    spawn_particles(bx + bw / 2, by + bh / 2, ROW_COLORS[r])

                    -- Bounce
                    if ow < oh then
                        ball.vx = -ball.vx
                        if ball.x < bx + bw / 2 then
                            ball.x = bx - BALL_R
                        else
                            ball.x = bx + bw + BALL_R
                        end
                    else
                        ball.vy = -ball.vy
                        if ball.y < by + bh / 2 then
                            ball.y = by - BALL_R
                        else
                            ball.y = by + bh + BALL_R
                        end
                    end

                    if bricks_left <= 0 then
                        engine.state.switch("win")
                        return
                    end

                    hit_brick = true
                    break
                end
            end
        end
        if hit_brick then break end
    end

    update_particles(dt)
end

-- ═══════════════════════════════════════════════════════════════════════════
-- DRAW
-- ═══════════════════════════════════════════════════════════════════════════

local function draw_walls()
    setColor(COLOR.DARK_GRAY)
    rectangle(0, AREA_T, AREA_L, H - AREA_T)
    rectangle(AREA_R, AREA_T, W - AREA_R, H - AREA_T)
    rectangle(0, AREA_T - 2, W, 2)
end

local function draw_hud()
    setColor(COLOR.DARK_BLUE)
    rectangle(0, 0, W, AREA_T)

    setColor(COLOR.WHITE)
    text("SCORE:" .. score, 4, 3)

    -- Lives as balls
    for i = 1, lives do
        setColor(COLOR.RED)
        circle(W - 10 - (i - 1) * 14, 7, 3)
    end
end

local function draw_bricks()
    for r = 1, BRICK_ROWS do
        for c = 1, BRICK_COLS do
            if bricks[r][c] then
                local bx, by, bw, bh = brick_rect(r, c)
                setColor(ROW_COLORS[r])
                rectangle(bx, by, bw, bh)
                -- Top highlight
                setColor(COLOR.WHITE)
                line(bx + 1, by + 1, bx + bw - 2, by + 1)
                -- Bottom shadow
                setColor(COLOR.DARK_GRAY)
                line(bx + 1, by + bh - 1, bx + bw - 2, by + bh - 1)
            end
        end
    end
end

local function draw_paddle()
    -- Shadow
    setColor(COLOR.DARK_GRAY)
    rectangle(paddle_x + 1, PADDLE_Y + 1, PADDLE_W, PADDLE_H)
    -- Body
    setColor(COLOR.GRAY)
    rectangle(paddle_x, PADDLE_Y, PADDLE_W, PADDLE_H)
    -- Highlight
    setColor(COLOR.WHITE)
    line(paddle_x + 1, PADDLE_Y, paddle_x + PADDLE_W - 2, PADDLE_Y)
end

local function draw_ball()
    -- Small glow
    setColor(COLOR.GRAY)
    circle(ball.x, ball.y, BALL_R + 1)
    setColor(COLOR.WHITE)
    circle(ball.x, ball.y, BALL_R)
end

local function draw_particles()
    for i = 1, #particles do
        local p = particles[i]
        if p.life > 0 then
            setColor(p.color)
            if p.life > 0.3 then
                -- Larger particle when fresh
                setPixel(p.x, p.y)
                setPixel(p.x + 1, p.y)
                setPixel(p.x, p.y + 1)
            else
                setPixel(p.x, p.y)
            end
        end
    end
end

local function draw_title()
    clear(COLOR.BLACK)

    -- Starfield
    engine.random.seed(999)
    for i = 1, 40 do
        local sx = engine.random.integer(0, W - 1)
        local sy = engine.random.integer(0, H - 1)
        setColor(engine.random.integer(5, 7))
        setPixel(sx, sy)
    end
    engine.random.seed(math.floor(engine.time.now() * 100))

    -- Title text with drop shadow (scale=2 for large title)
    setColor(COLOR.DARK_RED)
    textCentered("ARKANOID", 40, 2)
    setColor(COLOR.RED)
    textCentered("ARKANOID", 39, 2)
    setColor(COLOR.ORANGE)
    textCentered("ARKANOID", 38, 2)

    -- Rainbow brick row
    local demo_colors = { COLOR.RED, COLOR.ORANGE, COLOR.YELLOW, COLOR.GREEN, COLOR.BLUE, COLOR.INDIGO,
                          COLOR.PINK, COLOR.RED, COLOR.ORANGE, COLOR.YELLOW }
    for i = 1, 10 do
        local bx = 60 + (i - 1) * 22
        setColor(demo_colors[i])
        rectangle(bx, 65, 20, 8)
        setColor(COLOR.WHITE)
        line(bx + 1, 66, bx + 18, 66)
    end

    -- Instructions
    setColor(COLOR.GRAY)
    textCentered("A/D or LEFT/RIGHT : MOVE", 110)
    textCentered("Z or ENTER        : LAUNCH", 124)

    -- Blinking prompt
    if math.floor(engine.time.now() * 2) % 2 == 0 then
        setColor(COLOR.WHITE)
        textCentered("PRESS Z TO START", 170)
    end

    setColor(COLOR.DARK_GRAY)
    textCentered("BUILT WITH ENJIN LUA", 220)
end

local function draw_dialog()
    setColor(COLOR.BLACK)
    rectangle(60, 80, 200, 80)
    setColor(COLOR.DARK_GRAY)
    rectangle(62, 82, 196, 76)
    setColor(COLOR.BLACK)
    rectangle(64, 84, 192, 72)
end

local function draw_gameover()
    draw_dialog()

    setColor(COLOR.RED)
    textCentered("GAME OVER", 95)

    setColor(COLOR.WHITE)
    textCentered("FINAL SCORE: " .. score, 118)

    if math.floor(engine.time.now() * 2) % 2 == 0 then
        setColor(COLOR.GRAY)
        textCentered("PRESS Z", 140)
    end
end

local function draw_win()
    draw_dialog()

    setColor(COLOR.YELLOW)
    textCentered("YOU WIN!", 95)

    setColor(COLOR.WHITE)
    textCentered("FINAL SCORE: " .. score, 118)

    if math.floor(engine.time.now() * 2) % 2 == 0 then
        setColor(COLOR.GRAY)
        textCentered("PRESS Z", 140)
    end
end

function draw()
    local cur = engine.state.current()

    if cur == "title" then
        draw_title()
        return
    end

    clear(COLOR.BLACK)
    draw_walls()
    draw_bricks()
    draw_paddle()
    draw_particles()

    if cur ~= "gameover" and cur ~= "win" then
        draw_ball()
    end

    draw_hud()

    if cur == "serve" then
        draw_ball()
        if math.floor(engine.time.now() * 3) % 2 == 0 then
            setColor(COLOR.YELLOW)
            textCentered("PRESS Z TO LAUNCH", H / 2 + 20)
        end
    elseif cur == "gameover" then
        draw_gameover()
    elseif cur == "win" then
        draw_win()
    end
end
